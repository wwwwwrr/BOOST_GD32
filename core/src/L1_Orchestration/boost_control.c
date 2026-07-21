#include "boost_control.h"
#include "project_config.h"
#include "incremental_pi.h"
#include "control_timer.h"
#include "adc_measurement.h"
#include "duty_control.h"
#include "status_indicator.h"

static volatile boost_control_context_t boost_control; /*!< 中断写入、线程读取的 Boost 共享运行上下文。 */
static incremental_pi_t boost_voltage_pi; /*!< 仅由 10 kHz 控制任务操作的电压环 PI 实例。 */
static incremental_pi_t boost_current_pi; /*!< 仅由 10 kHz 控制任务操作的电流环 PI 实例。 */
static volatile uint8_t boost_start_command = 0U; /*!< START 待处理标志：1 待处理。 */
static volatile uint8_t boost_stop_command = 0U; /*!< STOP 待处理标志：1 待处理。 */
static volatile uint8_t boost_clear_fault_command = 0U; /*!< CLEAR_FAULT 待处理标志：1 待处理。 */
static uint16_t boost_open_load_count = 0U; /*!< 输出低电流连续控制周期计数。 */
static uint8_t boost_load_detected = 0U; /*!< 本次运行是否曾检测到有效负载：1 是。 */

static void BoostControl_ClearDutyData(void);
static void BoostControl_ResetOpenLoadDetection(void);
static void BoostControl_ResetRuntimeData(void);
static void BoostControl_HandleCommand(void);
static void BoostControl_UpdateAdcData(void);
static void BoostControl_CheckOpenLoad(void);
static void BoostControl_CheckProtection(void);
static void BoostControl_UpdateSoftStart(void);
static void BoostControl_UpdatePowerLoop(void);
static void BoostControl_ExecuteActiveOutput(void);
static void BoostControl_ExecuteState(void);

/*!
    \brief      初始化 Boost 控制系统并启动 10 kHz 控制定时器
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void BoostControl_Init(void)
{
    uint8_t timer_initialized;      /*!< 10 kHz 控制定时器初始化结果：1 成功。 */
    uint8_t phase_offset_ready;     /*!< ADC1 三相偏置校准结果：1 成功。 */

    boost_start_command = 0U;
    boost_stop_command = 0U;
    boost_clear_fault_command = 0U;
    BoostControl_ResetOpenLoadDetection();
    StatusIndicator_Init();

    phase_offset_ready = ADCMeasurement_CalibratePhaseOffsets();

    boost_control.state = (phase_offset_ready != 0U) ?
                          BOOST_STATE_IDLE : BOOST_STATE_FAULT;
    boost_control.mode = BOOST_POWER_MODE_CV;
    boost_control.fault_flags = (phase_offset_ready != 0U) ?
                                BOOST_FAULT_NONE :
                                BOOST_FAULT_ADC_PHASE_CALIBRATION;

    boost_control.adc.output_voltage_v = 0.0f;
    boost_control.adc.output_current_a = 0.0f;
    boost_control.adc.input_voltage_v = 0.0f;
    boost_control.adc.phase_a_current_a = 0.0f;
    boost_control.adc.phase_b_current_a = 0.0f;
    boost_control.adc.phase_c_current_a = 0.0f;
    boost_control.pwm_running = 0U;
    BoostControl_ClearDutyData();
    boost_control.voltage_reference_v = BOOST_SOFT_START_INITIAL_VOLTAGE_V;

    DutyControl_Init();
    DutyControl_SetThreePhaseDuty(0.0f, 0.0f, 0.0f);
    ADCMeasurement_Init();
    ADCMeasurement_Start();

    IncrementalPI_Init(
        &boost_voltage_pi,
        BOOST_VOLTAGE_PI_KP,
        BOOST_VOLTAGE_PI_KI_PER_CYCLE,
        BOOST_VOLTAGE_PI_KD);
    IncrementalPI_Init(
        &boost_current_pi,
        BOOST_CURRENT_PI_KP,
        BOOST_CURRENT_PI_KI_PER_CYCLE,
        BOOST_CURRENT_PI_KD);
    timer_initialized = ControlTimer_Init(BoostControl_10kHzHandler);

    if (timer_initialized != 0U) {
        ControlTimer_Start();
    }
		//DutyControl_Start();
}

