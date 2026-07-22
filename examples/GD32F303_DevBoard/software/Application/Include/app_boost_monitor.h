#ifndef APP_BOOST_MONITOR_H
#define APP_BOOST_MONITOR_H

/*!
    \file    app_boost_monitor.h
    \brief   实例工程 Boost 串口命令与状态监视接口
*/

/*!
    \brief      初始化 USART0 与 Boost 状态监视、命令接收任务
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void AppBoostMonitor_Init(void);

/*!
    \brief      按配置周期复制并打印 Boost 运行上下文
    \param[in]  无
    \param[out] 无
    \retval     无
    \note       必须在主循环中持续调用
*/
void AppBoostMonitor_Task(void);

#endif /* APP_BOOST_MONITOR_H */
