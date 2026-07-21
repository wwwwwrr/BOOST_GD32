/*!
    \file    app_boost_monitor.c
    \brief   编排 Boost 上下文复制、文本格式化与 USART0 调试输出
*/

#include "app_boost_monitor.h"
#include "adc_measurement.h"
#include "boost_control.h"
#include "project_config.h"
#include "systick.h"
#include "usart.h"
#include <stdio.h>

static uint32_t boost_monitor_last_print_tick; /*!< 上次状态打印对应的毫秒时基。 */
static uint8_t boost_monitor_initialized;      /*!< 状态监视模块初始化标志。 */

static const char *AppBoostMonitor_StateToString(boost_system_state_t state);
static const char *AppBoostMonitor_ModeToString(boost_power_mode_t mode);
static void AppBoostMonitor_PrintPhaseOffsets(void);
static void AppBoostMonitor_Print(const boost_control_context_t *context);

/*!
    \brief      初始化 USART0 与 Boost 状态监视任务
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void AppBoostMonitor_Init(void)
{
    USART0_Init();
    boost_monitor_last_print_tick = systick_get_tick();
    boost_monitor_initialized = 1U;

    printf("\r\n=== Boost 10 kHz Control Ready ===\r\n");
    printf("KEY1: SHUTOFF Toggle, KEY2: Start, KEY3: Stop, KEY4: ClearFault.\r\n");
    AppBoostMonitor_PrintPhaseOffsets();
}

/*!
    \brief      在串口初始化后打印一次 ADC1 三相上电校准偏置
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void AppBoostMonitor_PrintPhaseOffsets(void)
{
    adc_phase_offset_value_t offset;        /*!< L3 保存的三相偏置 raw */
    float phase_a_offset_v;                 /*!< A 相偏置电压 */
    float phase_b_offset_v;                 /*!< B 相偏置电压 */
    float phase_c_offset_v;                 /*!< C 相偏置电压 */

    if (ADCMeasurement_GetPhaseOffsets(&offset) == 0U) {
        printf("adc_phase_offset=INVALID\r\n");
        return;
    }

    phase_a_offset_v = ((float)offset.phase_a_offset_raw *
                        BSP_ADC_REF_VOLTAGE) / BSP_ADC_FULL_SCALE;
    phase_b_offset_v = ((float)offset.phase_b_offset_raw *
                        BSP_ADC_REF_VOLTAGE) / BSP_ADC_FULL_SCALE;
    phase_c_offset_v = ((float)offset.phase_c_offset_raw *
                        BSP_ADC_REF_VOLTAGE) / BSP_ADC_FULL_SCALE;

    printf("adc_phase_offset={a=%u(%.3fV),b=%u(%.3fV),c=%u(%.3fV)}\r\n",
           (unsigned int)offset.phase_a_offset_raw,
           phase_a_offset_v,
           (unsigned int)offset.phase_b_offset_raw,
           phase_b_offset_v,
           (unsigned int)offset.phase_c_offset_raw,
           phase_c_offset_v);
}

/*!
    \brief      按配置周期复制并打印 Boost 运行上下文
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void AppBoostMonitor_Task(void)
{
    boost_control_context_t context; /*!< 本次打印使用的 Boost 上下文副本。 */
    uint32_t current_tick;           /*!< 当前毫秒时基。 */

    if (boost_monitor_initialized == 0U) {
        return;
    }

    current_tick = systick_get_tick();
    if ((uint32_t)(current_tick - boost_monitor_last_print_tick) <
        APP_BOOST_MONITOR_PRINT_PERIOD_MS) {
        return;
    }
    boost_monitor_last_print_tick = current_tick;
    if (BoostControl_GetContext(&context) != 0U) {
        AppBoostMonitor_Print(&context);
    }
}

/*!
    \brief      将 Boost 系统状态转换为串口显示字符串
    \param[in]  state: Boost 系统状态
    \retval     状态名称字符串
*/
static const char *AppBoostMonitor_StateToString(boost_system_state_t state)
{
    switch (state) {
    case BOOST_STATE_IDLE:
        return "IDLE";
    case BOOST_STATE_SOFT_START:
        return "SOFT_START";
    case BOOST_STATE_RUN:
        return "RUN";
    case BOOST_STATE_FAULT:
        return "FAULT";
    default:
        return "UNKNOWN";
    }
}

/*!
    \brief      将 Boost 功率模式转换为串口显示字符串
    \param[in]  mode: Boost 功率控制模式
    \retval     模式名称字符串
*/
static const char *AppBoostMonitor_ModeToString(boost_power_mode_t mode)
{
    switch (mode) {
    case BOOST_POWER_MODE_CV:
        return "CV";
    case BOOST_POWER_MODE_CC:
        return "CC";
    default:
        return "UNKNOWN";
    }
}

/*!
    \brief      通过 USART0 打印 Boost 上下文副本
    \param[in]  context: 主循环已复制的 Boost 上下文
    \param[out] 无
    \retval     无
*/
static void AppBoostMonitor_Print(const boost_control_context_t *context)
{
    if (context == 0) {
        return;
    }

    printf(
        "state=%s(%u) mode=%s(%u) fault=0x%08lX "
        "adc={vout=%.3fV,iout=%.3fA,vin=%.3fV,ia=%.3fA,ib=%.3fA,ic=%.3fA} "
        "duty={a=%.3f%%,b=%.3f%%,c=%.3f%%}\r\n",
        AppBoostMonitor_StateToString(context->state),
        (unsigned int)context->state,
        AppBoostMonitor_ModeToString(context->mode),
        (unsigned int)context->mode,
        (unsigned long)context->fault_flags,
        context->adc.output_voltage_v,
        context->adc.output_current_a,
        context->adc.input_voltage_v,
        context->adc.phase_a_current_a,
        context->adc.phase_b_current_a,
        context->adc.phase_c_current_a,
        context->duty_phase_a_percent,
        context->duty_phase_b_percent,
        context->duty_phase_c_percent);
}
