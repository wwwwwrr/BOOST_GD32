#ifndef CONTROL_TIMER_H
#define CONTROL_TIMER_H

#include <stdint.h>

/*! \brief 平台无关的控制定时器回调函数类型。 */
typedef void (*control_timer_callback_t)(void);

/*!
    \brief      初始化平台控制定时器并注册回调
    \param[in]  callback: 每个控制周期调用的回调函数
    \retval     1: 初始化成功
    \retval     0: 回调为空或底层定时器参数无效
*/
uint8_t ControlTimer_Init(control_timer_callback_t callback);

/*!
    \brief      启动平台控制定时器
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void ControlTimer_Start(void);

/*!
    \brief      停止平台控制定时器
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void ControlTimer_Stop(void);

#endif /* CONTROL_TIMER_H */
