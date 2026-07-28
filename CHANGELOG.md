# Changelog

All notable changes to the GD32F303CC FOC Motor Control Project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [未发布]

### 新增
- 新增 3.0 A 三相独立正向相电流限制：超限相仅禁止占空比继续增加，其他相继续跟随双环公共目标；电流恢复后按每周期最大 0.1% 限步追赶，公共目标降低时仍允许超限相降低占空比
- USART0 接收改为 DMA0_CH4 + IDLE 批次中断，新增单字节 `+`/`-` 命令，通过异步标志将 Boost 目标电流按 10 mA 步长增加/减少并限制在 0.5～2.0 A；每批只处理首个有效命令且不返回回执
- 新增 Boost 运行时目标电压调节接口，KEY3/KEY4 分别按 0.1 V 步长增加/减少目标电压，并将调节范围限制为 12.0～34.9 V
- USART0 周期状态输出新增目标电压和目标电流字段
- 新增 `BoostControl_GetContext()` 上下文复制接口，以及 Application 层 `AppBoostMonitor_Init/Task` 状态监视 API，通过 USART0 每 200 ms 打印 Boost 状态、模式、故障、ADC 实际值和三相占空比
- 新增 Boost 初始化前置 ADC1 三相零电流偏置校准，使用独立软件触发轮询流程对 A/B/C 每相 64 个样本求平均，校准后反初始化 ADC1，并在串口初始化时打印一次三相 raw 和偏置电压
- 新增 ADC1 三相偏置校准故障锁存，校准失败时禁止 PWM 启动
- 新增 Boost 静态开路与运行中断开保护：18 V 后以 50 mA 空载阈值确认静态开路，并在曾检测到 200 mA 负载后以 0.5 ms 快速确认运行中断开；两类场景统一锁存输出开路故障并停止三相 PWM
- 新增 L3 RGB 状态指示接口，Boost 空闲时熄灭 LED3，软启动、运行和故障状态分别显示蓝色、绿色和红色

### 变更
- 删除已停用且未被调用的 Boost 输出开路检查函数及其只写不读的私有状态，保持现有不启用开路保护的运行行为并消除 AC5 警告
- 删除 PWM 驱动中调用已被注释的未引用实际占空比换算函数，保持现有 PWM 输出行为不变并消除 AC5 警告
- ADC1 的 TIMER0 硬件触发频率由 300 kHz 降至 100 kHz，以可配置的 30%、63%、96% 采样点按 B→C→A 轮转；96% 转换跨更新点时丢弃一次旧 CCR 过渡结果，每 4 个 PWM 周期发布一组三相有效值
- 三相 PWM 保持边沿对齐向上计数和 A→B→C 的 120° 相序，将正向 CH0 改为 PWM1、互补 CH1 改为 PWM0，并按低电平结束位置换算 CCR，使请求占空比表示每周期末段的高电平比例
- ADC1 的 PWM 同步运行时采样顺序由 A→B→C 调整为 B→C→A，使 114°、234°、354° 三个触发点分别对应当前 PWM 各 120° 区间末尾的 B、C、A 相；偏置校准和读取结果字段仍保持 A/B/C 语义
- KEY3 停止和 KEY4 清故障回调改为目标电压增减请求，原命令调用保留为注释
- 板载调试串口由占用 ADC 管脚 PA2/PA3 的 USART1 迁移至 PA9/PA10 的 USART0，并将 `printf` 重定向到中断发送缓冲
- Keil 工程启用 AC5 MicroLIB，与 EIDE 工程配置保持一致
- `main.c` 删除状态复制、周期判断和文本格式化实现，只保留 `AppBoostMonitor_Init/Task` 上层 API 调用
- ADC1 改为单通道规则序列，在每次 EOC 中断中保存当前相最新 raw 并将 Rank0 切换到下一相；同时移除三相双发布缓冲、采样序号和新数据判断，读取接口直接返回 A/B/C 各相最近值
- 相电流换算由三相共用固定 1.1 V 偏置改为分别使用上电校准得到的 A/B/C 偏置 raw
- 输出开路判断的电压、电流和毫秒参数集中到 LS 配置层，并根据 10 kHz 控制频率自动换算确认周期；故障路径在 PI 计算前执行并于同一控制中断内停止 PWM
- LED 驱动删除占用 PWM 引脚的 PB3/PB4/PB5 旧映射及数字式控制接口，按最新版原理图改为 PC13/PC14/PC15 共地 RGB LED3 互斥颜色控制

