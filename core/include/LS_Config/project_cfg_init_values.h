#ifndef PROJECT_CFG_INIT_VALUES_H
#define PROJECT_CFG_INIT_VALUES_H

/*
 * GD32F303 12-bit ADC conversion constants.
 * The converter reports raw counts from 0 to 4095 over 4096 quantization
 * levels.
 */
#define BSP_ADC_REF_VOLTAGE        3.3f
#define BSP_ADC_FULL_SCALE         4096.0f

/* PA4 output-voltage divider: Vout = Vadc_uout * 29.75. */
#define BSP_ADC_UOUT_GAIN          29.75f

/* PA5 input-voltage divider: Vin = Vadc_pow * 5.29. */
#define BSP_ADC_POW_GAIN           5.29f

/* PA3 output-current and PA0/PA1/PA2 phase-current shunts are 5 milliohms. */
#define BSP_ADC_CURRENT_RSENSE     0.005f

/* Iout = Vadc_iout / (10 * Rsense) = Vadc_iout * 20. */
#define BSP_ADC_IOUT_GAIN          (1.0f / (10.0f * BSP_ADC_CURRENT_RSENSE))

/* Iphase = (Vadc_phase - 1.1 V) / (25 * Rsense) = delta-V * 8. */
#define BSP_ADC_PHASE_VREF         1.1f
#define BSP_ADC_PHASE_GAIN         (1.0f / (25.0f * BSP_ADC_CURRENT_RSENSE))

/* Boost 快速控制环固定以 10 kHz（100 us）运行。 */
#define BOOST_CONTROL_FREQUENCY_HZ             10000U

/* 实例 Application 层通过 USART0 打印 Boost 状态的周期。 */
#define APP_BOOST_MONITOR_PRINT_PERIOD_MS      200U

/* Boost 恒压、恒流与首版输出过压保护目标。 */
#define BOOST_OUTPUT_VOLTAGE_SETPOINT_V        30.0f
#define BOOST_OUTPUT_CURRENT_SETPOINT_A        2.0f
#define BOOST_OUTPUT_OVERVOLTAGE_THRESHOLD_V   40.0f

/* 软启动从 12 V 开始，每个 10 kHz 控制周期增加 0.001 V。 */
#define BOOST_SOFT_START_INITIAL_VOLTAGE_V     12.0f
#define BOOST_SOFT_START_STEP_V                0.001f

/* 三相公共占空比及单个控制周期的最大允许变化量，单位均为百分比。 */
#define BOOST_DUTY_MIN_PERCENT                 0.0f
#define BOOST_DUTY_MAX_PERCENT                 70.0f
#define BOOST_DUTY_MAX_STEP_PERCENT            0.1f

/*
 * 电压环和电流环的用户调参值。
 * Ki 直接表示每个 10 kHz 控制周期的系数，不再换算为每秒值。
 * 当前 Kd 为 0，结构体中仅保留该参数供后续扩展。
 */
#define BOOST_VOLTAGE_PI_KP                    0.05f
#define BOOST_VOLTAGE_PI_KI_PER_CYCLE          0.001f
#define BOOST_VOLTAGE_PI_KD                    0.0f
#define BOOST_CURRENT_PI_KP                    1.0f
#define BOOST_CURRENT_PI_KI_PER_CYCLE          0.001f
#define BOOST_CURRENT_PI_KD                    0.0f

#endif /* PROJECT_CFG_INIT_VALUES_H */
