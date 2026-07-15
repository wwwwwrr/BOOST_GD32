#ifndef TIMER4_CONTROL_H
#define TIMER4_CONTROL_H

#include "gd32f30x.h"
#include "interrupt_priority.h"

/* TIMER4 位于 APB1；当前 60 MHz APB1 在定时器倍频后得到 120 MHz 输入时钟。 */
#define TIMER4_INPUT_CLOCK_HZ             120000000U
#define TIMER4_FIXED_PRESCALER            119U
#define TIMER4_COUNTER_MAX_COUNTS         65536U

/*! \brief TIMER4 更新中断回调函数类型。 */
typedef void (*timer4_callback_t)(void);

/*!
    \brief      按指定频率初始化 TIMER4 更新中断
    \param[in]  frequency_hz: 目标更新频率，单位 Hz
    \retval     1: 初始化成功
    \retval     0: 频率无法由当前时钟参数精确产生
*/
uint8_t Timer4_Init(uint32_t frequency_hz);

/*!
    \brief      注册 TIMER4 更新中断回调
    \param[in]  callback: 中断触发时调用的回调函数
    \retval     无
*/
void Timer4_SetCallback(timer4_callback_t callback);

/*!
    \brief      从零计数值启动 TIMER4
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void Timer4_Start(void);

/*!
    \brief      停止 TIMER4 并清除更新中断标志
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void Timer4_Stop(void);

/*!
    \brief      处理 TIMER4 更新中断并调用已注册回调
    \param[in]  无
    \param[out] 无
    \retval     无
    \note       由芯片 TIMER4_IRQHandler 转发调用，禁止在此加入阻塞操作
*/
void Timer4_IRQHandler_Internal(void);

#endif /* TIMER4_CONTROL_H */
