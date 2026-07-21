#ifndef STATUS_INDICATOR_H
#define STATUS_INDICATOR_H

/*! \brief 平台无关的系统状态指示颜色。 */
typedef enum {
    STATUS_INDICATOR_OFF = 0, /*!< 状态灯全部熄灭。 */
    STATUS_INDICATOR_BLUE,    /*!< 状态灯显示蓝色。 */
    STATUS_INDICATOR_GREEN,   /*!< 状态灯显示绿色。 */
    STATUS_INDICATOR_RED      /*!< 状态灯显示红色。 */
} status_indicator_color_t;

/* 初始化状态指示硬件并保持熄灭。 */
void StatusIndicator_Init(void);

/* 设置状态指示颜色；颜色不变时不重复访问底层 GPIO。 */
void StatusIndicator_SetColor(status_indicator_color_t color);

#endif /* STATUS_INDICATOR_H */
