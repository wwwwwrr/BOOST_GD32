#include "timer4.h"
#include <stddef.h>

static timer4_callback_t timer4_callback = NULL; /*!< TIMER4 更新中断的已注册回调。 */
static volatile uint8_t timer4_initialized = 0U; /*!< TIMER4 初始化标志：1 表示参数有效。 */

/*!
    \brief      按指定频率初始化 TIMER4 更新中断
    \param[in]  frequency_hz: 目标更新频率，单位 Hz
    \retval     1: 初始化成功
    \retval     0: 频率无法由当前时钟参数精确产生
*/
uint8_t Timer4_Init(uint32_t frequency_hz)
{
    timer_parameter_struct timer_parameters; /*!< TIMER4 基本计数参数。 */
    uint32_t clock_divider;                  /*!< 预分频后计算目标频率所用的总除数。 */
    uint32_t period_counts;                  /*!< TIMER4 自动重载周期包含的计数数。 */

    timer4_initialized = 0U;
    if (frequency_hz == 0U) {
        return 0U;
    }

    clock_divider = (TIMER4_FIXED_PRESCALER + 1U) * frequency_hz;
    if ((clock_divider == 0U) ||
        ((TIMER4_INPUT_CLOCK_HZ % clock_divider) != 0U)) {
        return 0U;
    }

    period_counts = TIMER4_INPUT_CLOCK_HZ / clock_divider;
    if ((period_counts == 0U) ||
        (period_counts > TIMER4_COUNTER_MAX_COUNTS)) {
        return 0U;
    }

    rcu_periph_clock_enable(RCU_TIMER4);
    timer_deinit(TIMER4);
    timer_struct_para_init(&timer_parameters);
    timer_parameters.prescaler = TIMER4_FIXED_PRESCALER;
    timer_parameters.alignedmode = TIMER_COUNTER_EDGE;
    timer_parameters.counterdirection = TIMER_COUNTER_UP;
    timer_parameters.period = period_counts - 1U;
    timer_parameters.clockdivision = TIMER_CKDIV_DIV1;
    timer_parameters.repetitioncounter = 0U;
    timer_init(TIMER4, &timer_parameters);

    timer_interrupt_flag_clear(TIMER4, TIMER_INT_FLAG_UP);
    timer_interrupt_enable(TIMER4, TIMER_INT_UP);
    nvic_irq_enable(TIMER4_IRQn,
                    TIMER4_PRIORITY_GROUP,
                    TIMER4_PRIORITY_SUBGROUP);

    timer4_initialized = 1U;
    return 1U;
}

/*!
    \brief      注册 TIMER4 更新中断回调
    \param[in]  callback: 中断触发时调用的回调函数
    \retval     无
*/
void Timer4_SetCallback(timer4_callback_t callback)
{
    timer4_callback = callback;
}

/*!
    \brief      从零计数值启动 TIMER4
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void Timer4_Start(void)
{
    if (timer4_initialized == 0U) {
        return;
    }

    timer_counter_value_config(TIMER4, 0U);
    timer_interrupt_flag_clear(TIMER4, TIMER_INT_FLAG_UP);
    timer_enable(TIMER4);
}

/*!
    \brief      停止 TIMER4 并清除更新中断标志
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void Timer4_Stop(void)
{
    timer_disable(TIMER4);
    timer_interrupt_flag_clear(TIMER4, TIMER_INT_FLAG_UP);
}

/*!
    \brief      处理 TIMER4 更新中断并调用已注册回调
    \param[in]  无
    \param[out] 无
    \retval     无
    \note       由芯片 TIMER4_IRQHandler 转发调用，禁止在此加入阻塞操作
*/
void Timer4_IRQHandler_Internal(void)
{
    if (timer_interrupt_flag_get(TIMER4, TIMER_INT_FLAG_UP) == RESET) {
        return;
    }

    timer_interrupt_flag_clear(TIMER4, TIMER_INT_FLAG_UP);
    if (timer4_callback != NULL) {
        timer4_callback();
    }
}
