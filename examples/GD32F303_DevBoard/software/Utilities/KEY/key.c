/**
 * \file key.c
 * \brief 四路低电平有效按键的 EXTI 采集与非阻塞消抖实现。
 *
 * 中断服务只清除 EXTI 标志并记录待处理位；按下、松开消抖以及
 * 回调执行均由主循环中的 KEY_Task() 完成。
 */

#include "key.h"
#include "gd32f30x.h"
#include "interrupt_priority.h"
#include "systick.h"
#include <stddef.h>

/** \brief KEY1 所在的 GPIO 外设。 */
#define KEY1_GPIO_PERIPH                 GPIOA
/** \brief KEY1 的 GPIO 引脚，低电平表示按下。 */
#define KEY1_GPIO_PIN                    GPIO_PIN_8
/** \brief KEY1 对应的 EXTI 线。 */
#define KEY1_EXTI_LINE                   EXTI_8

/** \brief KEY2 所在的 GPIO 外设。 */
#define KEY2_GPIO_PERIPH                 GPIOB
/** \brief KEY2 的 GPIO 引脚，低电平表示按下。 */
#define KEY2_GPIO_PIN                    GPIO_PIN_15
/** \brief KEY2 对应的 EXTI 线。 */
#define KEY2_EXTI_LINE                   EXTI_15

/** \brief KEY3 所在的 GPIO 外设。 */
#define KEY3_GPIO_PERIPH                 GPIOB
/** \brief KEY3 的 GPIO 引脚，低电平表示按下。 */
#define KEY3_GPIO_PIN                    GPIO_PIN_14
/** \brief KEY3 对应的 EXTI 线。 */
#define KEY3_EXTI_LINE                   EXTI_14

/** \brief KEY4 所在的 GPIO 外设。 */
#define KEY4_GPIO_PERIPH                 GPIOB
/** \brief KEY4 的 GPIO 引脚，低电平表示按下。 */
#define KEY4_GPIO_PIN                    GPIO_PIN_13
/** \brief KEY4 对应的 EXTI 线。 */
#define KEY4_EXTI_LINE                   EXTI_13

/**
 * \brief 将按键编号转换为待处理标志位。
 * \param key 按键编号，取值见 key_id_t。
 * \return 对应的 32 位待处理掩码。
 */
#define KEY_PENDING_BIT(key)             (1UL << (uint32_t)(key))

/**
 * \brief 单路按键的非阻塞消抖状态。
 */
typedef enum {
    KEY_STATE_RELEASED = 0,              /**< 稳定松开状态。 */
    KEY_STATE_PRESS_DEBOUNCE,             /**< 按下消抖计时状态。 */
    KEY_STATE_PRESSED,                    /**< 稳定按下状态。 */
    KEY_STATE_RELEASE_DEBOUNCE            /**< 松开消抖计时状态。 */
} key_state_t;

/**
 * \brief 单路按键的运行时上下文。
 */
typedef struct {
    uint32_t gpio_periph;                 /**< 按键所在的 GPIO 外设基址。 */
    uint32_t gpio_pin;                    /**< 按键引脚位掩码。 */
    uint32_t state_timestamp;             /**< 当前消抖阶段的起始时刻，单位：ms。 */
    key_callback_t callback;              /**< 确认按下后执行的用户回调。 */
    key_state_t state;                    /**< 当前按键消抖状态。 */
} key_runtime_t;

/** \brief 四路按键的 GPIO 映射、回调和消抖运行数据。 */
static key_runtime_t key_runtime[KEY_ID_COUNT] = {
    {KEY1_GPIO_PERIPH, KEY1_GPIO_PIN, 0U, NULL, KEY_STATE_RELEASED},
    {KEY2_GPIO_PERIPH, KEY2_GPIO_PIN, 0U, NULL, KEY_STATE_RELEASED},
    {KEY3_GPIO_PERIPH, KEY3_GPIO_PIN, 0U, NULL, KEY_STATE_RELEASED},
    {KEY4_GPIO_PERIPH, KEY4_GPIO_PIN, 0U, NULL, KEY_STATE_RELEASED}
};

/** \brief EXTI ISR 置位、KEY_Task() 原子取走的待处理按键位集。 */
static volatile uint32_t key_pending_flags = 0U;
/** \brief KEY 模块已完成初始化的标志，1 表示 KEY_Task() 可运行。 */
static uint8_t key_initialized = 0U;

/**
 * \brief 在短临界区内取走全部待处理按键标志。
 * \return 本次 KEY_Task() 需处理的按键位集。
 * \note 函数保留进入前的全局中断状态，仅在复制并清零标志时短暂关中断。
 */
