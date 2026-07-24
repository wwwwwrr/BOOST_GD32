#ifndef _PWM_H_
#define _PWM_H_

/*!
    \file    pwm.h
    \brief   三相 120 度交错、六路逻辑互补 PWM 驱动

    \version 2026-7-14, V2.1.0, Chinese documentation completion
*/

#include "gd32f30x.h"

/* 固定 PWM 时序：120 MHz / 1200 = 100 kHz。 */
#define PWM_TIMER_CLOCK_HZ              120000000U /*!< 三个 PWM 定时器的输入时钟，单位 Hz。 */
#define PWM_FREQUENCY_HZ                100000U    /*!< 三相 PWM 固定输出频率，单位 Hz。 */
#define PWM_TIMER_PRESCALER             0U         /*!< PWM 定时器预分频寄存器值，不分频。 */
#define PWM_PERIOD_COUNTS               (PWM_TIMER_CLOCK_HZ / PWM_FREQUENCY_HZ) /*!< 一个 PWM 周期包含的计数数。 */
#define PWM_TIMER_PERIOD                (PWM_PERIOD_COUNTS - 1U) /*!< PWM 定时器自动重载寄存器值。 */
#define PWM_PHASE_STEP_COUNTS           (PWM_PERIOD_COUNTS / 3U) /*!< 相邻两相 120 度对应的计数差。 */

/* TIMER1、TIMER2、TIMER3 均为普通定时器，不具备硬件死区发生器。 */
#ifndef PWM_DEAD_TIME_NS
#define PWM_DEAD_TIME_NS                0U /*!< PWM 死区时间配置，单位 ns，必须保持为 0。 */
#endif

#if (PWM_DEAD_TIME_NS != 0U)
#error "TIMER1/TIMER2/TIMER3 do not support hardware dead time; PWM_DEAD_TIME_NS must be 0"
#endif

#if ((PWM_TIMER_CLOCK_HZ % PWM_FREQUENCY_HZ) != 0U)
#error "PWM timer clock must be an integer multiple of PWM frequency"
#endif

#if ((PWM_PERIOD_COUNTS % 3U) != 0U)
#error "PWM period must be divisible by three for exact 120-degree phase offsets"
#endif

#if (PWM_PERIOD_COUNTS > 65535U)
#error "PWM period and compare threshold must fit the 16-bit timer registers"
#endif

/* A 相：TIMER3 主定时器，PB6/CH0 为正向输出，PB7/CH1 为逻辑互补输出。 */
#define PWM_PHASE_A_TIMER               TIMER3      /*!< A 相 PWM 定时器外设。 */
#define PWM_PHASE_A_TIMER_RCU           RCU_TIMER3  /*!< A 相 PWM 定时器时钟。 */
#define PWM_PHASE_A_POSITIVE_GPIO       GPIOB       /*!< A 相正向输出 GPIO 端口。 */
#define PWM_PHASE_A_POSITIVE_PIN        GPIO_PIN_6  /*!< A 相正向输出引脚 PB6。 */
#define PWM_PHASE_A_COMPLEMENT_GPIO     GPIOB       /*!< A 相逻辑互补输出 GPIO 端口。 */
#define PWM_PHASE_A_COMPLEMENT_PIN      GPIO_PIN_7  /*!< A 相逻辑互补输出引脚 PB7。 */

/* B 相：TIMER2 从定时器，部分重映射，PB4/CH0 正向、PB5/CH1 逻辑互补。 */
#define PWM_PHASE_B_TIMER               TIMER2      /*!< B 相 PWM 定时器外设。 */
#define PWM_PHASE_B_TIMER_RCU           RCU_TIMER2  /*!< B 相 PWM 定时器时钟。 */
#define PWM_PHASE_B_POSITIVE_GPIO       GPIOB       /*!< B 相正向输出 GPIO 端口。 */
#define PWM_PHASE_B_POSITIVE_PIN        GPIO_PIN_4  /*!< B 相正向输出引脚 PB4。 */
#define PWM_PHASE_B_COMPLEMENT_GPIO     GPIOB       /*!< B 相逻辑互补输出 GPIO 端口。 */
#define PWM_PHASE_B_COMPLEMENT_PIN      GPIO_PIN_5  /*!< B 相逻辑互补输出引脚 PB5。 */

