#include "adc_measurement.h"
#include "project_config.h"
#include "adc.h"
#include <stddef.h>

static float ADCMeasurement_RawToAdcVoltage(uint16_t raw);
static float ADCMeasurement_RawToPhaseCurrent(uint16_t raw);
static adc_measurement_status_t ADCMeasurement_MapStatus(adc_status_t status);
static void ADCMeasurement_ResetMonitorCache(void);

/* 最近一次成功换算并提交的 ADC0 监测实际值缓存。 */
static adc_monitor_value_t adc0_latest_value;
/* 最近一次成功处理的 ADC0 完整快照发布序号。 */
static uint32_t adc0_last_processed_sequence = 0U;
/* ADC0 监测实际值缓存有效标志：1 表示可返回缓存值，0 表示尚未就绪。 */
static uint8_t adc0_value_valid = 0U;

/*!
    \brief      初始化 ADC 实际值测量模块
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void ADCMeasurement_Init(void)
{
    ADCMeasurement_ResetMonitorCache();
    ADC_Init();
}

/*!
    \brief      启动底层双 ADC 采集
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void ADCMeasurement_Start(void)
{
    ADCMeasurement_ResetMonitorCache();
    ADC_Start();
}

/*!
    \brief      停止底层双 ADC 采集
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void ADCMeasurement_Stop(void)
{
    ADC_Stop();
}

/*!
    \brief      处理 ADC0 最新硬件8倍平均 raw 并换算监测实际值
    \param[out] result: 输出电流、输出电压、输入电压及快照序号
    \param[out] new_data: 新快照标志
    \retval     ADC 实际值测量状态
*/
adc_measurement_status_t ADCMeasurement_ProcessMonitor(
    adc_monitor_value_t *result,
    uint8_t *new_data)
{
    adc0_snapshot_view_t snapshot;          /*!< 底层返回的当前 ADC0 快照视图 */
    adc_monitor_value_t next_value;         /*!< 本次换算完成但尚未提交的实际值 */
    adc_status_t status;                    /*!< 底层 ADC 快照读取状态 */
    float adc_voltage;                      /*!< 当前 raw 对应的 ADC 引脚电压 */
    uint16_t output_current_raw;            /*!< 从快照读取的输出电流 raw */
    uint16_t output_voltage_raw;            /*!< 从快照读取的输出电压 raw */
    uint16_t input_voltage_raw;             /*!< 从快照读取的输入电压 raw */

    if ((result == NULL) || (new_data == NULL)) {
        return ADC_MEASUREMENT_STATUS_INVALID_PARAMETER;
    }

    *new_data = 0U;
    status = ADC0_GetLatestSnapshot(&snapshot);
    if (status != ADC_STATUS_OK) {
        return ADCMeasurement_MapStatus(status);
    }

    if (snapshot.sequence == adc0_last_processed_sequence) {
        if (adc0_value_valid == 0U) {
            return ADC_MEASUREMENT_STATUS_NOT_READY;
        }
        *result = adc0_latest_value;
        return ADC_MEASUREMENT_STATUS_OK;
    }

    output_current_raw = snapshot.frame->output_current_raw;
    output_voltage_raw = snapshot.frame->output_voltage_raw;
    input_voltage_raw = snapshot.frame->input_voltage_raw;

    adc_voltage = ADCMeasurement_RawToAdcVoltage(output_current_raw);
    next_value.output_current_a = adc_voltage * BSP_ADC_IOUT_GAIN;

    adc_voltage = ADCMeasurement_RawToAdcVoltage(output_voltage_raw);
    next_value.output_voltage_v = adc_voltage * BSP_ADC_UOUT_GAIN;

    adc_voltage = ADCMeasurement_RawToAdcVoltage(input_voltage_raw);
    next_value.input_voltage_v = adc_voltage * BSP_ADC_POW_GAIN;
    next_value.sequence = snapshot.sequence;

    adc0_latest_value = next_value;
    adc0_last_processed_sequence = snapshot.sequence;
    adc0_value_valid = 1U;
    *result = adc0_latest_value;
    *new_data = 1U;

    return ADC_MEASUREMENT_STATUS_OK;
}

