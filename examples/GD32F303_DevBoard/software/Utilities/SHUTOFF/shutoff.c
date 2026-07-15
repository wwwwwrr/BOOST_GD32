/*!
    \file    shutoff.c
    \brief   PB12 SHUTOFF 推挽输出驱动
*/

#include "shutoff.h"
#include "gd32f30x.h"

/** \brief SHUTOFF 所在 GPIO 外设。 */
#define SHUTOFF_GPIO_PERIPH             GPIOB
/** \brief SHUTOFF 所在 GPIO 外设时钟。 */
#define SHUTOFF_GPIO_CLOCK              RCU_GPIOB
/** \brief SHUTOFF 使用的 GPIO 引脚。 */
#define SHUTOFF_GPIO_PIN                GPIO_PIN_12

/*!
    \brief      初始化 PB12 为 50 MHz 推挽输出并保持低电平
    \param[in]  无
    \param[out] 无
    \retval     无
    \note       切换为输出模式前先清零输出锁存，避免初始化瞬间产生高电平
*/
void SHUTOFF_Init(void)
{
    rcu_periph_clock_enable(SHUTOFF_GPIO_CLOCK);
    gpio_bit_reset(SHUTOFF_GPIO_PERIPH, SHUTOFF_GPIO_PIN);
    gpio_init(SHUTOFF_GPIO_PERIPH,
              GPIO_MODE_OUT_PP,
              GPIO_OSPEED_50MHZ,
              SHUTOFF_GPIO_PIN);
    gpio_bit_reset(SHUTOFF_GPIO_PERIPH, SHUTOFF_GPIO_PIN);
}

/*!
    \brief      反转 PB12 SHUTOFF 输出电平
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void SHUTOFF_Toggle(void)
{
    if (SET == gpio_output_bit_get(SHUTOFF_GPIO_PERIPH,
                                   SHUTOFF_GPIO_PIN)) {
        gpio_bit_reset(SHUTOFF_GPIO_PERIPH, SHUTOFF_GPIO_PIN);
    } else {
        gpio_bit_set(SHUTOFF_GPIO_PERIPH, SHUTOFF_GPIO_PIN);
    }
}
