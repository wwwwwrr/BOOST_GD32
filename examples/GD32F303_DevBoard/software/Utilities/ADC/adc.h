/*!
    \file    adc.h
    \brief   ADC0 监测量与 ADC1 三相电流采样驱动

    \version 2026-7-13, V2.1.0, dual ADC sampling for GD32F30x
*/

#ifndef ADC_H
#define ADC_H

#include "gd32f30x.h"

/* ADC0 循环 DMA 布局：共两帧，每帧三个通道。 */
#define ADC0_DMA_FRAME_COUNT              2U
#define ADC0_DMA_HALF_FRAME_COUNT         (ADC0_DMA_FRAME_COUNT / 2U)
#define ADC0_MONITOR_CHANNEL_COUNT        3U

typedef struct {
    uint16_t output_current_raw;           /*!< PA3 输出电流硬件8倍平均 raw */
    uint16_t output_voltage_raw;           /*!< PA4 输出电压硬件8倍平均 raw */
    uint16_t input_voltage_raw;            /*!< PA5 输入电压硬件8倍平均 raw */
} adc0_frame_raw_t;

typedef struct {
    const volatile adc0_frame_raw_t *frame; /*!< 当前已发布完整快照的只读地址 */
    uint32_t sequence;                     /*!< 当前完整快照的发布序号，不是地址 */
} adc0_snapshot_view_t;

typedef struct {
    uint16_t phase_a_raw;                  /*!< PA0 对应的 A 相电流 raw */
    uint16_t phase_b_raw;                  /*!< PA1 对应的 B 相电流 raw */
    uint16_t phase_c_raw;                  /*!< PA2 对应的 C 相电流 raw */
    uint32_t sequence;                     /*!< 完整三相采样组的发布序号 */
} adc1_phase_raw_t;

typedef enum {
    ADC_STATUS_OK = 0,                     /*!< 操作成功 */
    ADC_STATUS_ERROR,                      /*!< 未分类的底层错误 */
    ADC_STATUS_NOT_INITIALIZED,            /*!< ADC 底层尚未初始化 */
    ADC_STATUS_NOT_READY,                  /*!< 尚无完整采样数据 */
    ADC_STATUS_INVALID_PARAMETER,          /*!< 调用参数无效 */
    ADC_STATUS_DMA_ERROR                   /*!< ADC0 DMA 传输错误 */
} adc_status_t;

/* 配置 ADC0、ADC1、DMA0 通道0和 TIMER0 触发从定时器。 */
void ADC_Init(void);

/* 启动 ADC0 连续采集，并使 ADC1/TIMER0 触发链进入工作状态。 */
void ADC_Start(void);

/* 停止两路 ADC 采集及 TIMER0。 */
void ADC_Stop(void);

/*!
    \brief      获取最新完整 ADC0 硬件过采样快照的只读地址
    \param[out] snapshot: 当前完整快照的只读地址和发布序号
    \retval     ADC_STATUS_OK: 成功
    \retval     其他状态: 未初始化、数据未就绪、参数错误或 DMA 错误
*/
adc_status_t ADC0_GetLatestSnapshot(adc0_snapshot_view_t *snapshot);

/* 返回 ADC0 最近一次完整快照的发布序号。 */
uint32_t ADC0_GetSnapshotSequence(void);

/* 读取 ADC1 最新完整的 A/B/C 三相 raw 采样组。 */
adc_status_t ADC1_GetLatestRaw(adc1_phase_raw_t *result,
                               uint8_t *new_data);

/* 返回 ADC1 最近一次完整三相组的发布序号。 */
uint32_t ADC1_GetSequence(void);

/* 由 gd32f30x_it.c 转发调用的内部中断处理入口。 */
void ADC_DMA_IRQHandler_Internal(void);
void ADC1_IRQHandler_Internal(void);

#endif /* ADC_H */
