#ifndef ADC_MEASUREMENT_H
#define ADC_MEASUREMENT_H

#include <stdint.h>

/* 平台无关的 L3 ADC 实际值接口：电压单位为 V，电流单位为 A。 */

typedef enum {
    ADC_MEASUREMENT_STATUS_OK = 0,          /*!< 测量处理成功 */
    ADC_MEASUREMENT_STATUS_ERROR,           /*!< 未分类的测量错误 */
    ADC_MEASUREMENT_STATUS_NOT_INITIALIZED, /*!< 底层 ADC 尚未初始化 */
    ADC_MEASUREMENT_STATUS_NOT_READY,       /*!< 尚无完整测量数据 */
    ADC_MEASUREMENT_STATUS_INVALID_PARAMETER, /*!< 调用参数无效 */
    ADC_MEASUREMENT_STATUS_ACQUISITION_ERROR  /*!< 底层采集或 DMA 错误 */
} adc_measurement_status_t;

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
    uint32_t sequence;                     /*!< 对应的 ADC1 三相组序号 */
} adc_phase_current_value_t;

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
    \brief      处理最新 ADC0 快照并换算输出电流、输出电压和输入电压
    \param[out] result: 实际值结果，ADC0 每个通道已经完成硬件8倍平均
    \param[out] new_data: 本次是否处理了新快照，1 表示有新数据，0 表示无
    \retval     ADC_MEASUREMENT_STATUS_OK: 成功
    \retval     其他状态: 未初始化、数据未就绪、参数错误或采集错误
*/
adc_measurement_status_t ADCMeasurement_ProcessMonitor(
    adc_monitor_value_t *result,
    uint8_t *new_data);

/*!
    \brief      读取最新完整 ADC1 三相采样组并换算为带方向的相电流
    \param[out] result: A、B、C 三相实际电流及对应序号
    \param[out] new_data: 本次是否读取了新三相组，1 表示有新数据，0 表示无
    \retval     ADC_MEASUREMENT_STATUS_OK: 成功
    \retval     其他状态: 未初始化、数据未就绪、参数错误或采集错误
*/
adc_measurement_status_t ADCMeasurement_GetPhaseCurrents(
    adc_phase_current_value_t *result,
    uint8_t *new_data);

#endif /* ADC_MEASUREMENT_H */