/*!
    \brief      读取 ADC1 最新完整三相 raw 并换算三相实际电流
    \param[out] result: 三相电流及三相组序号
    \param[out] new_data: 新三相组标志
    \retval     ADC 实际值测量状态
*/
adc_measurement_status_t ADCMeasurement_GetPhaseCurrents(
    adc_phase_current_value_t *result,
    uint8_t *new_data)
{
    adc1_phase_raw_t raw;                   /*!< ADC1 最新完整三相 raw 采样组 */
    adc_status_t status;                    /*!< 底层 ADC1 三相组读取状态 */

    if ((result == NULL) || (new_data == NULL)) {
        return ADC_MEASUREMENT_STATUS_INVALID_PARAMETER;
    }

    *new_data = 0U;
    status = ADC1_GetLatestRaw(&raw, new_data);
    if (status != ADC_STATUS_OK) {
        return ADCMeasurement_MapStatus(status);
    }

    result->phase_a_current_a =
        ADCMeasurement_RawToPhaseCurrent(raw.phase_a_raw);
    result->phase_b_current_a =
        ADCMeasurement_RawToPhaseCurrent(raw.phase_b_raw);
    result->phase_c_current_a =
        ADCMeasurement_RawToPhaseCurrent(raw.phase_c_raw);
    result->sequence = raw.sequence;

    return ADC_MEASUREMENT_STATUS_OK;
}

/*!
    \brief      将 12 位 ADC raw 换算为 ADC 引脚电压
    \param[in]  raw: ADC 原始计数，范围为 0～4095
    \param[out] 无
    \retval     ADC 引脚电压，单位 V
*/
static float ADCMeasurement_RawToAdcVoltage(uint16_t raw)
{
    return ((float)raw * BSP_ADC_REF_VOLTAGE) / BSP_ADC_FULL_SCALE;
}

/*!
    \brief      将相电流 ADC raw 换算为带方向的实际电流
    \param[in]  raw: 相电流 ADC 原始计数，范围为 0～4095
    \param[out] 无
    \retval     相电流，单位 A，允许返回负值
*/
static float ADCMeasurement_RawToPhaseCurrent(uint16_t raw)
{
    float adc_voltage;                      /*!< 相电流 raw 对应的 ADC 引脚电压 */

    adc_voltage = ADCMeasurement_RawToAdcVoltage(raw);
    return (adc_voltage - BSP_ADC_PHASE_VREF) * BSP_ADC_PHASE_GAIN;
}

/*!
    \brief      将底层 ADC 状态映射为平台无关的实际值测量状态
    \param[in]  status: 底层 ADC 状态
    \param[out] 无
    \retval     对应的 ADC 实际值测量状态
*/
static adc_measurement_status_t ADCMeasurement_MapStatus(adc_status_t status)
{
    adc_measurement_status_t measurement_status; /*!< 映射后的 L3 测量状态 */

    switch (status) {
    case ADC_STATUS_OK:
        measurement_status = ADC_MEASUREMENT_STATUS_OK;
        break;
    case ADC_STATUS_NOT_INITIALIZED:
        measurement_status = ADC_MEASUREMENT_STATUS_NOT_INITIALIZED;
        break;
    case ADC_STATUS_NOT_READY:
        measurement_status = ADC_MEASUREMENT_STATUS_NOT_READY;
        break;
    case ADC_STATUS_INVALID_PARAMETER:
        measurement_status = ADC_MEASUREMENT_STATUS_INVALID_PARAMETER;
        break;
    case ADC_STATUS_DMA_ERROR:
        measurement_status = ADC_MEASUREMENT_STATUS_ACQUISITION_ERROR;
        break;
    case ADC_STATUS_ERROR:
    default:
        measurement_status = ADC_MEASUREMENT_STATUS_ERROR;
        break;
    }

    return measurement_status;
}

/*!
    \brief      清除 ADC0 上层最后有效实际值和已处理序号
    \param[in]  无
    \param[out] 无
    \retval     无
*/
static void ADCMeasurement_ResetMonitorCache(void)
{
    adc0_latest_value.output_current_a = 0.0f;
    adc0_latest_value.output_voltage_v = 0.0f;
    adc0_latest_value.input_voltage_v = 0.0f;
    adc0_latest_value.sequence = 0U;
    adc0_last_processed_sequence = 0U;
    adc0_value_valid = 0U;
}
