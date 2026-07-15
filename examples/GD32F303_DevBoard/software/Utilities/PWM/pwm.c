/**
 * \file pwm.c
 * \brief 三相 120 度交错、六路逻辑互补 PWM 底层实现。
 *
 * TIMER3 作主定时器同步启动 TIMER2/TIMER1，三相使用 0/120/240 度
 * 计数初值。每相 CH0 为正向 PWM0，CH1 为共用 CCR 的逻辑互补 PWM1；
 * 普通定时器不产生硬件死区。CCR/ARR 通过影子寄存器更新，PWM_Stop()
 * 关闭三台定时器后将六路 GPIO 强制为低电平。
 */

#include "pwm.h"

#define PWM_GPIOB_OUTPUT_PINS           (GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | \
                                         GPIO_PIN_6 | GPIO_PIN_7) /*!< GPIOB 上全部 PWM 输出引脚掩码。 */

static float current_duty[PWM_PHASE_COUNT] = { /*!< A/B/C 三相最近一次量化后的实际占空比，单位 %。 */
    PWM_DEFAULT_DUTY_A,
    PWM_DEFAULT_DUTY_B,
    PWM_DEFAULT_DUTY_C
};
static uint8_t pwm_initialized = 0U; /*!< PWM 初始化标志：1 表示硬件已配置，0 表示未配置。 */
static uint8_t pwm_running = 0U; /*!< PWM 运行标志：1 表示同步链已启动，0 表示已停止。 */

/*!
    \brief      将六路 PWM 引脚配置为推挽输出并强制为低电平
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void PWM_GPIO_ConfigureSafeLow(void);

/*!
    \brief      配置 PWM 引脚复用并保留 SWD 调试接口
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void PWM_GPIO_ConfigureAlternateFunction(void);

/*!
    \brief      配置一台固定 100 kHz 的双通道逻辑互补 PWM 定时器
    \param[in]  timer_periph: TIMER1、TIMER2 或 TIMER3 外设基地址
    \param[in]  compare_value: CH0/CH1 共用的比较计数值
    \param[out] 无
    \retval     无
*/
static void PWM_Timer_Configure(uint32_t timer_periph, uint16_t compare_value);

/*!
    \brief      同时配置三相六个定时器通道的输出使能状态
    \param[in]  state: TIMER_CCX_ENABLE 或 TIMER_CCX_DISABLE
    \param[out] 无
    \retval     无
*/
static void PWM_ChannelStateConfigure(uint32_t state);

/*!
    \brief      触发三相影子寄存器装载并恢复 0/120/240 度计数初值
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void PWM_ReloadSynchronizationState(void);

/*!
    \brief      将逻辑相位映射到对应的定时器外设
    \param[in]  phase: A、B 或 C 相编号
    \param[out] 无
    \return     对应 TIMER3、TIMER2 或 TIMER1 外设基地址
*/
static uint32_t PWM_TimerFromPhase(pwm_phase_t phase);

/*!
    \brief      将百分比占空比换算为定时器比较计数值
    \param[in]  duty_percent: 请求占空比，单位 %
    \param[out] 无
    \return     直接截断量化后的 CCR 比较计数值
*/
static uint16_t PWM_CalculateCompareValue(float duty_percent);

/*!
    \brief      将比较计数值换算为实际量化占空比
    \param[in]  compare_value: CCR 比较计数值
    \param[out] 无
    \return     实际量化占空比，单位 %
*/
static float PWM_CalculateActualDuty(uint16_t compare_value);

