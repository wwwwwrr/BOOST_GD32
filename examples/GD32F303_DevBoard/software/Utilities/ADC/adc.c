/*!
    \file    adc.c
    \brief   ADC0 监测量与 ADC1 三相电流采样驱动

    \version 2026-7-13, V2.1.0, dual ADC sampling for GD32F30x
*/

#include "adc.h"
#include "interrupt_priority.h"
#include "systick.h"
#include <stddef.h>

#define ADC_CLOCK_PRESCALER               RCU_CKADC_CKAPB2_DIV4

#define ADC0_DMA_PERIPH                   DMA0
#define ADC0_DMA_CHANNEL                  DMA_CH0
#define ADC0_DMA_RCU                      RCU_DMA0
#define ADC0_DMA_TRANSFER_COUNT           (ADC0_DMA_FRAME_COUNT * \
                                           ADC0_MONITOR_CHANNEL_COUNT)
#define ADC0_SNAPSHOT_BUFFER_COUNT        2U

#define ADC0_OUTPUT_CURRENT_CHANNEL       ADC_CHANNEL_3
#define ADC0_OUTPUT_VOLTAGE_CHANNEL       ADC_CHANNEL_4
#define ADC0_INPUT_VOLTAGE_CHANNEL        ADC_CHANNEL_5
#define ADC0_SAMPLE_TIME                  ADC_SAMPLETIME_71POINT5
#define ADC0_OVERSAMPLING_MODE            ADC_OVERSAMPLING_ALL_CONVERT
#define ADC0_OVERSAMPLING_SHIFT           ADC_OVERSAMPLING_SHIFT_3B
#define ADC0_OVERSAMPLING_RATIO           ADC_OVERSAMPLING_RATIO_MUL8

#define ADC1_PHASE_CHANNEL_COUNT          3U
#define ADC1_PUBLISHED_BUFFER_COUNT       2U
#define ADC1_PHASE_A_CHANNEL              ADC_CHANNEL_0
#define ADC1_PHASE_B_CHANNEL              ADC_CHANNEL_1
#define ADC1_PHASE_C_CHANNEL              ADC_CHANNEL_2
#define ADC1_SAMPLE_TIME                  ADC_SAMPLETIME_7POINT5
#define ADC1_ALL_PHASES_MASK              0x07U

#define ADC_TRIGGER_TIMER                 TIMER0
#define ADC_TRIGGER_TIMER_RCU             RCU_TIMER0
#define ADC_TRIGGER_TIMER_CLOCK_HZ        120000000U
#define ADC_TRIGGER_FREQUENCY_HZ          300000U
#define ADC_TRIGGER_TIMER_PRESCALER       0U
#define ADC_TRIGGER_PERIOD_COUNTS         (ADC_TRIGGER_TIMER_CLOCK_HZ / \
                                           ADC_TRIGGER_FREQUENCY_HZ)
#define ADC_TRIGGER_TIMER_PERIOD          (ADC_TRIGGER_PERIOD_COUNTS - 1U)
#define ADC_TRIGGER_COMPARE_VALUE         ADC_TRIGGER_TIMER_PERIOD

#if ((ADC0_DMA_FRAME_COUNT % 2U) != 0U)
#error "ADC0 DMA frame count must be even"
#endif

#if (ADC0_DMA_HALF_FRAME_COUNT != 1U)
#error "ADC0 DMA half transfer must contain exactly one frame"
#endif

#if ((ADC_TRIGGER_TIMER_CLOCK_HZ % ADC_TRIGGER_FREQUENCY_HZ) != 0U)
#error "TIMER0 clock must be an integer multiple of the ADC trigger frequency"
#endif

#if (ADC_TRIGGER_PERIOD_COUNTS > 65536U)
#error "TIMER0 period does not fit the 16-bit counter"
#endif

/* ADC0 DMA 直接写入的两帧三通道循环缓冲，通道顺序为 PA3、PA4、PA5。 */
static volatile uint16_t adc0_dma_buffer[ADC0_DMA_FRAME_COUNT]
                                                [ADC0_MONITOR_CHANNEL_COUNT];

