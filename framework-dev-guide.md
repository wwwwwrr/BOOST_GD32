# 框架开发指引

## 本文档定位

本文档是 GD32F303 开发框架的**开发指引**，面向以下场景：

- **初次接触框架的开发者**：了解各层状态和填充时机
- **新增需求时**：对照决策树确定需要实现的层级
- **扩展实例工程时**：参考扩展指南和规范

框架遵循**按需填充、不预填业务代码**的原则。`core/` 中的各层在当前无具体需求时保持骨架状态，待特定需求到来时再按本文档指引实现。
如果发现缺少对应需求，请向开发者确认需求，而非随意使用其他方式实现。

---
> **⚠️ 硬件文档警告：`docs/hardware.md` 描述的是另一套硬件平台（三相交错 Boost 电源板），与当前实例代码严重不匹配。详见下文第 1.4 节。**

---

## 1. 框架当前状态总览

### 1.1 核心库 core/ 各层状态

| 层级 | 目录 | 状态 | 说明 |
|------|------|------|------|
| LS (配置层) | `core/include/LS_Config/` | 🟡 **空骨架** | 仅目录结构就绪，无头文件 |
| L1 (编排层) | `core/include/L1_Orchestration/` `core/src/L1_Orchestration/` | 🔴 **空** | 无任何文件 |
| L2 (核心业务层) | `core/include/L2_Core/` `core/src/L2_Core/` | 🔴 **空** | 子目录 Business/Protocol/Runtime 均无文件 |
| L3 (HAL 层) | `core/include/L3_Hal/` `core/src/L3_Hal/` | 🔴 **空** | 无任何文件 |
| L4 (BSP 层) | `examples/<instance>/software/Utilities/` | 🟢 **完整** | ADC/PWM/Timer/USART/I2C/LED/AS5600 驱动已实现 |

### 1.2 BSP 驱动就绪列表

以下外设驱动已在实例工程 `examples/GD32F303_DevBoard/software/Utilities/` 中实现：

| 模块 | 功能 | 文件 |
|------|------|------|
| ADC | 三相电流采样、输出电压/电流检测 | `ADC/adc.c/.h` |
| PWM | 三相交错 Boost PWM 输出 | `PWM/pwm.c/.h` |
| USART | 板载 CH340N 串口通信（USART0，PA9/PA10） | `USART/usart.c/.h` |
| USART2 | 扩展串口（USART2） | `USART/usart2.c/.h` |
| I2C | I2C 总线驱动 | `I2C/i2c.c/.h` |
| AS5600 | 磁编码器驱动 | `AS5600/as5600.c/.h` |
| LED | GPIO LED 控制 | `LED/led.c/.h` |

### 1.3 实例应用层就绪情况

| 文件 | 状态 | 说明 |
|------|------|------|
| `Application/Source/main.c` | 🟢 **可用** | 仅编排系统与 Application 上层模块的初始化和主循环任务 |
| `Application/Source/app_key.c` | 🟢 **可用** | 连接 L1 Boost 命令与 L4 KEY/SHUTOFF |
| `Application/Source/app_boost_monitor.c` | 🟢 **可用** | 连接 L1 Boost 上下文与 L4 USART0 调试输出 |
| `Application/Include/main.h` | 🟢 **可用** | 汇总主程序所需上层接口 |
| `Application/Include/interrupt_priority.h` | 🟢 **可用** | 中断优先级集中定义，可持续使用 |

## 1.4 ⚠️ 硬件文档与 BSP 代码不匹配

`docs/hardware.md` 描述的是**三相交错 Boost 升压电源板**的硬件，但当前实例 `examples/GD32F303_DevBoard` 的 BSP 驱动代码实际适配的是**另一套硬件**（电机控制板）。两套硬件的管脚定义完全冲突。

### 已发现的不匹配项

| 模块 | docs/hardware.md 描述 | 代码实际定义 | 影响 |
|------|----------------------|-------------|------|
| **PWM** | TIM1/TIM2/TIM3 → PB6/PB7/PB5/PB4/PB3/PA15 | TIMER0 → PA8/PA9/PA10 + PB13/PB14/PB15 | 管脚完全不匹配 |
| **ADC** | PA0/PA1/PA2 (3路相电流) | PA6/PA7 (2路相电流) | 通道和数量均不同 |
| **LED** | PC13/PC14/PC15 | PB3/PB4/PB5 | 管脚不同 |
| **按键** | PA8/PB15/PB14/PB13 | ❌ 无对应驱动（这些脚被 PWM 占用） | 无实际按键驱动 |

### 建议处理方式

**方案 A**：确认当前代码适配的真实硬件，将 `docs/hardware.md` 重写为对应的管脚文档。
**方案 B**：如果 `docs/hardware.md` 描述的是目标硬件，则需要重写所有 BSP 驱动（`Utilities/` 下的 PWM/ADC/USART/LED 模块）以匹配该文档。

在问题修正前，开发时以**代码中 `.h` 文件定义的管脚宏为准**，不以 `docs/hardware.md` 为准。

---

## 2. 各层填充决策树

以下决策树指导在具体需求到来时，应填充哪些层级。

