#include "incremental_pi.h"
#include "project_config.h"
#include <stddef.h>

/*!
    \brief      将浮点数限制在指定闭区间内
    \param[in]  value: 待限幅数值
    \param[in]  minimum: 限幅下限
    \param[in]  maximum: 限幅上限
    \return     限幅后的数值
*/
static float IncrementalPI_Clamp(float value,
                                 float minimum,
                                 float maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }

    return value;
}

/*!
    \brief      配置并复位一个增量 PID 控制器
    \param[out] controller: 待初始化的控制器实例
    \param[in]  kp: 比例系数
    \param[in]  ki: 每周期积分系数
    \param[in]  kd: 微分系数，当前配置为 0
    \retval     无
*/
void IncrementalPI_Init(incremental_pi_t *controller,
                        float kp,
                        float ki,
                        float kd)
{
    if (controller == NULL) {
        return;
    }

    controller->kp = kp;
    controller->ki = ki;
    controller->kd = kd;
    IncrementalPI_Reset(controller);
}

/*!
    \brief      清除增量 PID 的积分累计值和上一次误差
    \param[out] controller: 待复位的控制器实例
    \retval     无
*/
void IncrementalPI_Reset(incremental_pi_t *controller)
{
    if (controller == NULL) {
        return;
    }

    controller->integral = 0.0f;
    controller->last_err = 0.0f;
}

/*!
    \brief      按参考算法计算一次已限幅的输出增量
    \param[out] controller: 增量 PID 控制器实例
    \param[in]  error: 当前目标值与反馈值之差
    \return     限制在正负单周期最大步长内的输出增量
*/
float IncrementalPI_Calculate(incremental_pi_t *controller,
                              float error)
{
    float proportional_output; /*!< 本周期误差变化产生的比例增量。 */
    float integral_output;     /*!< 当前误差产生的每周期积分增量。 */
    float delta_output;        /*!< 本周期已限幅的控制输出增量。 */

    if (controller == NULL) {
        return 0.0f;
    }

    proportional_output = controller->kp *
                          (error - controller->last_err);
    integral_output = controller->ki * error;
    controller->last_err = error;

    delta_output = proportional_output + integral_output;
    return IncrementalPI_Clamp(delta_output,
                               -BOOST_DUTY_MAX_STEP_PERCENT,
                               BOOST_DUTY_MAX_STEP_PERCENT);
}