/* DMA 中断复制完整硬件平均帧后，供上层读取的 ADC0 双快照缓冲。 */
static volatile adc0_frame_raw_t
    adc0_snapshot_buffer[ADC0_SNAPSHOT_BUFFER_COUNT];
/*
 * ADC0 快照发布状态，不是 DMA 缓冲区：低1位是当前快照索引，
 * 高31位是完整快照发布序号。
 */
static volatile uint32_t adc0_published_state = 0U;
/* ADC0 DMA 传输错误锁存标志：1 表示发生过错误，0 表示正常。 */
static volatile uint8_t adc0_dma_error = 0U;

/* ADC1 尚未组成完整三相组的工作缓冲，每次 TIMER0_CH0 事件写入一相。 */
static volatile uint16_t adc1_working_raw[ADC1_PHASE_CHANNEL_COUNT];
/* ADC1 完整 A/B/C 三相采样组的双发布缓冲。 */
static volatile uint16_t adc1_published_raw[ADC1_PUBLISHED_BUFFER_COUNT]
                                                  [ADC1_PHASE_CHANNEL_COUNT];
/* ADC1 发布状态：低1位是发布缓冲索引，高31位是完整三相组序号。 */
static volatile uint32_t adc1_published_state = 0U;
/* 下一次 ADC1 EOC 转换结果对应的相位下标，依次为 A、B、C。 */
static volatile uint8_t adc1_next_phase = 0U;
/* ADC1 工作缓冲中 A/B/C 三相是否已更新的位掩码。 */
static volatile uint8_t adc1_fresh_phase_mask = 0U;
/* 上层最近一次成功读取的 ADC1 完整三相组序号。 */
static uint32_t adc1_last_read_sequence = 0U;

/* ADC 底层初始化完成标志：1 表示已初始化，0 表示未初始化。 */
static volatile uint8_t adc_initialized = 0U;
/* 双 ADC 采集运行标志：1 表示正在采集，0 表示已停止。 */
static volatile uint8_t adc_running = 0U;

static void ADC_GPIO_Config(void);
static void ADC_DMA_Config(void);
static void ADC_Peripheral_Config(void);
static void ADC_TriggerTimer_Config(void);
static void ADC_ResetRuntimeState(void);
static void ADC_EnableAndCalibrate(void);
static void ADC0_CopyCompletedFrame(uint32_t source_frame_offset);

/*!
    \brief      初始化 ADC0、ADC1、DMA 和 ADC1 触发定时器
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void ADC_Init(void)
{
    adc_initialized = 0U;
    adc_running = 0U;

    ADC_GPIO_Config();
    ADC_DMA_Config();
    ADC_TriggerTimer_Config();
    ADC_Peripheral_Config();
    ADC_ResetRuntimeState();

    NVIC_CONFIG(ADC0_1_IRQn,
                ADC0_1_PRIORITY_GROUP,
                ADC0_1_PRIORITY_SUBGROUP);
    NVIC_CONFIG(DMA0_Channel0_IRQn,
                ADC0_DMA_PRIORITY_GROUP,
                ADC0_DMA_PRIORITY_SUBGROUP);

    adc_initialized = 1U;
}

/*!
    \brief      启动 ADC0 连续采集、DMA 和 ADC1 硬件触发采集
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void ADC_Start(void)
{
    if ((adc_initialized == 0U) || (adc_running != 0U)) {
        return;
    }

    timer_disable(ADC_TRIGGER_TIMER);
    timer_counter_value_config(ADC_TRIGGER_TIMER, 0U);

    dma_channel_disable(ADC0_DMA_PERIPH, ADC0_DMA_CHANNEL);
    dma_memory_address_config(ADC0_DMA_PERIPH,
                              ADC0_DMA_CHANNEL,
                              (uint32_t)adc0_dma_buffer);
    dma_transfer_number_config(ADC0_DMA_PERIPH,
                               ADC0_DMA_CHANNEL,
                               ADC0_DMA_TRANSFER_COUNT);
    dma_interrupt_flag_clear(ADC0_DMA_PERIPH,
                             ADC0_DMA_CHANNEL,
                             DMA_INT_FLAG_G);

    ADC_ResetRuntimeState();
    ADC_EnableAndCalibrate();

    adc_interrupt_flag_clear(ADC1, ADC_INT_FLAG_EOC);
    adc_interrupt_enable(ADC1, ADC_INT_EOC);
    adc_external_trigger_config(ADC1, ADC_ROUTINE_CHANNEL, ENABLE);

    adc_dma_mode_enable(ADC0);
    /* ADC1 虽无 DMA 映射，但扫描模式仍要求置位 DMA 位。 */
    adc_dma_mode_enable(ADC1);
    dma_channel_enable(ADC0_DMA_PERIPH, ADC0_DMA_CHANNEL);

    adc_external_trigger_config(ADC0, ADC_ROUTINE_CHANNEL, ENABLE);
    adc_running = 1U;
    adc_software_trigger_enable(ADC0, ADC_ROUTINE_CHANNEL);
}

