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
static volatile uint8_t boost_voltage_increase_command = 0U; /*!< 电压目标增加请求：1 待处理。 */
static volatile uint8_t boost_voltage_decrease_command = 0U; /*!< 电压目标减少请求：1 待处理。 */
static volatile uint8_t boost_current_increase_command = 0U; /*!< 电流目标增加请求：1 待处理。 */
static volatile uint8_t boost_current_decrease_command = 0U; /*!< 电流目标减少请求：1 待处理。 */

static void BoostControl_ClearDutyData(void);
static void BoostControl_ResetRuntimeData(void);
static void BoostControl_HandleSetpointAdjustment(void);
static void BoostControl_HandleCommand(void);
static void BoostControl_UpdateAdcData(void);
static void BoostControl_CheckProtection(void);
static void BoostControl_UpdateSoftStart(void);
static void BoostControl_UpdatePowerLoop(void);
static void BoostControl_UpdatePhaseDuty(void);
static void BoostControl_ExecuteActiveOutput(void);
static void BoostControl_ExecuteState(void);

float adc_output_current_a = 0.0f;

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
    boost_voltage_increase_command = 0U;
    boost_voltage_decrease_command = 0U;
    boost_current_increase_command = 0U;
    boost_current_decrease_command = 0U;
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
    boost_control.voltage_setpoint_v = BOOST_OUTPUT_VOLTAGE_SETPOINT_V;
    boost_control.current_setpoint_a = BOOST_OUTPUT_CURRENT_SETPOINT_A;
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
        // DutyControl_SetThreePhaseDuty(33.3f,33.3f, 33.3f);
		// DutyControl_Start();
		// DutyControl_SetThreePhaseDuty(33.3f,33.3f, 33.3f);
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
    context->voltage_setpoint_v = boost_control.voltage_setpoint_v;
    context->current_setpoint_a = boost_control.current_setpoint_a;
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
    \brief      请求将输出电压目标增加 0.1 V
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void BoostControl_RequestIncreaseVoltageSetpoint(void)
{
    boost_voltage_increase_command = 1U;
}

/*!
    \brief      请求将输出电压目标减少 0.1 V
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void BoostControl_RequestDecreaseVoltageSetpoint(void)
{
    boost_voltage_decrease_command = 1U;
}

/*!
    \brief      请求将输出电流目标增加 0.01 A
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void BoostControl_RequestIncreaseCurrentSetpoint(void)
{
    boost_current_increase_command = 1U;
}

/*!
    \brief      请求将输出电流目标减少 0.01 A
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void BoostControl_RequestDecreaseCurrentSetpoint(void)
{
    boost_current_decrease_command = 1U;
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
    if (boost_control.state == BOOST_STATE_FAULT) {
        BoostControl_ExecuteState();
    }
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
    boost_control.voltage_reference_v = BOOST_SOFT_START_INITIAL_VOLTAGE_V;
    boost_control.mode = BOOST_POWER_MODE_CV;
    BoostControl_ClearDutyData();
}

/*!
    \brief      消费外部提交的电压、电流目标调整请求并更新控制环目标
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void BoostControl_HandleSetpointAdjustment(void)
{
    float voltage_setpoint_v; /*!< 应用调整并限幅后的输出电压目标。 */
    float current_setpoint_a; /*!< 应用调整并限幅后的输出电流目标。 */

    voltage_setpoint_v = boost_control.voltage_setpoint_v;
    current_setpoint_a = boost_control.current_setpoint_a;

    if (boost_voltage_increase_command != 0U) {
        boost_voltage_increase_command = 0U;
        voltage_setpoint_v += BOOST_OUTPUT_VOLTAGE_ADJUST_STEP_V;
    }

    if (boost_voltage_decrease_command != 0U) {
        boost_voltage_decrease_command = 0U;
        voltage_setpoint_v -= BOOST_OUTPUT_VOLTAGE_ADJUST_STEP_V;
    }

    if (boost_current_increase_command != 0U) {
        boost_current_increase_command = 0U;
        current_setpoint_a += BOOST_OUTPUT_CURRENT_ADJUST_STEP_A;
    }

    if (boost_current_decrease_command != 0U) {
        boost_current_decrease_command = 0U;
        current_setpoint_a -= BOOST_OUTPUT_CURRENT_ADJUST_STEP_A;
    }

    if (voltage_setpoint_v > BOOST_OUTPUT_VOLTAGE_SETPOINT_MAX_V) {
        voltage_setpoint_v = BOOST_OUTPUT_VOLTAGE_SETPOINT_MAX_V;
    } else if (voltage_setpoint_v < BOOST_OUTPUT_VOLTAGE_SETPOINT_MIN_V) {
        voltage_setpoint_v = BOOST_OUTPUT_VOLTAGE_SETPOINT_MIN_V;
    }

    if (current_setpoint_a > BOOST_OUTPUT_CURRENT_SETPOINT_MAX_A) {
        current_setpoint_a = BOOST_OUTPUT_CURRENT_SETPOINT_MAX_A;
    } else if (current_setpoint_a < BOOST_OUTPUT_CURRENT_SETPOINT_MIN_A) {
        current_setpoint_a = BOOST_OUTPUT_CURRENT_SETPOINT_MIN_A;
    }

    boost_control.voltage_setpoint_v = voltage_setpoint_v;
    boost_control.current_setpoint_a = current_setpoint_a;
    if ((boost_control.state == BOOST_STATE_RUN) ||
        (boost_control.voltage_reference_v > voltage_setpoint_v)) {
        boost_control.voltage_reference_v = voltage_setpoint_v;
    }
}