/*!
    \brief      将当前 Boost 运行上下文复制到调用方缓冲区
    \param[in]  无
    \param[out] context: Boost 上下文副本
    \retval     1 复制成功，0 参数为空
    \note       本接口由主循环调用，不关闭中断或重试
*/
uint8_t BoostControl_GetContext(boost_control_context_t *context)
{
    if (context == 0) {
        return 0U;
    }

    context->state = boost_control.state;
    context->mode = boost_control.mode;
    context->fault_flags = boost_control.fault_flags;
    context->adc.output_voltage_v = boost_control.adc.output_voltage_v;
    context->adc.output_current_a = boost_control.adc.output_current_a;
    context->adc.input_voltage_v = boost_control.adc.input_voltage_v;
    context->adc.phase_a_current_a = boost_control.adc.phase_a_current_a;
    context->adc.phase_b_current_a = boost_control.adc.phase_b_current_a;
    context->adc.phase_c_current_a = boost_control.adc.phase_c_current_a;
    context->voltage_reference_v = boost_control.voltage_reference_v;
    context->duty_voltage_percent = boost_control.duty_voltage_percent;
    context->duty_current_percent = boost_control.duty_current_percent;
    context->duty_total_percent = boost_control.duty_total_percent;
    context->duty_phase_a_percent = boost_control.duty_phase_a_percent;
    context->duty_phase_b_percent = boost_control.duty_phase_b_percent;
    context->duty_phase_c_percent = boost_control.duty_phase_c_percent;
    context->pwm_running = boost_control.pwm_running;

    return 1U;
}

/*!
    \brief      请求 Boost 从空闲状态进入软启动
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void BoostControl_RequestStart(void)
{
    boost_start_command = 1U;
}

/*!
    \brief      请求 Boost 停止运行
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void BoostControl_RequestStop(void)
{
    boost_stop_command = 1U;
}

/*!
    \brief      请求清除 Boost 锁存故障
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void BoostControl_RequestClearFault(void)
{
    boost_clear_fault_command = 1U;
}

/*!
    \brief      执行 Boost 10 kHz 快速控制任务
    \param[in]  无
    \param[out] 无
    \retval     无
    \note       由 TIMER4 每 100 us 调用一次，禁止阻塞或执行通信输出
*/
void BoostControl_10kHzHandler(void)
{
    BoostControl_HandleCommand();
    BoostControl_UpdateAdcData();
    BoostControl_CheckProtection();
    if (boost_control.state == BOOST_STATE_SOFT_START) {
        BoostControl_UpdateSoftStart();
    }
    if ((boost_control.state == BOOST_STATE_SOFT_START) ||
        (boost_control.state == BOOST_STATE_RUN)) {
        BoostControl_UpdatePowerLoop();
    }

    BoostControl_ExecuteState();
}

/*!
    \brief      清零 Boost 三个占空比计算结果
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void BoostControl_ClearDutyData(void)
{
    boost_control.duty_voltage_percent = 0.0f;
    boost_control.duty_current_percent = 0.0f;
    boost_control.duty_total_percent = 0.0f;
    boost_control.duty_phase_a_percent = 0.0f;
    boost_control.duty_phase_b_percent = 0.0f;
    boost_control.duty_phase_c_percent = 0.0f;
}

/*!
    \brief      清除输出开路检测的负载历史和连续计数
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void BoostControl_ResetOpenLoadDetection(void)
{
    boost_open_load_count = 0U;
    boost_load_detected = 0U;
}

/*!
    \brief      重新初始化 PI、软启动目标、占空比和运行模式
    \param[in]  无
    \param[out] 无
    \retval     无
    \note       本函数不清除 ADC 实际值和故障标志
*/
static void BoostControl_ResetRuntimeData(void)
{
    IncrementalPI_Reset(&boost_voltage_pi);
    IncrementalPI_Reset(&boost_current_pi);
    BoostControl_ResetOpenLoadDetection();
    boost_control.voltage_reference_v = BOOST_SOFT_START_INITIAL_VOLTAGE_V;
    boost_control.mode = BOOST_POWER_MODE_CV;
    BoostControl_ClearDutyData();
}