/*!
    \brief      停止 ADC0、ADC1、DMA 和 ADC1 触发定时器
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void ADC_Stop(void)
{
    if ((adc_initialized == 0U) || (adc_running == 0U)) {
        return;
    }

    timer_disable(ADC_TRIGGER_TIMER);
    adc_external_trigger_config(ADC1, ADC_ROUTINE_CHANNEL, DISABLE);
    adc_interrupt_disable(ADC1, ADC_INT_EOC);
    adc_external_trigger_config(ADC0, ADC_ROUTINE_CHANNEL, DISABLE);

    adc_dma_mode_disable(ADC0);
    adc_dma_mode_disable(ADC1);
    dma_channel_disable(ADC0_DMA_PERIPH, ADC0_DMA_CHANNEL);

    adc_disable(ADC0);
    adc_disable(ADC1);
    adc_running = 0U;
}

/*!
    \brief      获取最新完整 ADC0 硬件过采样快照的只读地址
    \param[out] snapshot: 当前完整快照的只读地址和发布序号
    \retval     ADC_STATUS_OK: 成功
    \retval     其他状态: 未初始化、数据未就绪、参数错误或 DMA 错误
*/
adc_status_t ADC0_GetLatestSnapshot(adc0_snapshot_view_t *snapshot)
{
    uint32_t published_state;              /*!< 本次读取到的 ADC0 发布状态副本 */
    uint32_t sequence;                     /*!< 从发布状态高31位提取的快照序号 */
    uint8_t snapshot_index;                /*!< 从发布状态低1位提取的快照下标 */

    if (snapshot == NULL) {
        return ADC_STATUS_INVALID_PARAMETER;
    }
    if (adc_initialized == 0U) {
        return ADC_STATUS_NOT_INITIALIZED;
    }
    if (adc0_dma_error != 0U) {
        return ADC_STATUS_DMA_ERROR;
    }

    published_state = adc0_published_state;
    sequence = published_state >> 1U;
    if (sequence == 0U) {
        return ADC_STATUS_NOT_READY;
    }

    snapshot_index = (uint8_t)(published_state & 1U);
    __DMB();
    snapshot->frame = &adc0_snapshot_buffer[snapshot_index];
    snapshot->sequence = sequence;
    return ADC_STATUS_OK;
}

/*!
    \brief      获取 ADC0 最近一次完整快照的发布序号
    \param[in]  无
    \param[out] 无
    \retval     ADC0 快照发布序号，0 表示尚无有效快照
*/
uint32_t ADC0_GetSnapshotSequence(void)
{
    return adc0_published_state >> 1U;
}

