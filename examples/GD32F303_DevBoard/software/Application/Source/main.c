#include "main.h"

/*!
    \brief      初始化板级基础服务与 Boost 10 kHz 控制系统
    \param[in]  无
    \param[out] 无
    \retval     主函数不返回
*/
int main(void)
{
    systick_config();
    AppKey_Init();
    BoostControl_Init();
    AppBoostMonitor_Init();

    while (1) {
        AppKey_Task();
        AppBoostMonitor_Task();
    }
}
