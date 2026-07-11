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
│   │   ├── main.h            ← 主头文件（测试用，正式开发需重建）
│   │   ├── interrupt_priority.h ← 中断优先级集中定义
│   │   └── ...
│   └── Source/
│       └── main.c            ← 主程序（当前为测试代码，正式开发需重建）
├── Utilities/                 ← BSP 外设驱动
│   ├── ADC/                  ← 三相电流采样、电压检测
│   ├── PWM/                  ← 三相交错 PWM 输出
│   ├── TIMER1/               ← 算法定时器
│   ├── TIMER2/               ← 通用定时器
│   ├── USART/                ← USART1 串口通信
│   ├── USART2/               ← 扩展串口
│   ├── I2C/                  ← I2C 总线驱动
│   ├── AS5600/               ← 磁编码器驱动
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

- BSP 驱动层已完成并验证（ADC/PWM/Timer/USART/I2C/LED/AS5600）
- `main.c` 包含临时测试代码，用于验证各外设驱动
- 框架核心层（LS/L1/L2/L3）尚未填充，详见 `docs/framework-dev-guide.md`

## 快速开始

1. 在 VS Code 中打开 `GD32F303_DevBoard.code-workspace`
2. 使用 EIDE 扩展编译，或直接打开 Keil `Project.uvprojx`
3. 通过 ST-LINK 烧录到目标板
4. 串口（默认 115200-8N1）查看输出信息

## 管脚分配速查

| 功能 | 引脚 |
|------|------|
| 三相 PWM | PB6/PB7, PB5/PB4, PB3/PA15 |
| 三相电流 ADC | PA0/PA1/PA2 |
| 输出电压检测 | PA3 |
| 输出电流检测 | PA4 |
| 输入电压检测 | PA5 |
| USART0 (CH340) | PA9/PA10 |
| USART2 (扩展) | PB10/PB11 |
| I2C0 (AS5600) | PB6(SCL)/PB7(SDA) |
| 按键 | PA8/PB15/PB14/PB13 |
| LED | PC15(R)/PC14(G)/PC13(B) |

完整管脚定义见 `docs/hardware.md`。

---

*本实例为框架的 BSP 层参考实现，具体业务代码待需求到来时按 `docs/framework-dev-guide.md` 指引填充。*