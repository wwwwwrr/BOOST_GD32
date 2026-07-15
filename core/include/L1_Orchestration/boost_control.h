#ifndef BOOST_CONTROL_H
#define BOOST_CONTROL_H

#include <stdint.h>

/*! \brief Boost 系统运行状态。 */
typedef enum {
    BOOST_STATE_IDLE = 0,       /*!< 空闲状态，三相 PWM 保持关闭。 */
    BOOST_STATE_SOFT_START,     /*!< 软启动状态，电压目标按固定斜率上升。 */
    BOOST_STATE_RUN,            /*!< 正常双闭环运行状态。 */
    BOOST_STATE_FAULT           /*!< 故障锁存状态，三相 PWM 强制关闭。 */
} boost_system_state_t;

/*! \brief Boost 当前起限制作用的功率控制模式。 */
typedef enum {
    BOOST_POWER_MODE_CV = 0,    /*!< 电压环占空比较小，当前处于恒压限制。 */
    BOOST_POWER_MODE_CC         /*!< 电流环占空比较小，当前处于恒流限制。 */
} boost_power_mode_t;

/*! \brief Boost 软件故障标志位。 */
typedef enum {
    BOOST_FAULT_NONE = 0x00000000U,             /*!< 当前没有锁存故障。 */
    BOOST_FAULT_OUTPUT_OVERVOLTAGE = 0x00000001U /*!< Boost 输出过压故障。 */
} boost_fault_flag_t;

/*! \brief Boost 控制使用的 ADC 实际值。 */
typedef struct {
    float output_voltage_v;       /*!< Boost 输出电压，单位 V。 */
    float output_current_a;       /*!< Boost 输出电流，单位 A。 */
    float input_voltage_v;        /*!< Boost 输入电压，单位 V。 */
    float phase_a_current_a;      /*!< A 相电感电流，单位 A。 */
    float phase_b_current_a;      /*!< B 相电感电流，单位 A。 */
    float phase_c_current_a;      /*!< C 相电感电流，单位 A。 */
} boost_adc_data_t;

/*! \brief Boost 状态、ADC、软启动目标及占空比运行上下文。 */
typedef struct {
    boost_system_state_t state;   /*!< 当前 Boost 系统状态。 */
    boost_power_mode_t mode;      /*!< 当前恒压或恒流限制模式。 */
    uint32_t fault_flags;         /*!< 已锁存的 Boost 故障标志位。 */
    boost_adc_data_t adc;         /*!< 当前控制周期使用的 ADC 实际值。 */
    float voltage_reference_v;    /*!< 当前软启动电压目标，单位 V。 */
    float duty_voltage_percent;   /*!< 电压环占空比输出，单位 %。 */
    float duty_current_percent;   /*!< 电流环占空比输出，单位 %。 */
    float duty_total_percent;     /*!< 双环取小后的公共占空比，单位 %。 */
    float duty_phase_a_percent;   /*!< A 相占空比，单位 %。 */
    float duty_phase_b_percent;   /*!< B 相占空比，单位 %。 */
    float duty_phase_c_percent;   /*!< C 相占空比，单位 %。 */
    uint8_t pwm_running;          /*!< PWM 运行标志：1 已启动，0 已停止。 */
} boost_control_context_t;

/*!
    \brief      初始化 Boost 控制系统并启动 10 kHz 控制定时器
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void BoostControl_Init(void);

/*!
    \brief      将当前 Boost 运行上下文复制到调用方缓冲区
    \param[in]  无
    \param[out] context: Boost 上下文副本
    \retval     1 复制成功，0 参数为空
    \note       本接口不关闭中断；副本可能包含相邻控制周期的数据
*/
uint8_t BoostControl_GetContext(boost_control_context_t *context);

/*!
    \brief      请求 Boost 从空闲状态进入软启动
    \param[in]  无
    \param[out] 无
    \retval     无
    \note       本函数只置位命令，命令由下一个 10 kHz 控制周期消费
*/
void BoostControl_RequestStart(void);

/*!
    \brief      请求 Boost 停止运行
    \param[in]  无
    \param[out] 无
    \retval     无
    \note       STOP 不清除已经锁存的故障
*/
void BoostControl_RequestStop(void);

/*!
    \brief      请求清除 Boost 锁存故障
    \param[in]  无
    \param[out] 无
    \retval     无
    \note       仅在故障状态有效，保护条件仍存在时会在同一周期重新锁存
*/
void BoostControl_RequestClearFault(void);

/*!
    \brief      执行 Boost 10 kHz 快速控制任务
    \param[in]  无
    \param[out] 无
    \retval     无
    \note       由 TIMER4 每 100 us 调用一次，禁止阻塞或执行通信输出
*/
void BoostControl_10kHzHandler(void);

#endif /* BOOST_CONTROL_H */
