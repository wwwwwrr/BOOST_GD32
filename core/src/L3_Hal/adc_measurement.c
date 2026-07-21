#include "adc_measurement.h"
#include "project_config.h"
#include "adc.h"
#include <stddef.h>

static float ADCMeasurement_RawToAdcVoltage(uint16_t raw);
static float ADCMeasurement_RawToPhaseCurrent(uint16_t raw,
                                               uint16_t offset_raw);
static void ADCMeasurement_ResetCaches(void);

/* 最近一次成功换算并提交的 ADC0 监测实际值缓存。 */
static adc_monitor_value_t adc0_latest_value;
/* 最近一次成功处理的 ADC0 完整快照发布序号。 */
static uint32_t adc0_last_processed_sequence = 0U;
/* 最近一次成功换算的 ADC1 三相实际电流缓存。 */
static adc_phase_current_value_t adc1_latest_value;
/* 上电前置校准得到的 ADC1 A/B/C 三相零电流偏置。 */
static adc_phase_offset_value_t adc1_phase_offset;
/* 三相偏置有效标志：1 表示三相已全部校准，0 表示不可用于换算。 */
static uint8_t adc1_phase_offset_ready = 0U;

/*!
    \brief      使用独立 ADC1 轮询流程校准三相零电流偏置
    \param[in]  无
    \param[out] 无
    \retval     1: 三相偏置已校准并保存
    \retval     0: 校准失败
*/
uint8_t ADCMeasurement_CalibratePhaseOffsets(void)
{
    adc1_phase_raw_t raw_offset;            /*!< BSP 返回的三相偏置平均 raw */

    adc1_phase_offset_ready = 0U;
    adc1_phase_offset.phase_a_offset_raw = 0U;
    adc1_phase_offset.phase_b_offset_raw = 0U;
    adc1_phase_offset.phase_c_offset_raw = 0U;

    if (ADC1_CalibratePhaseOffsets(&raw_offset) == 0U) {
        return 0U;
    }

    adc1_phase_offset.phase_a_offset_raw = raw_offset.phase_a_raw;
    adc1_phase_offset.phase_b_offset_raw = raw_offset.phase_b_raw;
    adc1_phase_offset.phase_c_offset_raw = raw_offset.phase_c_raw;
    adc1_phase_offset_ready = 1U;
    return 1U;
}

/*!
    \brief      查询三相零电流偏置是否已经成功校准
    \param[in]  无
    \param[out] 无
    \retval     1: 已校准
    \retval     0: 未校准或校准失败
*/
uint8_t ADCMeasurement_IsPhaseOffsetReady(void)
{
    return adc1_phase_offset_ready;
}

/*!
    \brief      复制上电校准得到的三相偏置 raw
    \param[out] result: 三相偏置 raw
    \retval     1: 复制成功
    \retval     0: 参数为空或偏置无效
*/
uint8_t ADCMeasurement_GetPhaseOffsets(adc_phase_offset_value_t *result)
{
    if ((result == NULL) || (adc1_phase_offset_ready == 0U)) {
        return 0U;
    }

    *result = adc1_phase_offset;
    return 1U;
}

/*!
    \brief      初始化 ADC 实际值测量模块
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void ADCMeasurement_Init(void)
{
    ADCMeasurement_ResetCaches();
    ADC_Init();
}

/*!
    \brief      启动底层双 ADC 采集
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void ADCMeasurement_Start(void)
{
    ADCMeasurement_ResetCaches();
    ADC_Start();
}

/*!
    \brief      停止底层双 ADC 采集
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void ADCMeasurement_Stop(void)
{
    ADC_Stop();
}

/*!
    \brief      换算 ADC0 最新硬件8倍平均 raw 并返回缓存值
    \param[out] result: 输出电流、输出电压、输入电压及发布序号
    \retval     无
*/
void ADCMeasurement_ProcessMonitor(adc_monitor_value_t *result)
{
    adc0_frame_raw_t raw;                   /*!< 底层复制的最新 ADC0 raw */
    adc_monitor_value_t next_value;         /*!< 本次换算完成但尚未提交的实际值 */
    uint32_t sequence;                      /*!< 底层 ADC0 完整数据发布序号 */
    float adc_voltage;                      /*!< 当前 raw 对应的 ADC 引脚电压 */

    if (result == NULL) {
        return;
    }

    if ((ADC0_GetLatestRaw(&raw, &sequence) != 0U) &&
        (sequence != adc0_last_processed_sequence)) {
        adc_voltage = ADCMeasurement_RawToAdcVoltage(
            raw.output_current_raw);
        next_value.output_current_a = adc_voltage * BSP_ADC_IOUT_GAIN;

        adc_voltage = ADCMeasurement_RawToAdcVoltage(
            raw.output_voltage_raw);
        next_value.output_voltage_v = adc_voltage * BSP_ADC_UOUT_GAIN;

        adc_voltage = ADCMeasurement_RawToAdcVoltage(
            raw.input_voltage_raw);
        next_value.input_voltage_v = adc_voltage * BSP_ADC_POW_GAIN;
        next_value.sequence = sequence;

        adc0_latest_value = next_value;
        adc0_last_processed_sequence = sequence;
    }

    *result = adc0_latest_value;
}

