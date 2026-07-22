/*!
    \file    app_key.c
    \brief   将板级按键事件绑定到 SHUTOFF 与 Boost 控制命令
*/

#include "app_key.h"
#include "boost_control.h"
#include "key.h"
#include "shutoff.h"

static void AppKey_Key1PressedCallback(void);
static void AppKey_Key2PressedCallback(void);
static void AppKey_Key3PressedCallback(void);
static void AppKey_Key4PressedCallback(void);

/*!
    \brief      初始化 SHUTOFF、按键驱动及四路业务回调
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void AppKey_Init(void)
{
    SHUTOFF_Init();
    KEY_Init();
    KEY_SetCallback(KEY_ID_1, AppKey_Key1PressedCallback);
    KEY_SetCallback(KEY_ID_2, AppKey_Key2PressedCallback);
    KEY_SetCallback(KEY_ID_3, AppKey_Key3PressedCallback);
    KEY_SetCallback(KEY_ID_4, AppKey_Key4PressedCallback);
}

/*!
    \brief      执行按键消抖与业务回调任务
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void AppKey_Task(void)
{
    KEY_Task();
}

/*!
    \brief      KEY1 稳定按下后反转 SHUTOFF 输出
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void AppKey_Key1PressedCallback(void)
{
    SHUTOFF_Toggle();
}

/*!
    \brief      KEY2 稳定按下后请求启动 Boost
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void AppKey_Key2PressedCallback(void)
{
    BoostControl_RequestStart();
}

/*!
    \brief      KEY3 稳定按下后将 Boost 输出电压目标增加 0.1 V
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void AppKey_Key3PressedCallback(void)
{
    /* BoostControl_RequestStop(); */
    BoostControl_RequestIncreaseVoltageSetpoint();
}

/*!
    \brief      KEY4 稳定按下后将 Boost 输出电压目标减少 0.1 V
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void AppKey_Key4PressedCallback(void)
{
    /* BoostControl_RequestClearFault(); */
    BoostControl_RequestDecreaseVoltageSetpoint();
}
