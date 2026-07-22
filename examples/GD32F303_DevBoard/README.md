# GD32F303 DevBoard 实例工程

## 概述

本实例工程基于 **GD32F303CCT6** 芯片，该实例演示了框架的 BSP 驱动层实现，涵盖 PWM 控制、ADC 采样、串口通信、I2C 总线及磁编码器读取等常见外设功能。

## 硬件平台

| 项目 | 参数 |
|------|------|
| MCU | GD32F303CCT6 (ARM Cortex-M3) |
| 工作电压 | 3.3V |
| 时钟 | 外部 8MHz 晶振 |
| 调试接口 | SWD (PA13/PA14) |
| 通信 | USART0 (CH340N)、USART2 (扩展)、I2C0 (AS5600) |

详细硬件说明见 `docs/hardware.md`。

## 软件结构

```
software/
├── Application/               ← 应用层
│   ├── Include/
│   │   ├── main.h            ← 主程序依赖入口
│   │   ├── app_key.h         ← 按键业务绑定接口
│   │   ├── app_boost_monitor.h ← Boost 串口监视上层接口
│   │   ├── interrupt_priority.h ← 中断优先级集中定义
│   │   └── ...
│   └── Source/
│       ├── app_key.c         ← SHUTOFF 与 Boost 按键业务编排
│       ├── app_boost_monitor.c ← 200 ms 状态复制与 USART0 输出编排
│       └── main.c            ← Boost 控制系统初始化入口
├── Utilities/                 ← BSP 外设驱动
│   ├── ADC/                  ← 三相电流采样、电压检测
│   ├── PWM/                  ← 三相交错 PWM 输出
│   ├── TIMER4/               ← 10 kHz Boost 控制定时器
│   ├── USART/                ← USART0 板载 CH340N 串口通信
│   ├── USART2/               ← 扩展串口
│   ├── I2C/                  ← I2C 总线驱动
│   ├── AS5600/               ← 磁编码器驱动
│   ├── KEY/                  ← 四路 EXTI 按键与非阻塞消抖
│   ├── SHUTOFF/              ← PB12 SHUTOFF 输出控制
│   └── LED/                  ← GPIO LED 控制
├── Firmware/                  ← 芯片 SDK (GD32F30x 标准外设库)
├── RTE/                       ← Keil 运行时环境
└── Objects/                   ← 编译输出
```

## 开发环境

- **主工具链**：Keil μVision 5 + ARM Compiler 5 (AC5)
- **开发 IDE**：VS Code + EIDE 扩展
- **调试器**：ST-LINK
- **构建方式**：直接打开 `Project.uvprojx` 或通过 EIDE 编译

## 当前状态

- PWM 驱动已切换为 TIMER3/TIMER2/TIMER1 三相 120° 交错、六路逻辑互补输出
- `main.c` 只编排系统及 Application 上层模块的初始化和主循环任务；系统上电保持空闲，通过按键回调提交启动或目标电压调整请求
- TIMER4 以 10 kHz 运行 Boost 快速控制环，执行命令处理、ADC 换算、过压与开路判断、双环 PI 和最终 PWM 状态输出
- ADC0 软件触发连续扫描 PA3/PA4/PA5，采用 71.5 周期采样和硬件 8 倍过采样；DMA 半满/全满各发布一组三通道平均 raw，理论更新率约 14.9 kHz
- Boost 初始化首先使用独立 ADC1 软件轮询流程校准 PA0/PA1/PA2 的零电流偏置，丢弃前 8 轮并对每相 64 个样本求平均，完成后反初始化 ADC1 再进入正常采样流程
- ADC1 由 TIMER3 同步启动的 300 kHz TIMER0_CH0 触发，使用单通道规则序列并在每次 EOC 中断中轮转 PA0/PA1/PA2，每相更新率为 100 kHz
- L3 `ADCMeasurement_*` 将 ADC0 平均 raw 换算为输出电流、输出电压和输入电压，并将 ADC1 各相最新 raw 换算为带正负方向的相电流
- 六路波形与长期相位稳定性仍需使用示波器或六通道逻辑分析仪完成硬件验收
- L1 持有默认 30 V 电压环和 2 A 电流环 PI 实例，两环占空比取较小值并统一输出到 A/B/C 三相；目标电压可在 12.0～34.9 V 范围内按 0.1 V 步长调整，USART0 的单字节 `+`/`-` 命令可将目标电流在 0.5～2.0 A 范围内按 10 mA 步长调整
- 软启动目标从 12 V 开始，每个 100 us 增加 0.001 V；占空比限制为 0%～70%，每周期最大变化为 0.1%
- 软件保护启用 35 V 输出过压锁存，以及 18 V/50 mA/5 ms 静态开路和 200 mA 带载历史后 50 mA/0.5 ms 运行中断开判断；故障在 PI 计算前锁存，并于同一控制中断内清零并停止全部 PWM
- LED3 已按原理图改为 PC13/PC14/PC15 共地 RGB 驱动；Boost 空闲时熄灭，软启动显示蓝色，正常运行显示绿色，故障显示红色
- KEY1～KEY4 已配置为低电平有效的 EXTI 输入；KEY1 反转 PB12 SHUTOFF，KEY2 请求启动 Boost，KEY3/KEY4 分别将目标电压增加/减少 0.1 V
- Application 层 `AppBoostMonitor_Init/Task` 使用 PA9/PA10 的 USART0 和 115200-8N1；RX 通过 DMA0_CH4 接收并在 IDLE 中断中解析每批首个 `+`/`-` 命令，其余字符静默忽略且不返回回执；初始化时打印一次三相校准偏置，之后每 200 ms 复制一次 Boost 上下文并打印状态、模式、故障、目标电压/电流、六项 ADC 实际值和三相占空比