/*!
    \brief      初始化固定 100 kHz 三相 PWM 硬件
    \param[in]  无
    \param[out] 无
    \retval     无
    \note       初始化完成后六路通道仍保持关闭，GPIO 被强制为低电平
*/
void PWM_Init(void)
{
    uint16_t compare_a; /*!< A 相默认占空比对应的 CCR 比较计数值。 */
    uint16_t compare_b; /*!< B 相默认占空比对应的 CCR 比较计数值。 */
    uint16_t compare_c; /*!< C 相默认占空比对应的 CCR 比较计数值。 */

    pwm_initialized = 0U;
    pwm_running = 0U;

    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_AF);
    rcu_periph_clock_enable(PWM_PHASE_A_TIMER_RCU);
    rcu_periph_clock_enable(PWM_PHASE_B_TIMER_RCU);
    rcu_periph_clock_enable(PWM_PHASE_C_TIMER_RCU);

    PWM_GPIO_ConfigureSafeLow();

    timer_deinit(PWM_PHASE_A_TIMER);
    timer_deinit(PWM_PHASE_B_TIMER);
    timer_deinit(PWM_PHASE_C_TIMER);

    compare_a = PWM_CalculateCompareValue(PWM_DEFAULT_DUTY_A);
    compare_b = PWM_CalculateCompareValue(PWM_DEFAULT_DUTY_B);
    compare_c = PWM_CalculateCompareValue(PWM_DEFAULT_DUTY_C);

    PWM_Timer_Configure(PWM_PHASE_A_TIMER, compare_a);
    PWM_Timer_Configure(PWM_PHASE_B_TIMER, compare_b);
    PWM_Timer_Configure(PWM_PHASE_C_TIMER, compare_c);

    /* TIMER3 的 CEN 输出为 TRGO，TIMER1/TIMER2 均通过 ITI3 接收该事件。 */
    timer_master_output_trigger_source_select(PWM_PHASE_A_TIMER,
                                               TIMER_TRI_OUT_SRC_ENABLE);
    timer_master_slave_mode_config(PWM_PHASE_A_TIMER,
                                   TIMER_MASTER_SLAVE_MODE_ENABLE);

    timer_input_trigger_source_select(PWM_PHASE_B_TIMER,
                                      TIMER_SMCFG_TRGSEL_ITI3);
    timer_slave_mode_select(PWM_PHASE_B_TIMER, TIMER_SLAVE_MODE_EVENT);
    timer_input_trigger_source_select(PWM_PHASE_C_TIMER,
                                      TIMER_SMCFG_TRGSEL_ITI3);
    timer_slave_mode_select(PWM_PHASE_C_TIMER, TIMER_SLAVE_MODE_EVENT);

    PWM_ChannelStateConfigure(TIMER_CCX_DISABLE);
    PWM_ReloadSynchronizationState();
    pwm_initialized = 1U;
}

/*!
    \brief      重装三相计数器并启动同步 PWM 链
    \param[in]  无
    \param[out] 无
    \retval     无
    \note       仅由软件启动 TIMER3 主定时器，TIMER2/TIMER1 由 TRGO 同步启动
*/
void PWM_Start(void)
{
    if ((pwm_initialized == 0U) || (pwm_running != 0U)) {
        return;
    }

    PWM_ChannelStateConfigure(TIMER_CCX_DISABLE);
    timer_disable(PWM_PHASE_A_TIMER);
    timer_disable(PWM_PHASE_B_TIMER);
    timer_disable(PWM_PHASE_C_TIMER);

    /* UPG 在装载相位计数初值前，将 ARR/CCR 影子值同步到工作寄存器。 */
    PWM_ReloadSynchronizationState();
    PWM_GPIO_ConfigureAlternateFunction();

    /* 软件只启动 TIMER3 主定时器，TRGO 随后启动两个 Event 模式从定时器。 */
    timer_enable(PWM_PHASE_A_TIMER);
    PWM_ChannelStateConfigure(TIMER_CCX_ENABLE);
    pwm_running = 1U;
}

/*!
    \brief      停止全部 PWM 定时器并将六路输出引脚强制为低电平
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void PWM_Stop(void)
{
    if (pwm_initialized == 0U) {
        return;
    }

    PWM_ChannelStateConfigure(TIMER_CCX_DISABLE);
    timer_disable(PWM_PHASE_A_TIMER);
    timer_disable(PWM_PHASE_B_TIMER);
    timer_disable(PWM_PHASE_C_TIMER);
    PWM_GPIO_ConfigureSafeLow();
    pwm_running = 0U;
}

/*!
    \brief      设置一相正向 PWM 的请求占空比
    \param[in]  phase: PWM_PHASE_A、PWM_PHASE_B 或 PWM_PHASE_C
    \param[in]  duty_percent: 请求占空比，单位 %，底层限制到 0～100
    \param[out] 无
    \retval     无
*/
void PWM_SetDutyCycle(pwm_phase_t phase, float duty_percent)
{
    uint32_t timer_periph; /*!< 当前相位对应的定时器外设基地址。 */
    uint16_t compare_value; /*!< 请求占空比对应的 CCR 比较计数值。 */

    if ((pwm_initialized == 0U) ||
        ((uint32_t)phase >= (uint32_t)PWM_PHASE_COUNT)) {
        return;
    }

    /* 第一个条件同时把 NaN 和负无穷映射到 0%。 */
    if (!(duty_percent >= 0.0f)) {
        duty_percent = 0.0f;
    } else if (duty_percent > 100.0f) {
        duty_percent = 100.0f;
    }

    timer_periph = PWM_TimerFromPhase(phase);
    compare_value = PWM_CalculateCompareValue(duty_percent);
    current_duty[phase] = PWM_CalculateActualDuty(compare_value);

    /* 两次 CCR 写入期间禁止更新事件，避免溢出只装载其中一个通道。 */
    timer_update_event_disable(timer_periph);
    timer_channel_output_pulse_value_config(timer_periph, TIMER_CH_0,
                                            compare_value);
    timer_channel_output_pulse_value_config(timer_periph, TIMER_CH_1,
                                            compare_value);
    timer_update_event_enable(timer_periph);
}