/*!
    \brief      按 STOP、CLEAR_FAULT、START 优先级消费外部命令
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void BoostControl_HandleCommand(void)
{
    if (boost_stop_command != 0U) {
        boost_stop_command = 0U;
        boost_start_command = 0U;
        BoostControl_ResetRuntimeData();
        if (boost_control.fault_flags != BOOST_FAULT_NONE) {
            boost_control.state = BOOST_STATE_FAULT;
        } else {
            boost_control.state = BOOST_STATE_IDLE;
        }
        return;
    }

    if (boost_clear_fault_command != 0U) {
        boost_clear_fault_command = 0U;
        if (boost_control.state == BOOST_STATE_FAULT) {
            boost_control.fault_flags = BOOST_FAULT_NONE;
            BoostControl_ResetRuntimeData();
            boost_control.state = BOOST_STATE_IDLE;
        }
        return;
    }

    if (boost_start_command != 0U) {
        boost_start_command = 0U;
        if ((boost_control.state == BOOST_STATE_IDLE) &&
            (boost_control.fault_flags == BOOST_FAULT_NONE)) {
            BoostControl_ResetRuntimeData();
            boost_control.state = BOOST_STATE_SOFT_START;
        }
    }
}

/*!
    \brief      更新 Boost 控制使用的 ADC 实际值
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void BoostControl_UpdateAdcData(void)
{
    adc_monitor_value_t monitor_value = {0};   /*!< ADC0 输出监测实际值。 */
    adc_phase_current_value_t phase_value = {0}; /*!< ADC1 三相电流实际值。 */

    ADCMeasurement_ProcessMonitor(&monitor_value);
    ADCMeasurement_GetPhaseCurrents(&phase_value);

    boost_control.adc.output_voltage_v = monitor_value.output_voltage_v;
    boost_control.adc.output_current_a = monitor_value.output_current_a;
    boost_control.adc.input_voltage_v = monitor_value.input_voltage_v;
    boost_control.adc.phase_a_current_a = phase_value.phase_a_current_a;
    boost_control.adc.phase_b_current_a = phase_value.phase_b_current_a;
    boost_control.adc.phase_c_current_a = phase_value.phase_c_current_a;
}

/*!
    \brief      检查静态开路和运行中断开
    \param[in]  无
    \param[out] 无
    \retval     无
    \note       静态开路使用较长确认时间，检测到负载后改用快速确认时间
*/
static void BoostControl_CheckOpenLoad(void)
{
    uint16_t confirm_cycles; /*!< 当前场景要求的低电流连续控制周期数。 */

    if ((boost_control.state != BOOST_STATE_SOFT_START) &&
        (boost_control.state != BOOST_STATE_RUN)) {
        boost_open_load_count = 0U;
        return;
    }

    if (boost_control.adc.output_current_a >=
        BOOST_LOAD_PRESENT_CURRENT_THRESHOLD_A) {
        boost_load_detected = 1U;
        boost_open_load_count = 0U;
        return;
    }

    if (boost_control.adc.output_current_a >=
        BOOST_OPEN_LOAD_CURRENT_THRESHOLD_A) {
        boost_open_load_count = 0U;
        return;
    }

    if ((boost_load_detected == 0U) &&
        (boost_control.adc.output_voltage_v <
         BOOST_STATIC_OPEN_ARM_VOLTAGE_V)) {
        boost_open_load_count = 0U;
        return;
    }

    confirm_cycles = (boost_load_detected != 0U) ?
                     BOOST_RUNTIME_OPEN_CONFIRM_CYCLES :
                     BOOST_STATIC_OPEN_CONFIRM_CYCLES;

    if (boost_open_load_count < confirm_cycles) {
        boost_open_load_count++;
    }

    if (boost_open_load_count >= confirm_cycles) {
        boost_control.fault_flags |= BOOST_FAULT_OUTPUT_OPEN;
    }
}