> **各层职责、依赖关系和调用约束详见 `docs/architecture.md` 的"分层架构模型"和"架构约束"章节。**

### 2.1 LS 配置层 —— 何时填充

```
┌─ 是否有可配置参数（频率/阈值/使能开关）？ ──→ 是 ──→ 创建 LS_Config 配置宏文件
│
└─ 是否有自定义类型（枚举/结构体）？ ──→ 是 ──→ 创建 LS_Config 类型定义文件
│
└─ 是否有编译期约束需要检查？ ──→ 是 ──→ 创建 compile_limits.h
```

**典型触发场景**：
- 需要定义功能裁剪宏（如 `ENABLE_FEATURE_XXX`）
- 需要定义系统默认参数（如控制频率、阈值）
- 需要定义业务相关的枚举和结构体类型
- 需要对非法配置进行编译期阻断

**实现参考**：LS 层按用途分类的文件清单见下文第 **3.1 节**。

### 2.2 L1 编排层 —— 何时填充

```
┌─ 是否有多个模块需要按顺序初始化？ ──→ 是 ──→ 创建 L1 初始化编排
│
└─ 是否有多个运行时实例需要统一持有？ ──→ 是 ──→ 创建系统上下文结构体
│
└─ 是否需要主循环统一调度多速率任务？ ──→ 是 ──→ 创建主循环框架
```

**典型触发场景**：
- 项目包含多个业务模块，需要统一管理初始化顺序
- 需要集中持有所有运行时状态（系统结构体）
- 需要调度多个不同频率的任务

### 2.3 L2 核心业务层 —— 何时填充

```
┌─ 是否有业务算法（如 PID、滤波、变换）？ ──→ 是 ──→ 在 L2_Core/Business/ 实现
│
└─ 是否有通信协议需要处理？ ──→ 是 ──→ 在 L2_Core/Protocol/ 实现
│
└─ 是否需要运行时工具（队列/调度器/调试）？ ──→ 是 ──→ 在 L2_Core/Runtime/ 实现
```

**典型触发场景**：
- 需要实现控制算法（如 Boost 电压闭环）
- 需要实现自定义通信协议
- 需要环形缓冲区、任务调度器等基础工具

### 2.4 L3 HAL 层 —— 何时填充

```
┌─ 是否有 L2 业务代码需要访问硬件外设？ ──→ 是 ──→ 创建 L3 HAL API 接口
│
└─ 是否需要跨平台移植能力？ ──→ 是 ──→ 创建 L3 HAL 接口 + 实例适配层
```

**典型触发场景**：
- L2 业务层需要读取 ADC、设置 PWM、发送串口数据
- 项目未来可能移植到不同 MCU 平台
- 需要单元测试（mock 硬件接口）

---

## 3. 各层实现规范

### 3.1 LS 层：文件模板

LS 层为纯编译期实体，按用途分类为以下文件：

```
core/include/LS_Config/
├── project_config.h              ← 统一入口（按序包含以下文件）
├── project_symbol_defs.h         ← 符号定义、硬件地址映射、兼容性宏
├── project_cfg_feature_switches.h ← 功能裁剪开关（ENABLE/DISABLE 宏）
├── project_cfg_init_values.h     ← 运行时默认值（结构体初始化用）
├── project_compile_limits.h      ← 编译期约束检查（#error / #warning）
├── project_shared_types.h        ← 类型定义统一入口
├── project_math_types.h          ← 滤波器、PID 等通用数学类型
├── project_business_types.h      ← 业务核心数据结构类型
├── project_scheduler_types.h     ← 调度枚举、任务速率定义
├── project_protocol_types.h      ← 协议命令枚举、解析器状态
├── project_runtime_types.h       ← 运行时状态码、错误码
└── project_snapshot_types.h      ← 快照/视图结构体（调试输出）
```

**约束**：
- 宏头文件之间不交叉依赖
- 各 `.c` 文件中枚举/结构体定义全部收敛至 LS 层
- 业务代码只包含 `project_config.h` 和 `project_shared_types.h`

### 3.2 跨层接口设计原则

各层接口设计遵循以下模式。完整的分层职责和调用关系见 `docs/architecture.md`。

| 层级 | 接口设计模式 | 实例化规则 |
|------|-------------|-----------|
| **L1** | `System_Init(SystemContext_t *ctx)`、`System_Run(SystemContext_t *ctx)` | 持有所有运行时实例（系统结构体） |
| **L2** | `Business_Init(Business_Context_t *ctx, const Business_Config_t *cfg)`、`Business_Process(Business_Context_t *ctx)` | 不持实例，通过指针操作传入数据 |
| **L3** | `HAL_ADC_Init(const HAL_ADC_Config_t *cfg)`、`HAL_ADC_GetSample(HAL_ADC_Sample_t *sample)` | 纯函数接口，无实例 |
| **L4** | `ADC_Init()`、`ADC_GetSample(adc_sample_t *sample)`（已实现） | 芯片固有实例（外设寄存器） |

**L3 HAL 层说明**：L3 接口头文件放在 `core/include/L3_Hal/` 中（平台无关），实现在实例工程的 `Application/` 下（平台相关）。L2 只能调用 L3 接口，不能直接调用 L4。