/*!
    \brief      换算 ADC1 的 A/B/C 各相最新 raw 并返回缓存值
    \param[out] result: 三相电流
    \retval     无
*/
void ADCMeasurement_GetPhaseCurrents(adc_phase_current_value_t *result)
{
    adc1_phase_raw_t raw;                   /*!< ADC1 A/B/C 各相最新 raw */
    adc_phase_current_value_t next_value;   /*!< 本次换算完成但尚未提交的三相值 */

    if (result == NULL) {
        return;
    }

    if ((adc1_phase_offset_ready != 0U) &&
        (ADC1_GetLatestRaw(&raw) != 0U)) {
        next_value.phase_a_current_a =
            ADCMeasurement_RawToPhaseCurrent(
                raw.phase_a_raw,
                adc1_phase_offset.phase_a_offset_raw);
        next_value.phase_b_current_a =
            ADCMeasurement_RawToPhaseCurrent(
                raw.phase_b_raw,
                adc1_phase_offset.phase_b_offset_raw);
        next_value.phase_c_current_a =
            ADCMeasurement_RawToPhaseCurrent(
                raw.phase_c_raw,
                adc1_phase_offset.phase_c_offset_raw);

        adc1_latest_value = next_value;
    }

    *result = adc1_latest_value;
}

/*!
    \brief      将 12 位 ADC raw 换算为 ADC 引脚电压
    \param[in]  raw: ADC 原始计数，范围为 0～4095
    \param[out] 无
    \retval     ADC 引脚电压，单位 V
*/
static float ADCMeasurement_RawToAdcVoltage(uint16_t raw)
{
    return ((float)raw * BSP_ADC_REF_VOLTAGE) / BSP_ADC_FULL_SCALE;
}

/*!
    \brief      将相电流 ADC raw 换算为带方向的实际电流
    \param[in]  raw: 相电流 ADC 原始计数，范围为 0～4095
    \param[in]  offset_raw: 当前相位上电校准得到的零电流偏置 raw
    \param[out] 无
    \retval     相电流，单位 A，允许返回负值
*/
static float ADCMeasurement_RawToPhaseCurrent(uint16_t raw,
                                               uint16_t offset_raw)
{
    int32_t raw_delta;                      /*!< 当前 raw 相对本相偏置的有符号差值 */

    raw_delta = (int32_t)raw - (int32_t)offset_raw;
    return (((float)raw_delta * BSP_ADC_REF_VOLTAGE) /
            BSP_ADC_FULL_SCALE) * BSP_ADC_PHASE_GAIN;
}

/*!
    \brief      清除 ADC0 和 ADC1 的最后有效换算值及 ADC0 已处理序号
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void ADCMeasurement_ResetCaches(void)
{
    adc0_latest_value.output_current_a = 0.0f;
    adc0_latest_value.output_voltage_v = 0.0f;
    adc0_latest_value.input_voltage_v = 0.0f;
    adc0_latest_value.sequence = 0U;
    adc0_last_processed_sequence = 0U;

    adc1_latest_value.phase_a_current_a = 0.0f;
    adc1_latest_value.phase_b_current_a = 0.0f;
    adc1_latest_value.phase_c_current_a = 0.0f;
}