/*!
    \brief      读取 ADC1 最新完整的 A、B、C 三相 raw 数据
    \param[out] result: 三相 raw 数据和对应发布序号
    \param[out] new_data: 新三相组标志，1 表示有新数据，0 表示无
    \retval     ADC_STATUS_OK: 成功
    \retval     其他状态: 未初始化、数据未就绪或参数错误
*/
adc_status_t ADC1_GetLatestRaw(adc1_phase_raw_t *result,
                               uint8_t *new_data)
{
    uint32_t state_before;                 /*!< 复制三相数据前读取的发布状态 */
    uint32_t state_after;                  /*!< 复制三相数据后读取的发布状态 */
    uint32_t sequence_before;              /*!< 本次读取的完整三相组序号 */
    uint8_t published_index;               /*!< 当前 ADC1 发布缓冲的数组下标 */

    if ((result == NULL) || (new_data == NULL)) {
        return ADC_STATUS_INVALID_PARAMETER;
    }
    if (adc_initialized == 0U) {
        return ADC_STATUS_NOT_INITIALIZED;
    }

    do {
        state_before = adc1_published_state;
        sequence_before = state_before >> 1U;
        if (sequence_before == 0U) {
            *new_data = 0U;
            return ADC_STATUS_NOT_READY;
        }

        published_index = (uint8_t)(state_before & 1U);
        __DMB();
        result->phase_a_raw = adc1_published_raw[published_index][0];
        result->phase_b_raw = adc1_published_raw[published_index][1];
        result->phase_c_raw = adc1_published_raw[published_index][2];
        __DMB();
        state_after = adc1_published_state;
    } while (state_before != state_after);

    result->sequence = sequence_before;
    *new_data = (sequence_before != adc1_last_read_sequence) ? 1U : 0U;
    adc1_last_read_sequence = sequence_before;
    return ADC_STATUS_OK;
}

/*!
    \brief      获取 ADC1 最近一次完整三相组的发布序号
    \param[in]  无
    \param[out] 无
    \retval     ADC1 三相组发布序号，0 表示尚无有效三相组
*/
uint32_t ADC1_GetSequence(void)
{
    return adc1_published_state >> 1U;
}

/*!
    \brief      将 PA0～PA5 配置为 ADC 模拟输入
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void ADC_GPIO_Config(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    gpio_init(GPIOA,
              GPIO_MODE_AIN,
              GPIO_OSPEED_50MHZ,
              GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 |
              GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5);
}

/*!
    \brief      配置 ADC0 使用的 DMA0_CH0 两帧循环传输
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void ADC_DMA_Config(void)
{
    dma_parameter_struct dma_init_struct;  /*!< ADC0 DMA 初始化参数结构体 */

    rcu_periph_clock_enable(ADC0_DMA_RCU);
    dma_deinit(ADC0_DMA_PERIPH, ADC0_DMA_CHANNEL);
    dma_struct_para_init(&dma_init_struct);

    dma_init_struct.periph_addr = (uint32_t)&ADC_RDATA(ADC0);
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_addr = (uint32_t)adc0_dma_buffer;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_16BIT;
    dma_init_struct.memory_width = DMA_MEMORY_WIDTH_16BIT;
    dma_init_struct.direction = DMA_PERIPHERAL_TO_MEMORY;
    dma_init_struct.number = ADC0_DMA_TRANSFER_COUNT;
    dma_init_struct.priority = DMA_PRIORITY_HIGH;
    dma_init(ADC0_DMA_PERIPH, ADC0_DMA_CHANNEL, &dma_init_struct);

    dma_circulation_enable(ADC0_DMA_PERIPH, ADC0_DMA_CHANNEL);
    dma_interrupt_enable(ADC0_DMA_PERIPH,
                         ADC0_DMA_CHANNEL,
                         DMA_INT_HTF | DMA_INT_FTF | DMA_INT_ERR);
}

