# GD32F303 开发框架

嵌入式开发框架，采用 **"核心库 + 实例工程"** 双层结构，适用于需要长期维护、跨平台移植的嵌入式固件项目。

**当前版本**：v0.0.0

## 快速开始

1. 克隆本仓库
2. 打开 `examples/GD32F303_DevBoard/GD32F303_DevBoard.code-workspace`
3. 使用 Keil μVision 5 或 VS Code + EIDE 编译烧录

## 文档索引

| 文档 | 用途 |
|------|------|
| `framework-dev-guide.md` | **开发入口** — 框架状态、填充决策树、开发规范、场景示例、硬件警告 |
| `docs/architecture.md` | 架构唯一事实源 — 分层模型、各层职责、调用约束 |
| `docs/development.md` | 开发流程和编码规范 |
| `docs/hardware.md` | ⚠️ 硬件管脚映射（当前与代码不匹配，见 framework-dev-guide.md） |

## 项目结构

```
├── core/                          ← 核心库骨架（待按需填充）
├── examples/<instance>/           ← 板级工程与驱动实现
├── framework-dev-guide.md         ← 开发指引（从此开始）
├── docs/                          ← 架构/开发/硬件文档
├── dev-guidelines/                ← 开发规则
└── CHANGELOG.md                   ← 版本历史