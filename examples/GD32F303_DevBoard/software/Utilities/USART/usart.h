#ifndef _USART_H_
#define _USART_H_

/*!
    \file    usart.h
    \brief   USART module for serial communication

    \version 2026-7-15, V1.1.0, USART0 communication implementation
*/

#include "gd32f30x.h"
#include "interrupt_priority.h"
#include <string.h>

/* USART0 默认映射：PA9 TX、PA10 RX，对应板载 CH340N。 */
#define USART0_PERIPH          USART0
#define USART0_RCU             RCU_USART0
#define USART0_GPIO_RCU        RCU_GPIOA
#define USART0_GPIO            GPIOA
#define USART0_TX_PIN          GPIO_PIN_9
#define USART0_RX_PIN          GPIO_PIN_10
#define USART0_IRQN            USART0_IRQn

/* USART configuration */
#define USART0_BAUDRATE        115200U
#define USART0_WORD_LENGTH     USART_WL_8BIT
#define USART0_STOP_BITS       USART_STB_1BIT
#define USART0_PARITY          USART_PM_NONE
#define USART0_HARDWARE_FLOW   USART_RTS_DISABLE

/* Buffer sizes */
#define USART0_RX_BUFFER_SIZE  128
#define USART0_TX_BUFFER_SIZE  128

/* USART status flags */
typedef enum {
    USART_STATUS_OK = 0,
    USART_STATUS_BUSY,
    USART_STATUS_ERROR,
    USART_STATUS_BUFFER_FULL
} usart_status_t;

/* Function prototypes */
void USART0_Init(void);
void USART0_LoopbackEnable(void);
void USART0_LoopbackDisable(void);
usart_status_t USART0_SendByte(uint8_t data);
usart_status_t USART0_SendString(const char *str);
uint8_t USART0_ReceiveByte(void);
uint8_t USART0_IsDataAvailable(void);
void USART0_ClearBuffers(void);

/* Interrupt callback type */
typedef void (*usart_rx_callback_t)(uint8_t data);

/* Callback registration */
void USART0_SetRxCallback(usart_rx_callback_t callback);

/* Interrupt handler implementation called by gd32f30x_it.c. */
void USART0_IRQHandler_Internal(void);

#endif /* _USART_H_ */