/*!
    \brief      配置 ADC0 连续扫描、硬件8倍过采样和 ADC1 间断采样
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void ADC_Peripheral_Config(void)
{
    rcu_periph_clock_enable(RCU_ADC0);
    rcu_periph_clock_enable(RCU_ADC1);
    rcu_adc_clock_config(ADC_CLOCK_PRESCALER);

    adc_deinit(ADC0);
    adc_deinit(ADC1);
    adc_mode_config(ADC_MODE_FREE);

    adc_special_function_config(ADC0, ADC_SCAN_MODE, ENABLE);
    adc_special_function_config(ADC0, ADC_CONTINUOUS_MODE, ENABLE);
    adc_data_alignment_config(ADC0, ADC_DATAALIGN_RIGHT);
    adc_resolution_config(ADC0, ADC_RESOLUTION_12B);
    adc_oversample_mode_config(ADC0,
                               ADC0_OVERSAMPLING_MODE,
                               ADC0_OVERSAMPLING_SHIFT,
                               ADC0_OVERSAMPLING_RATIO);
    adc_oversample_mode_enable(ADC0);
    adc_channel_length_config(ADC0,
                              ADC_ROUTINE_CHANNEL,
                              ADC0_MONITOR_CHANNEL_COUNT);
    adc_routine_channel_config(ADC0,
                               0U,
                               ADC0_OUTPUT_CURRENT_CHANNEL,
                               ADC0_SAMPLE_TIME);
    adc_routine_channel_config(ADC0,
                               1U,
                               ADC0_OUTPUT_VOLTAGE_CHANNEL,
                               ADC0_SAMPLE_TIME);
    adc_routine_channel_config(ADC0,
                               2U,
                               ADC0_INPUT_VOLTAGE_CHANNEL,
                               ADC0_SAMPLE_TIME);
    adc_external_trigger_source_config(ADC0,
                                       ADC_ROUTINE_CHANNEL,
                                       ADC0_1_2_EXTTRIG_ROUTINE_NONE);
    adc_external_trigger_config(ADC0, ADC_ROUTINE_CHANNEL, DISABLE);

    adc_special_function_config(ADC1, ADC_SCAN_MODE, ENABLE);
    adc_special_function_config(ADC1, ADC_CONTINUOUS_MODE, DISABLE);
    adc_data_alignment_config(ADC1, ADC_DATAALIGN_RIGHT);
    adc_resolution_config(ADC1, ADC_RESOLUTION_12B);
    adc_oversample_mode_disable(ADC1);
    adc_channel_length_config(ADC1,
                              ADC_ROUTINE_CHANNEL,
                              ADC1_PHASE_CHANNEL_COUNT);
    adc_routine_channel_config(ADC1,
                               0U,
                               ADC1_PHASE_A_CHANNEL,
                               ADC1_SAMPLE_TIME);
    adc_routine_channel_config(ADC1,
                               1U,
                               ADC1_PHASE_B_CHANNEL,
                               ADC1_SAMPLE_TIME);
    adc_routine_channel_config(ADC1,
                               2U,
                               ADC1_PHASE_C_CHANNEL,
                               ADC1_SAMPLE_TIME);
    adc_discontinuous_mode_config(ADC1, ADC_ROUTINE_CHANNEL, 1U);
    adc_external_trigger_source_config(ADC1,
                                       ADC_ROUTINE_CHANNEL,
                                       ADC0_1_EXTTRIG_ROUTINE_T0_CH0);
    adc_external_trigger_config(ADC1, ADC_ROUTINE_CHANNEL, DISABLE);
    adc_interrupt_disable(ADC1, ADC_INT_EOC);
}

/*!
    \brief      配置 TIMER0 为 ADC1 的 300 kHz 硬件触发从定时器
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void ADC_TriggerTimer_Config(void)
{
    timer_parameter_struct timer_init_struct;       /*!< TIMER0 基础计数参数 */
    timer_oc_parameter_struct timer_oc_struct;      /*!< TIMER0 CH0 输出比较参数 */

    rcu_periph_clock_enable(ADC_TRIGGER_TIMER_RCU);
    timer_deinit(ADC_TRIGGER_TIMER);

    timer_struct_para_init(&timer_init_struct);
    timer_init_struct.prescaler = ADC_TRIGGER_TIMER_PRESCALER;
    timer_init_struct.alignedmode = TIMER_COUNTER_EDGE;
    timer_init_struct.counterdirection = TIMER_COUNTER_UP;
    timer_init_struct.period = ADC_TRIGGER_TIMER_PERIOD;
    timer_init_struct.clockdivision = TIMER_CKDIV_DIV1;
    timer_init_struct.repetitioncounter = 0U;
    timer_init(ADC_TRIGGER_TIMER, &timer_init_struct);

    timer_channel_output_struct_para_init(&timer_oc_struct);
    timer_oc_struct.outputstate = TIMER_CCX_DISABLE;
    timer_oc_struct.outputnstate = TIMER_CCXN_DISABLE;
    timer_oc_struct.ocpolarity = TIMER_OC_POLARITY_HIGH;
    timer_oc_struct.ocnpolarity = TIMER_OCN_POLARITY_HIGH;
    timer_oc_struct.ocidlestate = TIMER_OC_IDLE_STATE_LOW;
    timer_oc_struct.ocnidlestate = TIMER_OCN_IDLE_STATE_LOW;
    timer_channel_output_config(ADC_TRIGGER_TIMER,
                                TIMER_CH_0,
                                &timer_oc_struct);
    timer_channel_output_mode_config(ADC_TRIGGER_TIMER,
                                     TIMER_CH_0,
                                     TIMER_OC_MODE_TIMING);
    timer_channel_output_shadow_config(ADC_TRIGGER_TIMER,
                                       TIMER_CH_0,
                                       TIMER_OC_SHADOW_DISABLE);
    timer_channel_output_pulse_value_config(ADC_TRIGGER_TIMER,
                                            TIMER_CH_0,
                                            ADC_TRIGGER_COMPARE_VALUE);

    timer_auto_reload_shadow_enable(ADC_TRIGGER_TIMER);
    timer_input_trigger_source_select(ADC_TRIGGER_TIMER,
                                      TIMER_SMCFG_TRGSEL_ITI3);
    timer_slave_mode_select(ADC_TRIGGER_TIMER, TIMER_SLAVE_MODE_EVENT);
    timer_event_software_generate(ADC_TRIGGER_TIMER, TIMER_EVENT_SRC_UPG);
    timer_counter_value_config(ADC_TRIGGER_TIMER, 0U);
    timer_disable(ADC_TRIGGER_TIMER);
}

