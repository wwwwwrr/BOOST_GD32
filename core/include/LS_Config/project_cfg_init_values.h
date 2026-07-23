#ifndef PROJECT_CFG_INIT_VALUES_H
#define PROJECT_CFG_INIT_VALUES_H

/*
 * GD32F303 12-bit ADC conversion constants.
 * The converter reports raw counts from 0 to 4095 over 4096 quantization
 * levels.
 */
#define BSP_ADC_REF_VOLTAGE        3.3f
#define BSP_ADC_FULL_SCALE         4096.0f

/* PA4 output-voltage divider: Vout = Vadc_uout * 26.64. */
#define BSP_ADC_UOUT_GAIN          26.64f

/* PA5 input-voltage divider: Vin = Vadc_pow * 5.29. */
#define BSP_ADC_POW_GAIN           5.29f

/* PA3 output-current shunt is 10 milliohms; phase-current shunts are 5 milliohms. */
#define BSP_ADC_IOUT_RSENSE        0.010f
#define BSP_ADC_CURRENT_RSENSE     0.005f

/* Iout amplifier: Rfeedback / Rinput = 100 kOhm / 3.9 kOhm. */
#define BSP_ADC_IOUT_AMP_GAIN      (100.0f / 3.9f)

/* Iout = Vadc_iout / (BSP_ADC_IOUT_AMP_GAIN * BSP_ADC_IOUT_RSENSE) = Vadc_iout * 3.9. */
#define BSP_ADC_IOUT_GAIN          (1.0f / (BSP_ADC_IOUT_AMP_GAIN * BSP_ADC_IOUT_RSENSE))

/* Iphase = delta-V / (25.64 * Rsense) = delta-V * 3.9. */
#define BSP_ADC_PHASE_GAIN         (1.0f / (BSP_ADC_IOUT_AMP_GAIN * BSP_ADC_CURRENT_RSENSE))

/* ADC1 上电偏置校准：先丢弃8轮，再对每相64个样本求平均。 */
#define BSP_ADC_PHASE_OFFSET_DISCARD_ROUNDS   8U
#define BSP_ADC_PHASE_OFFSET_SAMPLE_ROUNDS    64U
#define BSP_ADC_PHASE_OFFSET_TIMEOUT_MS       2U

/* Boost 快速控制环固定以 10 kHz（100 us）运行。 */
#define BOOST_CONTROL_FREQUENCY_HZ             10000U

/* 实例 Application 层通过 USART0 打印 Boost 状态的周期。 */
#define APP_BOOST_MONITOR_PRINT_PERIOD_MS      200U

/* Boost 恒压、恒流与首版输出过压保护目标。 */
#define BOOST_OUTPUT_VOLTAGE_SETPOINT_V        30.0f
#define BOOST_OUTPUT_CURRENT_SETPOINT_A        1.0f
#define BOOST_OUTPUT_OVERVOLTAGE_THRESHOLD_V   70.0f
#define BOOST_OUTPUT_OVERCURRENT_THRESHOLD_V   3.0f
//电压调节
#define BOOST_OUTPUT_VOLTAGE_ADJUST_STEP_V      10.0f
#define BOOST_OUTPUT_VOLTAGE_SETPOINT_MIN_V     12.0f
#define BOOST_OUTPUT_VOLTAGE_SETPOINT_MAX_V     65.0f
   
//电流调节
#define BOOST_OUTPUT_CURRENT_ADJUST_STEP_A       0.1f
#define BOOST_OUTPUT_CURRENT_SETPOINT_MIN_A      0.2f
#define BOOST_OUTPUT_CURRENT_SETPOINT_MAX_A      2.0f

/* Boost 静态开路与运行中断开保护参数。 */
#define BOOST_OPEN_LOAD_CURRENT_THRESHOLD_A       0.09f
#define BOOST_LOAD_PRESENT_CURRENT_THRESHOLD_A    0.15f
#define BOOST_STATIC_OPEN_ARM_VOLTAGE_V            16.0f
#define BOOST_STATIC_OPEN_CONFIRM_TIME_MS          5.0f
#define BOOST_RUNTIME_OPEN_CONFIRM_TIME_MS         0.5f

/* 将毫秒配置换算为 10 kHz 控制周期数。 */
#define BOOST_CONTROL_TIME_MS_TO_CYCLES(time_ms)                      \
    ((uint16_t)((((time_ms) * (float)BOOST_CONTROL_FREQUENCY_HZ) /   \
                  1000.0f) ))

#define BOOST_STATIC_OPEN_CONFIRM_CYCLES                            \
    BOOST_CONTROL_TIME_MS_TO_CYCLES(BOOST_STATIC_OPEN_CONFIRM_TIME_MS)

#define BOOST_RUNTIME_OPEN_CONFIRM_CYCLES                           \
    BOOST_CONTROL_TIME_MS_TO_CYCLES(BOOST_RUNTIME_OPEN_CONFIRM_TIME_MS)



/* 软启动从 12 V 开始，每个 10 kHz 控制周期增加 0.002 V。 */
#define BOOST_SOFT_START_INITIAL_VOLTAGE_V     12.0f
#define BOOST_SOFT_START_STEP_V                0.002f

/* 三相公共占空比及单个控制周期的最大允许变化量，单位均为百分比。 */
#define BOOST_DUTY_MIN_PERCENT                 0.0f
#define BOOST_DUTY_MAX_PERCENT                 90.0f
#define BOOST_DUTY_MAX_STEP_PERCENT            0.1f

/*
 * 电压环和电流环的用户调参值。
 * Ki 直接表示每个 10 kHz 控制周期的系数，不再换算为每秒值。
 * 当前 Kd 为 0，结构体中仅保留该参数供后续扩展。
 */
#define BOOST_VOLTAGE_PI_KP                    0.6f
#define BOOST_VOLTAGE_PI_KI_PER_CYCLE          0.05f
#define BOOST_VOLTAGE_PI_KD                    0.0f
#define BOOST_CURRENT_PI_KP                    2.5f
#define BOOST_CURRENT_PI_KI_PER_CYCLE          0.8f
#define BOOST_CURRENT_PI_KD                    0.0f

#endif /* PROJECT_CFG_INIT_VALUES_H */