## [0.5.0] - 2026-07-14

### 新增
- 新增 L1 `BoostControl_*` 10 kHz Boost 控制编排，包含空闲、软启动、运行和故障四种状态
- 新增 START、STOP、CLEAR_FAULT 异步命令接口，STOP 与清故障均复位双环 PI、12 V 软启动目标和全部占空比
- 新增 TIMER4 10 kHz 控制定时器及 L3 `ControlTimer_*` 抽象，中断内只清除更新标志并调用控制回调
- 新增 30 V 电压环与 2 A 电流环取小控制、0.1% 单周期占空比限步和 40 V 输出过压锁存保护
- 新增 PB12 SHUTOFF 推挽输出驱动，初始化前清零输出锁存并默认保持低电平

### 变更
- L2 `IncrementalPI_*` 改用 `kp/ki/kd/integral/last_err` 五字段结构，只返回单周期增量
- `main.c` 从 PWM/ADC 专项测试切换为 Boost 控制系统初始化，并通过 Application 层 `app_key` 模块绑定按键业务
- Keil 工程加入 L1 Boost 控制、L3 控制定时器和 L4 TIMER4 源文件，并通过 AC5 零错误零警告编译
- 简化 ADC0 实际值处理流程，删除浮点换算完成后的快照令牌复查
- 删除不再使用的 ADC0 快照发布令牌字段和快照二次校验接口
- 补齐 ADC 底层与实际值测量层的变量、结构体字段和状态中文注释
- ADC 采样方式、DMA 配置、实际值换算系数及 ADC1 处理逻辑保持不变
- Boost 电压环与电流环直接使用 `Ki=0.001/周期`，删除每秒 Ki 及采样周期换算
- 增量计算对齐参考公式 `Δu=Kp×[e(k)-e(k-1)]+Ki×e(k)`，增量在 L2 内按 LS 配置限幅，占空比由 L1 累加并限制到 0%～70%
- 电压环和电流环增加 `Kd=0` 配置，`kd` 与 `integral` 字段当前保留但不参与计算
- 补全 PWM 的定时器映射、GPIO、相位、影子更新、同步启停和逻辑互补中文注释，功能不变
- 补全 KEY 的四路 EXTI 映射、待处理位、消抖状态机、ISR 限制和主循环任务中文注释，功能不变
- 将 `KEY_SetCallback` 注册移出 `main.c`；KEY1 反转 SHUTOFF，KEY2/KEY3/KEY4 分别请求启动、停止和清除 Boost 故障
- Boost 仅保留一个 `boost_control_context_t` 运行上下文，删除快照类型、快照接口、ADC 有效标志和控制周期计数
- Boost 10 kHz 任务改为每周期直接复制 ADC 实际值，并在双 PI 取小后再执行 0%～70% 最终限幅
- IDLE 和 FAULT 状态通过 `pwm_running` 仅在 PWM 实际运行时执行一次停止，避免 10 kHz 重复操作定时器和 GPIO
- 删除 ControlTimer 与 TIMER4 未使用的 DWT 执行时间统计接口及运行数据
- 删除未被当前主流程使用的 TIMER1/TIMER2 通用中断驱动、旧 `timer1_algorithm` 调度模块及对应 ISR；PWM 对硬件 TIMER1/TIMER2 的直接使用保持不变
- 同步 EIDE 活动目标的 Application、L1/L2/L3、TIMER4、KEY 和 SHUTOFF 源文件及包含路径，使其与当前 Keil 工程保持一致

## [0.4.0] - 2026-07-13

