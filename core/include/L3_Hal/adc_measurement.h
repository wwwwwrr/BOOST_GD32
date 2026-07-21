#ifndef ADC_MEASUREMENT_H
#define ADC_MEASUREMENT_H

#include <stdint.h>

/* 平台无关的 L3 ADC 实际值接口：电压单位为 V，电流单位为 A。 */

typedef struct {
    float output_current_a;                /*!< 输出电流，单位 A */
    float output_voltage_v;                /*!< 输出电压，单位 V */
    float input_voltage_v;                 /*!< 输入电压，单位 V */
    uint32_t sequence;                     /*!< 对应的 ADC0 快照序号 */
} adc_monitor_value_t;

typedef struct {
    float phase_a_current_a;               /*!< A 相电流，单位 A */
    float phase_b_current_a;               /*!< B 相电流，单位 A */
    float phase_c_current_a;               /*!< C 相电流，单位 A */
} adc_phase_current_value_t;

typedef struct {
    uint16_t phase_a_offset_raw;            /*!< A 相零电流偏置 raw */
    uint16_t phase_b_offset_raw;            /*!< B 相零电流偏置 raw */
    uint16_t phase_c_offset_raw;            /*!< C 相零电流偏置 raw */
} adc_phase_offset_value_t;

/*!
    \brief      使用独立 ADC1 轮询流程校准三相零电流偏置
    \param[in]  无
    \param[out] 无
    \retval     1: 三相偏置已校准并保存
    \retval     0: 校准失败，偏置保持无效
*/
uint8_t ADCMeasurement_CalibratePhaseOffsets(void);

/* 返回三相偏置是否已经成功校准。 */
uint8_t ADCMeasurement_IsPhaseOffsetReady(void);

/* 复制已校准的三相偏置 raw；成功返回1，未校准或参数为空返回0。 */
uint8_t ADCMeasurement_GetPhaseOffsets(adc_phase_offset_value_t *result);

/*!
    \brief      初始化 ADC 实际值测量模块及底层双 ADC 采集通路
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void ADCMeasurement_Init(void);

/*!
    \brief      启动 ADC0 DMA 采集和 ADC1 定时器触发采集
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void ADCMeasurement_Start(void);

/*!
    \brief      停止 ADC0、ADC1、DMA 及 ADC1 触发定时器
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void ADCMeasurement_Stop(void);

/*! 
    \brief      换算最新 ADC0 raw，无新值时返回上一次换算结果
    \param[out] result: 实际值结果，首帧前返回初始零值
    \retval     无
*/
void ADCMeasurement_ProcessMonitor(adc_monitor_value_t *result);

/*!
    \brief      换算 ADC1 的 A/B/C 各相最新 raw
    \param[out] result: A、B、C 三相实际电流，三相首次有效前为零
    \retval     无
*/
void ADCMeasurement_GetPhaseCurrents(adc_phase_current_value_t *result);

#endif /* ADC_MEASUREMENT_H */
