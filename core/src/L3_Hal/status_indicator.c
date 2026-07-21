#include "status_indicator.h"
#include "LED.h"

static status_indicator_color_t status_indicator_current_color =
    STATUS_INDICATOR_OFF; /*!< 最近一次写入底层的状态颜色。 */

/*!
    \brief      初始化状态指示硬件并保持熄灭
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void StatusIndicator_Init(void)
{
    LED_Init();
    status_indicator_current_color = STATUS_INDICATOR_OFF;
}

/*!
    \brief      设置状态指示颜色
    \param[in]  color: 目标状态颜色，非法值按熄灭处理
    \param[out] 无
    \retval     无
*/
void StatusIndicator_SetColor(status_indicator_color_t color)
{
    led_color_t led_color; /*!< 交给实例 LED 驱动的目标颜色。 */

    switch (color) {
    case STATUS_INDICATOR_BLUE:
        led_color = LED_COLOR_BLUE;
        break;

    case STATUS_INDICATOR_GREEN:
        led_color = LED_COLOR_GREEN;
        break;

    case STATUS_INDICATOR_RED:
        led_color = LED_COLOR_RED;
        break;

    case STATUS_INDICATOR_OFF:
    default:
        color = STATUS_INDICATOR_OFF;
        led_color = LED_COLOR_OFF;
        break;
    }

    if (color == status_indicator_current_color) {
        return;
    }

    LED_SetColor(led_color);
    status_indicator_current_color = color;
}
