#include "main.h"
#include "interrupt_priority.h"

/*!
    \brief      初始化板级基础服务与 Boost 10 kHz 控制系统
    \param[in]  无
    \param[out] 无
    \retval     主函数不返回
*/
int main(void)
{
// 		rcu_periph_clock_enable(RCU_AF);

// /* 禁用JTAG，但保留SWD：PA13=SWDIO，PA14=SWCLK */
// gpio_pin_remap_config(GPIO_SWJ_SWDPENABLE_REMAP, ENABLE);
	
    nvic_priority_group_set(NVIC_PRIORITY_GROUPING);
    systick_config();
    AppKey_Init();
    BoostControl_Init();
    AppBoostMonitor_Init();


    while (1) {
        AppKey_Task();
        AppBoostMonitor_Task();
    }
}