/*!
    \brief      批量设置 A/B/C 三相正向 PWM 请求占空比
    \param[in]  duty_a: A 相请求占空比，单位 %
    \param[in]  duty_b: B 相请求占空比，单位 %
    \param[in]  duty_c: C 相请求占空比，单位 %
    \param[out] 无
    \retval     无
*/
void PWM_SetThreePhaseDuty(float duty_a, float duty_b, float duty_c)
{
    PWM_SetDutyCycle(PWM_PHASE_A, duty_a);
    PWM_SetDutyCycle(PWM_PHASE_B, duty_b);
    PWM_SetDutyCycle(PWM_PHASE_C, duty_c);
}

/*!
    \brief      读取一相最近一次量化后的实际占空比
    \param[in]  phase: PWM_PHASE_A、PWM_PHASE_B 或 PWM_PHASE_C
    \param[out] 无
    \return     实际量化占空比，单位 %；相位无效时返回 0
*/
float PWM_GetDutyCycle(pwm_phase_t phase)
{
    if ((uint32_t)phase >= (uint32_t)PWM_PHASE_COUNT) {
        return 0.0f;
    }

    return current_duty[phase];
}

/*!
    \brief      将六路 PWM 引脚配置为推挽输出并强制为低电平
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void PWM_GPIO_ConfigureSafeLow(void)
{
    gpio_init(GPIOA, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ,
              PWM_PHASE_C_POSITIVE_PIN);
    gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ,
              PWM_GPIOB_OUTPUT_PINS);
    gpio_bit_reset(GPIOA, PWM_PHASE_C_POSITIVE_PIN);
    gpio_bit_reset(GPIOB, PWM_GPIOB_OUTPUT_PINS);
}

/*!
    \brief      配置 PWM 引脚复用并保留 SWD 调试接口
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void PWM_GPIO_ConfigureAlternateFunction(void)
{
    /* 释放 PA15/PB3/PB4 的 JTAG 功能，同时保留 PA13/PA14 的 SWD。 */
    gpio_pin_remap_config(GPIO_SWJ_SWDPENABLE_REMAP, ENABLE);
    gpio_pin_remap_config(GPIO_TIMER1_FULL_REMAP, ENABLE);
    gpio_pin_remap_config(GPIO_TIMER2_PARTIAL_REMAP, ENABLE);

    gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ,
              PWM_PHASE_C_POSITIVE_PIN);
    gpio_init(GPIOB, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ,
              PWM_GPIOB_OUTPUT_PINS);
}

/*!
    \brief      配置一台固定 100 kHz 的双通道逻辑互补 PWM 定时器
    \param[in]  timer_periph: TIMER1、TIMER2 或 TIMER3 外设基地址
    \param[in]  compare_value: CH0/CH1 共用的比较计数值
    \param[out] 无
    \retval     无
*/
static void PWM_Timer_Configure(uint32_t timer_periph, uint16_t compare_value)
{
    timer_parameter_struct timer_initpara; /*!< 当前 PWM 定时器的基本计数参数。 */
    timer_oc_parameter_struct timer_ocpara; /*!< 当前 PWM 定时器的通道输出参数。 */

    timer_struct_para_init(&timer_initpara);
    timer_initpara.prescaler = PWM_TIMER_PRESCALER;
    timer_initpara.alignedmode = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection = TIMER_COUNTER_UP;
    timer_initpara.period = PWM_TIMER_PERIOD;
    timer_initpara.clockdivision = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0U;
    timer_init(timer_periph, &timer_initpara);

    timer_channel_output_struct_para_init(&timer_ocpara);
    timer_ocpara.outputstate = TIMER_CCX_DISABLE;
    timer_ocpara.outputnstate = TIMER_CCXN_DISABLE;
    timer_ocpara.ocpolarity = TIMER_OC_POLARITY_HIGH;
    timer_ocpara.ocnpolarity = TIMER_OCN_POLARITY_HIGH;
    timer_ocpara.ocidlestate = TIMER_OC_IDLE_STATE_LOW;
    timer_ocpara.ocnidlestate = TIMER_OCN_IDLE_STATE_LOW;
    timer_channel_output_config(timer_periph, TIMER_CH_0, &timer_ocpara);
    timer_channel_output_config(timer_periph, TIMER_CH_1, &timer_ocpara);

    /* CH0 使用 PWM0，CH1 使用 PWM1，在同一比较值下形成逻辑互补。 */
    timer_channel_output_mode_config(timer_periph, TIMER_CH_0,
                                     TIMER_OC_MODE_PWM0);
    timer_channel_output_mode_config(timer_periph, TIMER_CH_1,
                                     TIMER_OC_MODE_PWM1);
    timer_channel_output_shadow_config(timer_periph, TIMER_CH_0,
                                       TIMER_OC_SHADOW_ENABLE);
    timer_channel_output_shadow_config(timer_periph, TIMER_CH_1,
                                       TIMER_OC_SHADOW_ENABLE);
    timer_channel_output_pulse_value_config(timer_periph, TIMER_CH_0,
                                            compare_value);
    timer_channel_output_pulse_value_config(timer_periph, TIMER_CH_1,
                                            compare_value);
    timer_auto_reload_shadow_enable(timer_periph);
}