## 快速开始

1. 在 VS Code 中打开 `GD32F303_DevBoard.code-workspace`
2. 使用 EIDE 扩展编译，或直接打开 Keil `Project.uvprojx`
3. 通过 ST-LINK 烧录到目标板
4. 串口（默认 115200-8N1）查看输出信息

## 管脚分配速查

| 功能 | 引脚 |
|------|------|
| 三相 PWM | PB6/PB7（A 正向/互补）, PB4/PB5（B 正向/互补）, PA15/PB3（C 正向/互补） |
| 三相电流 ADC | PA0/PA1/PA2 |
| 输出电流检测 | PA3 |
| 输出电压检测 | PA4 |
| 输入电压检测 | PA5 |
| USART0 (CH340) | PA9/PA10 |
| USART2 (扩展) | PB10/PB11 |
| I2C0 (AS5600) | 旧配置：PB6(SCL)/PB7(SDA)，与 A 相 PWM 冲突，需迁移 |
| 按键 | PA8/PB15/PB14/PB13 |
| SHUTOFF 输出 | PB12，初始化为低电平，KEY1 按下时反转 |
| LED | PC15(R)/PC14(G)/PC13(B) |

PWM 固定为 100 kHz。TIMER3 为主定时器，TIMER2/TIMER1 通过 ITI3 Event 模式同步启动；A/B/C 正向输出相位依次为 0°/120°/240°。占空比接口接受 `float`，按 1200 个周期计数直接截断量化。当前普通定时器方案不支持硬件死区，`PWM_DEAD_TIME_NS` 必须保持为 0。

ADC0 底层通过双快照地址发布 PA3 输出电流、PA4 输出电压、PA5 输入电压的硬件 8 倍平均 raw，不在 BSP 内进行软件平均或物理量换算；ADC1 使用长度为 1 的规则序列，每次 EOC 中断保存当前相 raw 并把 Rank0 切换到下一相，读取结果允许来自相邻轮转。L3 `ADCMeasurement_*` 校验 ADC0 快照令牌后，使用 3.3 V/4096 量化比例换算实际值：输出电压增益 29.75、输入电压增益 5.29、输出电流增益 20 A/V；A/B/C 相电流分别使用上电校准的本相偏置 raw 计算并保留负值。

L3 的 `DutyControl_*` 接口负责初始化、启停和设置三相占空比，不向上层暴露 GD32 或 PWM 驱动类型。L2 的 `IncrementalPI_*` 按 `Δu=Kp[e(k)-e(k-1)]+Ki×e(k)` 计算单周期增量，并使用 LS 中的 `BOOST_DUTY_MAX_STEP_PERCENT` 限幅。控制器实例由 L1 持有，L1 负责将增量累加到电压环和电流环候选占空比，再将结果限制到 0%～70% 后交给 L3。

L1 `BoostControl_10kHzHandler()` 由 TIMER4 中断通过 L3 控制定时器回调触发。命令处理优先级为 STOP、CLEAR_FAULT、START；STOP 不清除锁存故障，STOP 和 CLEAR_FAULT 都会复位两个 PI、软启动目标以及全部占空比。真实 PWM 启停与写入只在最终状态执行阶段发生。

调试输出由 Application 层 `AppBoostMonitor_Task()` 调度，并通过 USART0 的 `printf` 重定向发送。`main.c` 不直接复制或格式化 Boost 数据；快速控制中断也不格式化或发送串口数据。Application 层复制上下文时不关闭中断，因此极少数打印行可能包含相邻两个控制周期的数据。

完整管脚定义见 `docs/hardware.md`。

---

*本实例当前包含三相交错 Boost 的首版 10 kHz 闭环控制编排，硬件验收仍需使用示波器、逻辑分析仪和可控电源完成。*