/*!
    \brief      按 STOP、CLEAR_FAULT、START 优先级消费外部命令
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void BoostControl_HandleCommand(void)
{
    BoostControl_HandleSetpointAdjustment();

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
    \brief      检查 Boost ADC 校准、输出过压和输出过流软件保护
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
    if (boost_control.adc.output_current_a >
        BOOST_OUTPUT_OVERCURRENT_THRESHOLD_V) {
        boost_control.fault_flags |= BOOST_FAULT_OUTPUT_OVERCURRENT;
        adc_output_current_a = boost_control.adc.output_current_a;
    }
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
        boost_control.voltage_setpoint_v) {
        boost_control.voltage_reference_v = boost_control.voltage_setpoint_v;
        boost_control.state = BOOST_STATE_RUN;
    }
}

/*!
    \brief      计算电压环、电流环及三相公共目标占空比
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
    current_error = boost_control.current_setpoint_a -
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

    BoostControl_UpdatePhaseDuty();
}

/*!
    \brief      按三相各自电流限制统一更新 A/B/C 相占空比
    \param[in]  无
    \param[out] 无
    \retval     无
    \note       超限时只禁止增加；目标降低时始终允许下降
*/
static void BoostControl_UpdatePhaseDuty(void)
{
    float duty_step; /*!< 当前相追赶公共目标所需的占空比增量。 */

    if (boost_control.duty_total_percent <=
        boost_control.duty_phase_a_percent) {
        boost_control.duty_phase_a_percent =
            boost_control.duty_total_percent;
    } else if (!(boost_control.adc.phase_a_current_a >
                 BOOST_PHASE_CURRENT_LIMIT_A)) {
        duty_step = boost_control.duty_total_percent -
                    boost_control.duty_phase_a_percent;
        if (duty_step > BOOST_DUTY_MAX_STEP_PERCENT) {
            duty_step = BOOST_DUTY_MAX_STEP_PERCENT;
        }
        boost_control.duty_phase_a_percent += duty_step;
    }

    if (boost_control.duty_total_percent <=
        boost_control.duty_phase_b_percent) {
        boost_control.duty_phase_b_percent =
            boost_control.duty_total_percent;
    } else if (!(boost_control.adc.phase_b_current_a >
                 BOOST_PHASE_CURRENT_LIMIT_A)) {
        duty_step = boost_control.duty_total_percent -
                    boost_control.duty_phase_b_percent;
        if (duty_step > BOOST_DUTY_MAX_STEP_PERCENT) {
            duty_step = BOOST_DUTY_MAX_STEP_PERCENT;
        }
        boost_control.duty_phase_b_percent += duty_step;
    }

    if (boost_control.duty_total_percent <=
        boost_control.duty_phase_c_percent) {
        boost_control.duty_phase_c_percent =
            boost_control.duty_total_percent;
    } else if (!(boost_control.adc.phase_c_current_a >
                 BOOST_PHASE_CURRENT_LIMIT_A)) {
        duty_step = boost_control.duty_total_percent -
                    boost_control.duty_phase_c_percent;
        if (duty_step > BOOST_DUTY_MAX_STEP_PERCENT) {
            duty_step = BOOST_DUTY_MAX_STEP_PERCENT;
        }
        boost_control.duty_phase_c_percent += duty_step;
    }
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
