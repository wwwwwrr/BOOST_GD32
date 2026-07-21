/*!
    \file    LED.c
    \brief   PC13/PC14/PC15 共地 RGB LED3 驱动
*/

#include "LED.h"
#include "gd32f30x.h"

#define LED_GPIO_PERIPH      GPIOC
#define LED_GPIO_CLOCK       RCU_GPIOC
#define LED_BLUE_GPIO_PIN    GPIO_PIN_15
#define LED_GREEN_GPIO_PIN   GPIO_PIN_14
#define LED_RED_GPIO_PIN     GPIO_PIN_13
#define LED_ALL_GPIO_PINS    (LED_BLUE_GPIO_PIN | \
                              LED_GREEN_GPIO_PIN | \
                              LED_RED_GPIO_PIN)

/*!
    \brief      初始化 RGB LED3 并保持全部熄灭
    \param[in]  无
    \param[out] 无
    \retval     无
    \note       切换为输出模式前先清零输出锁存，避免初始化闪烁
*/
void LED_Init(void)
{
    rcu_periph_clock_enable(LED_GPIO_CLOCK);
    gpio_bit_reset(LED_GPIO_PERIPH, LED_ALL_GPIO_PINS);
    gpio_init(LED_GPIO_PERIPH,
              GPIO_MODE_OUT_PP,
              GPIO_OSPEED_2MHZ,
              LED_ALL_GPIO_PINS);
    gpio_bit_reset(LED_GPIO_PERIPH, LED_ALL_GPIO_PINS);
}

/*!
    \brief      互斥设置 RGB LED3 显示颜色
    \param[in]  color: 目标颜色，非法值按全部熄灭处理
    \param[out] 无
    \retval     无
*/
void LED_SetColor(led_color_t color)
{
    gpio_bit_reset(LED_GPIO_PERIPH, LED_ALL_GPIO_PINS);

    switch (color) {
    case LED_COLOR_BLUE:
        gpio_bit_set(LED_GPIO_PERIPH, LED_BLUE_GPIO_PIN);
        break;

    case LED_COLOR_GREEN:
        gpio_bit_set(LED_GPIO_PERIPH, LED_GREEN_GPIO_PIN);
        break;

    case LED_COLOR_RED:
        gpio_bit_set(LED_GPIO_PERIPH, LED_RED_GPIO_PIN);
        break;

    case LED_COLOR_OFF:
    default:
        break;
    }
}