/*!
    \brief      检查 Boost ADC 校准、输出过压和输出开路软件保护
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void BoostControl_CheckProtection(void)
{
    if (ADCMeasurement_IsPhaseOffsetReady() == 0U) {
        boost_control.fault_flags |= BOOST_FAULT_ADC_PHASE_CALIBRATION;
    }

    if (boost_control.adc.output_voltage_v >
        BOOST_OUTPUT_OVERVOLTAGE_THRESHOLD_V) {
        boost_control.fault_flags |= BOOST_FAULT_OUTPUT_OVERVOLTAGE;
    }

    BoostControl_CheckOpenLoad();

    if (boost_control.fault_flags != BOOST_FAULT_NONE) {
        boost_control.state = BOOST_STATE_FAULT;
    }
}

/*!
    \brief      推进 Boost 软启动电压目标
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void BoostControl_UpdateSoftStart(void)
{
    boost_control.voltage_reference_v += BOOST_SOFT_START_STEP_V;
    if (boost_control.voltage_reference_v >=
        BOOST_OUTPUT_VOLTAGE_SETPOINT_V) {
        boost_control.voltage_reference_v = BOOST_OUTPUT_VOLTAGE_SETPOINT_V;
        boost_control.state = BOOST_STATE_RUN;
    }
}

/*!
    \brief      计算电压环、电流环及三相公共占空比
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void BoostControl_UpdatePowerLoop(void)
{
    float voltage_error; /*!< 当前电压目标与输出电压之差，单位 V。 */
    float current_error; /*!< 当前电流目标与输出电流之差，单位 A。 */

    voltage_error = boost_control.voltage_reference_v -
                    boost_control.adc.output_voltage_v;
    current_error = BOOST_OUTPUT_CURRENT_SETPOINT_A -
                    boost_control.adc.output_current_a;

    boost_control.duty_voltage_percent += IncrementalPI_Calculate(
        &boost_voltage_pi,
        voltage_error);
    boost_control.duty_current_percent += IncrementalPI_Calculate(
        &boost_current_pi,
        current_error);

    if (boost_control.duty_voltage_percent > BOOST_DUTY_MAX_PERCENT) {
        boost_control.duty_voltage_percent = BOOST_DUTY_MAX_PERCENT;
    } else if (boost_control.duty_voltage_percent < BOOST_DUTY_MIN_PERCENT) {
        boost_control.duty_voltage_percent = BOOST_DUTY_MIN_PERCENT;
    }

    if (boost_control.duty_current_percent > BOOST_DUTY_MAX_PERCENT) {
        boost_control.duty_current_percent = BOOST_DUTY_MAX_PERCENT;
    } else if (boost_control.duty_current_percent < BOOST_DUTY_MIN_PERCENT) {
        boost_control.duty_current_percent = BOOST_DUTY_MIN_PERCENT;
    }

    if (boost_control.duty_voltage_percent <=
        boost_control.duty_current_percent) {
        boost_control.duty_total_percent =
            boost_control.duty_voltage_percent;
        boost_control.mode = BOOST_POWER_MODE_CV;
    } else {
        boost_control.duty_total_percent =
            boost_control.duty_current_percent;
        boost_control.mode = BOOST_POWER_MODE_CC;
    }

    if (boost_control.duty_total_percent > BOOST_DUTY_MAX_PERCENT) {
        boost_control.duty_total_percent = BOOST_DUTY_MAX_PERCENT;
    } else if (boost_control.duty_total_percent < BOOST_DUTY_MIN_PERCENT) {
        boost_control.duty_total_percent = BOOST_DUTY_MIN_PERCENT;
    }

    boost_control.duty_phase_a_percent = boost_control.duty_total_percent;
    boost_control.duty_phase_b_percent = boost_control.duty_total_percent;
    boost_control.duty_phase_c_percent = boost_control.duty_total_percent;
}

/*!
    \brief      启动活动状态 PWM 并写入当前三相占空比
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void BoostControl_ExecuteActiveOutput(void)
{
    if (boost_control.pwm_running == 0U) {
        DutyControl_SetThreePhaseDuty(0.0f, 0.0f, 0.0f);
        DutyControl_Start();
        boost_control.pwm_running = 1U;
    }

    DutyControl_SetThreePhaseDuty(
        boost_control.duty_phase_a_percent,
        boost_control.duty_phase_b_percent,
        boost_control.duty_phase_c_percent);
}

/*!
    \brief      根据最终系统状态执行 PWM 和 RGB 状态指示
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void BoostControl_ExecuteState(void)
{
    switch (boost_control.state) {
    case BOOST_STATE_IDLE:
        if (boost_control.pwm_running != 0U) {
            DutyControl_SetThreePhaseDuty(0.0f, 0.0f, 0.0f);
            DutyControl_Stop();
            boost_control.pwm_running = 0U;
        }
        StatusIndicator_SetColor(STATUS_INDICATOR_OFF);
        break;

    case BOOST_STATE_SOFT_START:
        BoostControl_ExecuteActiveOutput();
        StatusIndicator_SetColor(STATUS_INDICATOR_BLUE);
        break;

    case BOOST_STATE_RUN:
        BoostControl_ExecuteActiveOutput();
        StatusIndicator_SetColor(STATUS_INDICATOR_GREEN);
        break;

    case BOOST_STATE_FAULT:
    default:
        BoostControl_ClearDutyData();
        if (boost_control.pwm_running != 0U) {
            DutyControl_SetThreePhaseDuty(0.0f, 0.0f, 0.0f);
            DutyControl_Stop();
            boost_control.pwm_running = 0U;
        }
        StatusIndicator_SetColor(STATUS_INDICATOR_RED);
        break;
    }
}