### 3.3 跨层调用约束

| 调用方向 | 是否允许 | 说明 |
|---------|---------|------|
| L1 → L2 | ✅ 允许 | L1 调用 L2 函数，传入实例指针 |
| L2 → L3 | ✅ 允许 | L2 通过 L3 API 访问硬件 |
| L3 → L4 | ✅ 允许 | L3 调用 BSP 驱动实现具体硬件操作 |
| L2 → L1 | ❌ 禁止 | 反向包含禁止，L2 不应感知 L1 |
| L2 → L4 | ❌ 禁止 | L2 必须通过 L3 访问硬件 |
| LS → X | ✅ 允许 | LS 为纯编译期，任意层可包含 |

---

## 4. 实例工程扩展指南

### 4.1 添加新外设驱动步骤

在实例工程的 `Utilities/` 下新增模块：

```
examples/<instance>/software/Utilities/<MODULE>/
├── <module>.h    ← API 声明（只暴露必要的接口）
└── <module>.c    ← 驱动实现
```

**规范**：
- 模块名使用大写缩写（如 `ADC`、`PWM`、`USART`）
- 函数命名：`Module_FunctionName`（如 `ADC_Init`、`PWM_SetDutyCycle`）
- 自包含——不依赖其他 Utilities 模块（除标准外设库外）
- 头文件必须包含外设类型定义（如 `i2c_status_t` 在 `i2c.h` 中定义）

**模块自包含检查清单**：
- [ ] 头文件可独立编译（包含所有必要依赖）
- [ ] 函数前缀统一（模块名）
- [ ] 无跨模块全局变量
- [ ] 类型定义在模块头文件内或 LS 层中

### 4.2 添加新实例工程步骤

```
Step 1: 复制 examples/<template>/ 为新工程
Step 2: 修改 software/Firmware/ 中的芯片 SDK（若 MCU 变更）
Step 3: 修改 hardware/ 下的管脚定义文档
Step 4: 修改 Application/Include/interrupt_priority.h
Step 5: 修改 Keil 工程配置（.uvprojx）包含路径和芯片型号
Step 6: 修改 .code-workspace 工作区配置
Step 7: 创建实例 README.md 说明硬件平台
```

### 4.3 测试代码与正式代码分离建议

- `main.c` 中的测试代码（如 `AS5600_Test()`、`ADC_Test()`）应在框架验证通过后删除
- 业务功能在 `core/` 的 L2 层实现，实例中只做板级适配
- 建议在 `Application/Source/` 下留空白 `main.c` 骨架，待需求到来时填充

---

## 6. 常见开发场景流程示例

### 场景 A：实现 Boost 电压闭环控制

**需求**：需要实现三相交错 Boost 的输出电压闭环控制。

**填充路径**：

```
1. LS 层：定义控制参数宏（目标电压、PID 系数、保护阈值）
2. L3 HAL 层：为 ADC（电压采样）、PWM（占空比控制）、Timer（控制周期）定义抽象接口
3. L2 Business 层：实现 PID 控制器、电压环算法（不持实例，通过指针操作）
4. L1 编排层：创建系统上下文，编排初始化流程，注册控制环路任务
5. 实例 Application：实现 L3 HAL 接口适配（调用 BSP 驱动），删除测试代码
```

**可复用产出**：PID 控制器可进入 `L2_Core/Business/`，其他实例可复用。

### 场景 B：添加 USART 通信

**需求**：需要通过 USART 与外部设备通信。

**填充路径**：

```
1. LS 层：定义协议帧格式类型、命令枚举、USART 配置参数
2. L4 BSP 层：在实例 Utilities/ 下实现 USART 驱动/数据读写函数等
3. L3 HAL 层：定义 USART 收发抽象接口
4. L2 Protocol 层：实现协议帧解析、组帧、校验（通过 L3 HAL 收发）
5. L1 编排层：在系统上下文中添加 USART 协议实例，注册接收处理
```

### 场景 C：新增实例工程

**需求**：在新硬件平台上使用该框架。

**填充路径**：

```
1. 按 4.2 节步骤复制工程
2. 检查 core/ 中的 L2/L3 代码是否依赖旧平台（应不依赖）
3. 实现新平台的 L4 BSP 驱动
4. 如果 L3 HAL 接口需要调整，在 core/ 中修改接口
5. 实现新平台的 L3 HAL 适配层
```

---

## 附录：文档索引

| 文件 | 用途 |
|------|------|
| `docs/architecture.md` | **唯一结构说明书（SSOT）** — 分层架构、各层职责、调用关系、架构约束 |
| `docs/development.md` | 开发流程和编码规范 |
| `docs/hardware.md` | 硬件管脚映射和连接 |
| `dev-guidelines/rules/development-workflow.mdc` | 版本控制、AI 辅助开发流程、质量关卡 |
| `dev-guidelines/rules/embedded-general.mdc` | 通用嵌入式开发规则（ARM Cortex-M） |
| `dev-guidelines/rules/project-specific.mdc` | 项目特定规则（模块边界、中断、时序等） |