/*!
    \brief      清除 ADC0 和 ADC1 的底层快照发布状态
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void ADC_ResetRuntimeState(void)
{
    uint32_t buffer;                       /*!< ADC1 双发布缓冲循环下标 */
    uint32_t phase;                        /*!< ADC1 A/B/C 三相通道循环下标 */

    adc0_published_state = 0U;
    adc0_dma_error = 0U;

    adc1_next_phase = 0U;
    adc1_fresh_phase_mask = 0U;
    adc1_published_state = 0U;
    adc1_last_read_sequence = 0U;
    for (phase = 0U; phase < ADC1_PHASE_CHANNEL_COUNT; phase++) {
        adc1_working_raw[phase] = 0U;
        for (buffer = 0U; buffer < ADC1_PUBLISHED_BUFFER_COUNT; buffer++) {
            adc1_published_raw[buffer][phase] = 0U;
        }
    }
}

/*!
    \brief      使能并校准 ADC0 和 ADC1
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void ADC_EnableAndCalibrate(void)
{
    adc_enable(ADC0);
    adc_enable(ADC1);
    delay_1ms(1U);
    adc_calibration_enable(ADC0);
    adc_calibration_enable(ADC1);
}

/*!
    \brief      将 DMA 已完成的一组三通道数据复制并发布到非活动快照
    \param[in]  source_frame_offset: DMA 循环缓冲中的源帧索引
    \param[out] 无
    \retval     无
*/
static void ADC0_CopyCompletedFrame(uint32_t source_frame_offset)
{
    uint32_t current_state;                /*!< 复制前的 ADC0 快照发布状态 */
    uint32_t next_sequence;                /*!< 本次完整快照的新发布序号 */
    uint8_t destination_snapshot;          /*!< 目标快照数组下标，不是数据缓冲区 */

    current_state = adc0_published_state;
    destination_snapshot = (uint8_t)((current_state & 1U) ^ 1U);
    adc0_snapshot_buffer[destination_snapshot].output_current_raw =
        adc0_dma_buffer[source_frame_offset][0];
    adc0_snapshot_buffer[destination_snapshot].output_voltage_raw =
        adc0_dma_buffer[source_frame_offset][1];
    adc0_snapshot_buffer[destination_snapshot].input_voltage_raw =
        adc0_dma_buffer[source_frame_offset][2];

    __DMB();
    next_sequence = (current_state >> 1U) + 1U;
    adc0_published_state = (next_sequence << 1U) | destination_snapshot;
}

