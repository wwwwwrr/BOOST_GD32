#ifndef SHUTOFF_H
#define SHUTOFF_H

/*!
    \file    shutoff.h
    \brief   PB12 SHUTOFF 输出控制接口
*/

/*!
    \brief      初始化 SHUTOFF 输出并保持低电平
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void SHUTOFF_Init(void);

/*!
    \brief      反转 SHUTOFF 输出电平
    \param[in]  无
    \param[out] 无
    \retval     无
*/
void SHUTOFF_Toggle(void);

#endif /* SHUTOFF_H */