/* C 相：TIMER1 从定时器，全重映射，PA15/CH0 正向、PB3/CH1 逻辑互补。 */
#define PWM_PHASE_C_TIMER               TIMER1      /*!< C 相 PWM 定时器外设。 */
#define PWM_PHASE_C_TIMER_RCU           RCU_TIMER1  /*!< C 相 PWM 定时器时钟。 */
#define PWM_PHASE_C_POSITIVE_GPIO       GPIOA       /*!< C 相正向输出 GPIO 端口。 */
#define PWM_PHASE_C_POSITIVE_PIN        GPIO_PIN_15 /*!< C 相正向输出引脚 PA15。 */
#define PWM_PHASE_C_COMPLEMENT_GPIO     GPIOB       /*!< C 相逻辑互补输出 GPIO 端口。 */
#define PWM_PHASE_C_COMPLEMENT_PIN      GPIO_PIN_3  /*!< C 相逻辑互补输出引脚 PB3。 */

#define PWM_DEFAULT_DUTY_A              20.0f /*!< A 相初始化请求占空比，单位 %；初始化期间输出保持关闭。 */
#define PWM_DEFAULT_DUTY_B              20.0f /*!< B 相初始化请求占空比，单位 %；初始化期间输出保持关闭。 */
#define PWM_DEFAULT_DUTY_C              20.0f /*!< C 相初始化请求占空比，单位 %；初始化期间输出保持关闭。 */

/* 计数器预装值使 A/B/C 计数周期边界依次相差 0、120、240 度。 */
#define PWM_PHASE_A_COUNTER_START       0U /*!< A 相同步启动时的计数器初值。 */
#define PWM_PHASE_B_COUNTER_START       (PWM_PHASE_STEP_COUNTS * 2U) /*!< B 相同步启动时的计数器初值。 */
#define PWM_PHASE_C_COUNTER_START       PWM_PHASE_STEP_COUNTS /*!< C 相同步启动时的计数器初值。 */

/*! \brief 三相 PWM 的逻辑相位编号。 */
typedef enum {
    PWM_PHASE_A = 0,                    /*!< A 相，对应 TIMER3 和 PB6/PB7。 */
    PWM_PHASE_B,                        /*!< B 相，对应 TIMER2 和 PB4/PB5。 */
    PWM_PHASE_C,                        /*!< C 相，对应 TIMER1 和 PA15/PB3。 */
    PWM_PHASE_COUNT                     /*!< PWM 相位数量，仅用于边界和数组长度。 */
} pwm_phase_t;

/*!
    \brief      初始化固定 100 kHz 三相 PWM 硬件
    \param[in]  无
    \param[out] 无
    \retval     无
    \note       初始化完成后六路通道仍保持关闭，GPIO 被强制为低电平
*/
void PWM_Init(void);

/*!
    \brief      重装三相计数器并启动同步 PWM 链
    \param[in]  无
    \param[out] 无
    \retval     无
    \note       仅由软件启动 TIMER3 主定时器，TIMER2/TIMER1 通过 ITI3 Event 模式同步启动
*/
void PWM_Start(void);

/*!
    \brief      停止全部 PWM 定时器并将六路输出引脚强制为低电平
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void PWM_Stop(void);

/*!
    \brief      设置一相正向 PWM 的请求占空比
    \param[in]  phase: PWM_PHASE_A、PWM_PHASE_B 或 PWM_PHASE_C
    \param[in]  duty_percent: 请求占空比，单位 %，底层限制到 0～100
    \param[out] 无
    \retval     无
    \note       CH0 使用 PWM1 在每周期内先低后高，CH1 使用 PWM0 形成逻辑互补波形
*/
void PWM_SetDutyCycle(pwm_phase_t phase, float duty_percent);

/*!
    \brief      批量设置 A/B/C 三相正向 PWM 请求占空比
    \param[in]  duty_a: A 相请求占空比，单位 %
    \param[in]  duty_b: B 相请求占空比，单位 %
    \param[in]  duty_c: C 相请求占空比，单位 %
    \param[out] 无
    \retval     无
*/
void PWM_SetThreePhaseDuty(float duty_a, float duty_b, float duty_c);

/*!
    \brief      读取一相最近一次量化后的实际占空比
    \param[in]  phase: PWM_PHASE_A、PWM_PHASE_B 或 PWM_PHASE_C
    \param[out] 无
    \return     实际量化占空比，单位 %；相位无效时返回 0
*/
float PWM_GetDutyCycle(pwm_phase_t phase);

#endif /* _PWM_H_ */