/*!
    \brief      处理 ADC0 DMA 半满、全满和传输错误中断
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void ADC_DMA_IRQHandler_Internal(void)
{
    FlagStatus half_transfer_flag;         /*!< ADC0 DMA 半传输完成标志 */
    FlagStatus full_transfer_flag;         /*!< ADC0 DMA 全传输完成标志 */

    if (dma_interrupt_flag_get(ADC0_DMA_PERIPH,
                               ADC0_DMA_CHANNEL,
                               DMA_INT_FLAG_ERR) != RESET) {
        dma_interrupt_flag_clear(ADC0_DMA_PERIPH,
                                 ADC0_DMA_CHANNEL,
                                 DMA_INT_FLAG_ERR);
        adc0_dma_error = 1U;
    }

    half_transfer_flag = dma_interrupt_flag_get(ADC0_DMA_PERIPH,
                                                 ADC0_DMA_CHANNEL,
                                                 DMA_INT_FLAG_HTF);
    full_transfer_flag = dma_interrupt_flag_get(ADC0_DMA_PERIPH,
                                                 ADC0_DMA_CHANNEL,
                                                 DMA_INT_FLAG_FTF);

    /* 两个标志同时积压时，只有后半帧仍能保证未被循环 DMA 覆盖。 */
    if (full_transfer_flag != RESET) {
        dma_interrupt_flag_clear(ADC0_DMA_PERIPH,
                                 ADC0_DMA_CHANNEL,
                                 DMA_INT_FLAG_FTF);
        if (half_transfer_flag != RESET) {
            dma_interrupt_flag_clear(ADC0_DMA_PERIPH,
                                     ADC0_DMA_CHANNEL,
                                     DMA_INT_FLAG_HTF);
        }
        ADC0_CopyCompletedFrame(ADC0_DMA_HALF_FRAME_COUNT);
    } else if (half_transfer_flag != RESET) {
        dma_interrupt_flag_clear(ADC0_DMA_PERIPH,
                                 ADC0_DMA_CHANNEL,
                                 DMA_INT_FLAG_HTF);
        ADC0_CopyCompletedFrame(0U);
    }
}

/*!
    \brief      处理 ADC1 单通道转换完成中断并发布完整三相组
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void ADC1_IRQHandler_Internal(void)
{
    uint32_t current_state;                /*!< 本次发布前的 ADC1 发布状态 */
    uint32_t next_sequence;                /*!< 本次完整三相组的新发布序号 */
    uint16_t conversion_value;             /*!< 当前 EOC 对应的 ADC1 转换结果 */
    uint8_t completed_phase;               /*!< 当前结果对应及下一次要处理的相位下标 */
    uint8_t destination_buffer;            /*!< ADC1 目标发布缓冲数组下标 */

    if (adc_interrupt_flag_get(ADC1, ADC_INT_FLAG_EOC) == RESET) {
        return;
    }

    conversion_value = adc_routine_data_read(ADC1);
    adc_interrupt_flag_clear(ADC1, ADC_INT_FLAG_EOC);

    completed_phase = adc1_next_phase;
    adc1_working_raw[completed_phase] = conversion_value;
    adc1_fresh_phase_mask |= (uint8_t)(1U << completed_phase);

    completed_phase++;
    if (completed_phase >= ADC1_PHASE_CHANNEL_COUNT) {
        completed_phase = 0U;
        if (adc1_fresh_phase_mask == ADC1_ALL_PHASES_MASK) {
            current_state = adc1_published_state;
            destination_buffer = (uint8_t)((current_state & 1U) ^ 1U);
            adc1_published_raw[destination_buffer][0] = adc1_working_raw[0];
            adc1_published_raw[destination_buffer][1] = adc1_working_raw[1];
            adc1_published_raw[destination_buffer][2] = adc1_working_raw[2];
            adc1_fresh_phase_mask = 0U;
            __DMB();
            next_sequence = (current_state >> 1U) + 1U;
            adc1_published_state = (next_sequence << 1U) |
                                   destination_buffer;
        }
    }
    adc1_next_phase = completed_phase;
}