static uint32_t KEY_TakePendingFlags(void);

/**
 * \brief 读取指定按键的即时电平并判断是否按下。
 * \param key 需读取的按键编号。
 * \return 引脚为低电平时返回 1，否则返回 0。
 */
static uint8_t KEY_PinIsPressed(key_id_t key);

/**
 * \brief 执行单路按键的非阻塞按下/松开消抖状态机。
 * \param key 需处理的按键编号。
 * \param pending_flags 本轮从 ISR 取得的待处理按键位集。
 * \param current_tick 当前系统毫秒计数，单位：ms。
 * \return 无。
 */
static void KEY_ProcessOne(key_id_t key,
                           uint32_t pending_flags,
                           uint32_t current_tick);

/**
 * \brief 初始化四路按键 GPIO、下降沿 EXTI、运行数据和 NVIC。
 * \return 无。
 * \note 该函数不会执行用户回调，初始化后需在主循环周期调用 KEY_Task()。
 */
void KEY_Init(void)
{
    uint32_t key_index;                  /**< 遍历按键运行数组的索引。 */

    key_initialized = 0U;
    key_pending_flags = 0U;

    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_AF);

    gpio_init(GPIOA, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, GPIO_PIN_8);
    gpio_init(GPIOB, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ,
              GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);

    gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOA, GPIO_PIN_SOURCE_8);
    gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOB, GPIO_PIN_SOURCE_15);
    gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOB, GPIO_PIN_SOURCE_14);
    gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOB, GPIO_PIN_SOURCE_13);

    exti_init(KEY1_EXTI_LINE, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    exti_init(KEY2_EXTI_LINE, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    exti_init(KEY3_EXTI_LINE, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    exti_init(KEY4_EXTI_LINE, EXTI_INTERRUPT, EXTI_TRIG_FALLING);

    exti_interrupt_flag_clear(KEY1_EXTI_LINE);
    exti_interrupt_flag_clear(KEY2_EXTI_LINE);
    exti_interrupt_flag_clear(KEY3_EXTI_LINE);
    exti_interrupt_flag_clear(KEY4_EXTI_LINE);

    for (key_index = 0U; key_index < (uint32_t)KEY_ID_COUNT; key_index++) {
        key_runtime[key_index].state_timestamp = 0U;
        key_runtime[key_index].callback = NULL;
        key_runtime[key_index].state = KEY_STATE_RELEASED;
    }

    NVIC_CONFIG(EXTI5_9_IRQn,
                KEY_EXTI_PRIORITY_GROUP,
                KEY_EXTI_PRIORITY_SUBGROUP);
    NVIC_CONFIG(EXTI10_15_IRQn,
                KEY_EXTI_PRIORITY_GROUP,
                KEY_EXTI_PRIORITY_SUBGROUP);

    key_initialized = 1U;
}

/**
 * \brief 为指定按键注册按下确认回调。
 * \param key 需注册回调的按键编号。
 * \param callback 稳定按下后在 KEY_Task() 上下文执行的回调，可为 NULL。
 * \return 无。
 */
void KEY_SetCallback(key_id_t key, key_callback_t callback)
{
    if ((uint32_t)key >= (uint32_t)KEY_ID_COUNT) {
        return;
    }

    key_runtime[key].callback = callback;
}

/**
 * \brief 在主循环中处理四路按键的 20 ms 非阻塞消抖和回调。
 * \return 无。
 * \note 本函数不应在 ISR 中调用，且必须由主循环持续调度。
 */
void KEY_Task(void)
{
    uint32_t pending_flags;              /**< 本轮原子取得的 EXTI 待处理按键位集。 */
    uint32_t current_tick;               /**< 本轮状态机共用的系统时刻，单位：ms。 */
    uint32_t key_index;                  /**< 遍历四路按键的索引。 */

    if (key_initialized == 0U) {
        return;
    }

    pending_flags = KEY_TakePendingFlags();
    current_tick = systick_get_tick();

    for (key_index = 0U; key_index < (uint32_t)KEY_ID_COUNT; key_index++) {
        KEY_ProcessOne((key_id_t)key_index, pending_flags, current_tick);
    }
}

/**
 * \brief 处理 KEY1 所在的 EXTI5_9 共享中断。
 * \return 无。
 * \note ISR 只清除硬件标志并置待处理位，禁止在此消抖、阻塞或执行用户回调。
 */
void KEY_EXTI5_9_IRQHandler_Internal(void)
{
    if (SET == exti_interrupt_flag_get(KEY1_EXTI_LINE)) {
        exti_interrupt_flag_clear(KEY1_EXTI_LINE);
        key_pending_flags |= KEY_PENDING_BIT(KEY_ID_1);
    }
}

/**
 * \brief 处理 KEY2、KEY3 和 KEY4 所在的 EXTI10_15 共享中断。
 * \return 无。
 * \note ISR 只清除硬件标志并置待处理位，禁止在此消抖、阻塞或执行用户回调。
 */
void KEY_EXTI10_15_IRQHandler_Internal(void)
{
    if (SET == exti_interrupt_flag_get(KEY2_EXTI_LINE)) {
        exti_interrupt_flag_clear(KEY2_EXTI_LINE);
        key_pending_flags |= KEY_PENDING_BIT(KEY_ID_2);
    }
    if (SET == exti_interrupt_flag_get(KEY3_EXTI_LINE)) {
        exti_interrupt_flag_clear(KEY3_EXTI_LINE);
        key_pending_flags |= KEY_PENDING_BIT(KEY_ID_3);
    }
    if (SET == exti_interrupt_flag_get(KEY4_EXTI_LINE)) {
        exti_interrupt_flag_clear(KEY4_EXTI_LINE);
        key_pending_flags |= KEY_PENDING_BIT(KEY_ID_4);
    }
}

/**
 * \brief 在短临界区内取走全部待处理按键标志。
 * \return 本次 KEY_Task() 需处理的按键位集。
 * \note 函数保留进入前的全局中断状态，仅在复制并清零标志时短暂关中断。
 */
static uint32_t KEY_TakePendingFlags(void)
{
    uint32_t interrupt_state;            /**< 进入临界区前的 PRIMASK 全局中断状态。 */
    uint32_t pending_flags;              /**< 从共享标志原子取得的按键位集。 */

    interrupt_state = __get_PRIMASK();
    __disable_irq();
    pending_flags = key_pending_flags;
    key_pending_flags = 0U;
    if (interrupt_state == 0U) {
        __enable_irq();
    }

    return pending_flags;
}

/**
 * \brief 读取指定按键的即时电平并判断是否按下。
 * \param key 需读取的按键编号。
 * \return 引脚为低电平时返回 1，否则返回 0。
 */
static uint8_t KEY_PinIsPressed(key_id_t key)
{
    return (uint8_t)(RESET == gpio_input_bit_get(key_runtime[key].gpio_periph,
                                                 key_runtime[key].gpio_pin));
}

/**
 * \brief 执行单路按键的非阻塞按下/松开消抖状态机。
 * \param key 需处理的按键编号。
 * \param pending_flags 本轮从 ISR 取得的待处理按键位集。
 * \param current_tick 当前系统毫秒计数，单位：ms。
 * \return 无。
 */
static void KEY_ProcessOne(key_id_t key,
                           uint32_t pending_flags,
                           uint32_t current_tick)
{
    key_runtime_t *runtime;              /**< 当前处理按键的运行时上下指针。 */

    runtime = &key_runtime[key];

    switch (runtime->state) {
    case KEY_STATE_RELEASED:
        if ((pending_flags & KEY_PENDING_BIT(key)) != 0U) {
            runtime->state_timestamp = current_tick;
            runtime->state = KEY_STATE_PRESS_DEBOUNCE;
        }
        break;

    case KEY_STATE_PRESS_DEBOUNCE:
        if ((current_tick - runtime->state_timestamp) >=
            KEY_DEBOUNCE_TIME_MS) {
            if (KEY_PinIsPressed(key) != 0U) {
                runtime->state = KEY_STATE_PRESSED;
                if (runtime->callback != NULL) {
                    runtime->callback();
                }
            } else {
                runtime->state = KEY_STATE_RELEASED;
            }
        }
        break;

    case KEY_STATE_PRESSED:
        if (KEY_PinIsPressed(key) == 0U) {
            runtime->state_timestamp = current_tick;
            runtime->state = KEY_STATE_RELEASE_DEBOUNCE;
        }
        break;

    case KEY_STATE_RELEASE_DEBOUNCE:
        if (KEY_PinIsPressed(key) != 0U) {
            runtime->state = KEY_STATE_PRESSED;
        } else if ((current_tick - runtime->state_timestamp) >=
                   KEY_DEBOUNCE_TIME_MS) {
            runtime->state = KEY_STATE_RELEASED;
        }
        break;

    default:
        runtime->state = KEY_STATE_RELEASED;
        break;
    }
}
