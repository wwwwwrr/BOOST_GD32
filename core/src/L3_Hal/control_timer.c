#include "control_timer.h"
#include "project_config.h"
#include "timer4.h"
#include <stddef.h>

/*!
    \brief      初始化平台控制定时器并注册回调
    \param[in]  callback: 每个控制周期调用的回调函数
    \retval     1: 初始化成功
    \retval     0: 回调为空或底层定时器参数无效
*/
uint8_t ControlTimer_Init(control_timer_callback_t callback)
{
    if (callback == NULL) {
        return 0U;
    }

    Timer4_SetCallback(callback);
    return Timer4_Init(BOOST_CONTROL_FREQUENCY_HZ);
}

/*!
    \brief      启动平台控制定时器
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void ControlTimer_Start(void)
{
    Timer4_Start();
}

/*!
    \brief      停止平台控制定时器
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void ControlTimer_Stop(void)
{
    Timer4_Stop();
}
