#ifndef KEY_H
#define KEY_H

/*!
    \file    key.h
    \brief   四路低电平有效按键 EXTI 与非阻塞消抖接口
*/

#include <stdint.h>

/* 主循环 KEY_Task() 使用的按下和松开消抖时间。 */
#ifndef KEY_DEBOUNCE_TIME_MS
#define KEY_DEBOUNCE_TIME_MS            20U /*!< 按键稳定电平确认时间，单位 ms。 */
#endif

/*! \brief 板上四路按键的逻辑编号。 */
typedef enum {
    KEY_ID_1 = 0,                       /*!< KEY1，对应 PA8。 */
    KEY_ID_2,                           /*!< KEY2，对应 PB15。 */
    KEY_ID_3,                           /*!< KEY3，对应 PB14。 */
    KEY_ID_4,                           /*!< KEY4，对应 PB13。 */
    KEY_ID_COUNT                        /*!< 按键数量，仅用于边界和数组长度。 */
} key_id_t;

/*! \brief 按键稳定按下后在主循环上下文调用的回调函数类型。 */
typedef void (*key_callback_t)(void);

/*!
    \brief      初始化四路上拉输入、下降沿 EXTI 和 NVIC 通道
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void KEY_Init(void);

/*!
    \brief      注册一路按键的主循环按下回调
    \param[in]  key: 待配置的按键编号
    \param[in]  callback: 稳定按下回调；传入 NULL 时取消回调
    \param[out] 无
    \retval     无
*/
void KEY_SetCallback(key_id_t key, key_callback_t callback);

/*!
    \brief      执行四路按键的 20 ms 非阻塞按下/松开消抖
    \param[in]  无
    \param[out] 无
    \retval     无
    \note       必须在主循环中持续调用，本函数不会阻塞
*/
void KEY_Task(void);

/*!
    \brief      处理 EXTI5～EXTI9 中的 KEY1 中断
    \param[in]  无
    \param[out] 无
    \retval     无
    \note       中断内只清除 EXTI 标志并设置软件待处理位
*/
void KEY_EXTI5_9_IRQHandler_Internal(void);

/*!
    \brief      处理 EXTI10～EXTI15 中的 KEY2～KEY4 中断
    \param[in]  无
    \param[out] 无
    \retval     无
    \note       中断内只清除 EXTI 标志并设置软件待处理位
*/
void KEY_EXTI10_15_IRQHandler_Internal(void);

#endif /* KEY_H */