/*!
    \brief      同时配置三相六个定时器通道的输出使能状态
    \param[in]  state: TIMER_CCX_ENABLE 或 TIMER_CCX_DISABLE
    \param[out] 无
    \retval     无
*/
static void PWM_ChannelStateConfigure(uint32_t state)
{
    timer_channel_output_state_config(PWM_PHASE_A_TIMER, TIMER_CH_0, state);
    timer_channel_output_state_config(PWM_PHASE_A_TIMER, TIMER_CH_1, state);
    timer_channel_output_state_config(PWM_PHASE_B_TIMER, TIMER_CH_0, state);
    timer_channel_output_state_config(PWM_PHASE_B_TIMER, TIMER_CH_1, state);
    timer_channel_output_state_config(PWM_PHASE_C_TIMER, TIMER_CH_0, state);
    timer_channel_output_state_config(PWM_PHASE_C_TIMER, TIMER_CH_1, state);
}

/*!
    \brief      触发三相影子寄存器装载并恢复 0/120/240 度计数初值
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void PWM_ReloadSynchronizationState(void)
{
    timer_event_software_generate(PWM_PHASE_A_TIMER, TIMER_EVENT_SRC_UPG);
    timer_event_software_generate(PWM_PHASE_B_TIMER, TIMER_EVENT_SRC_UPG);
    timer_event_software_generate(PWM_PHASE_C_TIMER, TIMER_EVENT_SRC_UPG);

    /* B 相再计数 400 个计数回绕，C 相再计数 800 个计数回绕。 */
    timer_counter_value_config(PWM_PHASE_A_TIMER,
                               (uint16_t)PWM_PHASE_A_COUNTER_START);
    timer_counter_value_config(PWM_PHASE_B_TIMER,
                               (uint16_t)PWM_PHASE_B_COUNTER_START);
    timer_counter_value_config(PWM_PHASE_C_TIMER,
                               (uint16_t)PWM_PHASE_C_COUNTER_START);
}

/*!
    \brief      将逻辑相位映射到对应的定时器外设
    \param[in]  phase: A、B 或 C 相编号
    \param[out] 无
    \return     对应 TIMER3、TIMER2 或 TIMER1 外设基地址
*/
static uint32_t PWM_TimerFromPhase(pwm_phase_t phase)
{
    uint32_t timer_periph; /*!< 当前逻辑相位对应的定时器外设基地址。 */

    switch (phase) {
    case PWM_PHASE_A:
        timer_periph = PWM_PHASE_A_TIMER;
        break;
    case PWM_PHASE_B:
        timer_periph = PWM_PHASE_B_TIMER;
        break;
    case PWM_PHASE_C:
        timer_periph = PWM_PHASE_C_TIMER;
        break;
    default:
        timer_periph = PWM_PHASE_A_TIMER;
        break;
    }

    return timer_periph;
}

/*!
    \brief      将百分比占空比换算为定时器比较计数值
    \param[in]  duty_percent: 请求占空比，单位 %
    \param[out] 无
    \return     直接截断量化后的 CCR 比较计数值
*/
static uint16_t PWM_CalculateCompareValue(float duty_percent)
{
    float compare_value; /*!< 百分比换算得到的浮点 CCR 计数值。 */

    compare_value = (duty_percent * (float)PWM_PERIOD_COUNTS) / 100.0f;
    return (uint16_t)compare_value;
}

/*!
    \brief      将比较计数值换算为实际量化占空比
    \param[in]  compare_value: CCR 比较计数值
    \param[out] 无
    \return     实际量化占空比，单位 %
*/
static float PWM_CalculateActualDuty(uint16_t compare_value)
{
    return ((float)compare_value * 100.0f) / (float)PWM_PERIOD_COUNTS;
}
