#ifndef INCREMENTAL_PI_H
#define INCREMENTAL_PI_H

/*! \brief 增量 PID 参数及历史状态，实例由 L1 持有。 */
typedef struct {
    float kp;          /*!< 比例系数。 */
    float ki;          /*!< 每个控制周期的积分系数。 */
    float kd;          /*!< 微分系数，当前配置为 0 且暂不参与计算。 */
    float integral;    /*!< 积分累计值，当前仅在初始化和复位时清零。 */
    float last_err;    /*!< 上一个控制周期的误差。 */
} incremental_pi_t;

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
                        float kd);

/*!
    \brief      清除增量 PID 的积分累计值和上一次误差
    \param[out] controller: 待复位的控制器实例
    \retval     无
*/
void IncrementalPI_Reset(incremental_pi_t *controller);

/*!
    \brief      按参考算法计算一次已限幅的输出增量
    \param[out] controller: 增量 PID 控制器实例
    \param[in]  error: 当前目标值与反馈值之差
    \return     限制在正负单周期最大步长内的输出增量
*/
float IncrementalPI_Calculate(incremental_pi_t *controller,
                              float error);

#endif /* INCREMENTAL_PI_H */
