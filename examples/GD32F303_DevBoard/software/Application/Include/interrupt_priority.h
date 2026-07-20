#ifndef _INTERRUPT_PRIORITY_H_
#define _INTERRUPT_PRIORITY_H_

/*!
    \file    interrupt_priority.h
    \brief   Centralized interrupt priority definitions

    \version 2026-3-9, V1.0.0, user interrupt priorities
*/

#include "gd32f30x.h"

/* Interrupt priority levels (0-15, where 0 is highest priority) */
#define NVIC_PRIORITY_GROUPING NVIC_PRIGROUP_PRE4_SUB0

/* TIMER4 运行 10 kHz Boost 控制任务，仅允许 ADC1 EOC 中断抢占。 */
#define TIMER4_PRIORITY_GROUP      0
#define TIMER4_PRIORITY_SUBGROUP   0

/* ADC1 EOC must complete before the next 300 kHz TIMER0 trigger. */
#define ADC0_1_PRIORITY_GROUP      3
#define ADC0_1_PRIORITY_SUBGROUP   0

/* ADC0 DMA only copies a completed 32-frame half-buffer. */
#define ADC0_DMA_PRIORITY_GROUP    2
#define ADC0_DMA_PRIORITY_SUBGROUP 0

/* USART interrupt priorities */
#define USART0_PRIORITY_GROUP      3
#define USART0_PRIORITY_SUBGROUP   0

#define USART2_PRIORITY_GROUP      4
#define USART2_PRIORITY_SUBGROUP   0

/* Key EXTI interrupts only set flags and therefore use a low priority. */
#define KEY_EXTI_PRIORITY_GROUP    6
#define KEY_EXTI_PRIORITY_SUBGROUP 0

/* SysTick interrupt priority */
#define SYSTICK_PRIORITY_GROUP     5
#define SYSTICK_PRIORITY_SUBGROUP  0

/* Helper macros for NVIC configuration */
#define NVIC_CONFIG(irqn, pri_group, pri_subgroup) \
    nvic_irq_enable((irqn), (pri_group), (pri_subgroup))

#endif /* _INTERRUPT_PRIORITY_H_ */
