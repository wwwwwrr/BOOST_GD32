#ifndef LED_H
#define LED_H

/*! \brief LED3 可显示的互斥颜色。 */
typedef enum {
    LED_COLOR_OFF = 0, /*!< LED3 全部熄灭。 */
    LED_COLOR_BLUE,    /*!< LED3 蓝色。 */
    LED_COLOR_GREEN,   /*!< LED3 绿色。 */
    LED_COLOR_RED      /*!< LED3 红色。 */
} led_color_t;

/* 初始化 PC13/PC14/PC15 RGB LED3，并默认全部熄灭。 */
void LED_Init(void);

/* 关闭全部通道后点亮指定颜色，非法颜色按全部熄灭处理。 */
void LED_SetColor(led_color_t color);

#endif /* LED_H */
