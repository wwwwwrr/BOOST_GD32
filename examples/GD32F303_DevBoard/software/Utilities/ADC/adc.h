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
    uint16_t phase_a_raw;                  /*!< PA0 对应的 A 相电流 raw */
    uint16_t phase_b_raw;                  /*!< PA1 对应的 B 相电流 raw */
    uint16_t phase_c_raw;                  /*!< PA2 对应的 C 相电流 raw */
} adc1_phase_raw_t;

/* 配置 ADC0、ADC1、DMA0 通道0和 TIMER0 触发从定时器。 */
void ADC_Init(void);

/* 启动 ADC0 连续采集，并使 ADC1/TIMER0 触发链进入工作状态。 */
void ADC_Start(void);

/* 停止两路 ADC 采集及 TIMER0。 */
void ADC_Stop(void);

/*!
    \brief      复制最新完整 ADC0 硬件过采样 raw
    \param[out] result: 输出电流、输出电压和输入电压 raw
    \param[out] sequence: 当前完整数据的发布序号
    \retval     1: 已复制有效数据
    \retval     0: 参数无效或尚无完整数据
*/
uint8_t ADC0_GetLatestRaw(adc0_frame_raw_t *result, uint32_t *sequence);

/* 读取 ADC1 的 A/B/C 各相最新 raw；三相均有效时返回1。 */
uint8_t ADC1_GetLatestRaw(adc1_phase_raw_t *result);

/* 由 gd32f30x_it.c 转发调用的内部中断处理入口。 */
void ADC_DMA_IRQHandler_Internal(void);
void ADC1_IRQHandler_Internal(void);

#endif /* ADC_H */
