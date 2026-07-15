#ifndef APP_KEY_H
#define APP_KEY_H

/*!
    \file    app_key.h
    \brief   实例工程按键业务绑定接口
*/

/*!
    \brief      初始化 SHUTOFF、按键驱动及四路业务回调
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void AppKey_Init(void);

/*!
    \brief      执行按键消抖与业务回调任务
    \param[in]  无
    \param[out] 无
    \retval     无
    \note       必须在主循环中持续调用
*/
void AppKey_Task(void);

#endif /* APP_KEY_H */