### 新增
- 新增 `PWM_PHASE_A/B/C` 独立占空比接口和三相占空比批量更新接口
- 新增 TIMER1/TIMER2/TIMER3 非零死区配置的编译期检查
- 在 `main.c` 中新增 100 kHz、20.1% 请求占空比的三相 PWM 专项测试
- 新增 KEY1～KEY4 按键模块，分别使用 PA8、PB15、PB14、PB13 下降沿 EXTI 中断
- 新增基于 SysTick 时间差的 20 ms 非阻塞按下/松开消抖状态机
- 新增四路独立按下回调注册接口，并在 `main.c` 中预留 KEY1～KEY4 处理函数
- 新增 ADC0 对 PA3/PA4/PA5 的软件触发连续扫描和 DMA 双快照采集
- 新增 ADC1 对 PA0/PA1/PA2 的 TIMER0_CH0 硬件触发间断采样
- 新增 ADC0 只读快照地址、发布序号和有效性令牌接口，以及 ADC1 三相 raw 读取接口
- 新增 LS 配置入口，集中管理 ADC 参考电压、分压增益、采样电阻和相电流偏置
- 新增 L3 `ADCMeasurement_*` 实际值接口，输出电压、电流及带方向的三相电流

### 变更
- 将 TIMER0 PWM 驱动替换为 TIMER3/TIMER2/TIMER1 同步边沿对齐 PWM
- 将三相正向/互补输出分别映射到 PB6/PB7、PB4/PB5 和 PA15/PB3
- 配置 TIMER3 TRGO、TIMER2/TIMER1 Event 从模式及 0°/120°/240° 计数器预装值
- 因 PWM 独占相关定时器和引脚资源，暂时停用 LED、定时器调度、I2C/AS5600 和 ADC 应用测试
- PWM 占空比设置和读取接口改为 `float`，支持 `20.1f` 等小数输入
- 浮点占空比直接截断量化为整数 CCR，读取接口返回量化后的实际占空比
- 将 ADC0 检测引脚修正为 PA3 输出电流、PA4 输出电压、PA5 输入电压
- ADC0 改用 71.5 周期采样、硬件 8 倍过采样和 3 位右移，DMA 半满/全满各发布一组三通道平均 raw
- ADC0 DMA 循环缓冲改为 2 帧，底层移除软件求和、平均和实际值缓存，由 L3 校验快照后换算物理量
- TIMER0 配置为 300 kHz TIMER3 从定时器，并在 CH0 计数末尾触发 ADC1
- ADC 实际值换算采用 29.75 输出电压增益、5.29 输入电压增益、5 mΩ 采样电阻和 1.1 V 相电流偏置
- EXTI5_9 与 EXTI10_15 中断仅清除硬件标志并设置软件待处理位，按键判断及回调统一移至主循环 `KEY_Task()`
- 按键按住期间不重复触发，稳定松开 20 ms 后才重新允许下一次按下

### 安全说明
- 当前普通定时器方案的死区为 0，不得直接驱动依赖 MCU 内部直通保护的功率级

## [0.3.0] - 2026-03-13

### Added
- ADC current sampling implementation (PA6/PA7 synchronous sampling)
- DMA-based ADC data transfer
- Current calculation with zero offset calibration
- Multi-language development guidelines (English/Chinese)
- Comprehensive documentation reorganization
- Semantic versioning and branching strategy

### Changed
- Renamed `.cursor/` to `dev-guidelines/` for generalization
- Reorganized documentation structure under `docs/`
- Simplified main.c by removing test loops for production readiness
- Optimized ADC module error checking for embedded efficiency
- Updated development workflow to support AI-assisted development

### Fixed
- Compiler warnings (newline in header, implicit function declarations)
- Code style consistency across modules
- Resource usage optimization for embedded constraints

### Removed
- Test functions from main loop (moved to conditional compilation)
- Redundant error checking in time-critical paths

## [0.2.0] - 2026-03-11

### Added
- PWM dead time implementation
- Timer1 algorithm callback system
- Hardware I2C driver for GD32F30x
- AS5600 magnetic encoder driver
- LED blink callback encapsulation

### Changed
- Timer modules now use parameterized initialization
- Improved module boundaries and API consistency

## [0.1.0] - 2026-03-10

### Added
- Basic project structure with GD32F303CC
- Timer-based multi-rate scheduling framework
- USART1 interrupt-driven communication
- PWM output for 3-phase motor control
- LED status indication
- Initial development guidelines and rules

