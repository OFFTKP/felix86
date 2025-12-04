[English](./README.md) | [网站](https://felix86.com)

# felix86

felix86 是一款 Linux 用户空间模拟器，可在 RISC-V 处理器上运行 x86 和 x86-64 程序

## 入门指南

### Ubuntu/Debian/Bianbu 及其他系统
执行以下命令：

```bash
bash <(curl -s https://install.felix86.com)
```

该命令将下载并执行脚本，自动安装 felix86 及您选择的根文件系统。

[阅读使用指南](./docs/how-to-use.md) 获取更多信息。

开发者请参阅[贡献指南](./docs/contributing.md)。

欢迎加入我们的**Discord服务器**：[https://discord.gg/TgBxgFwByU](https://discord.gg/TgBxgFwByU)

## 核心特性
- 准时 (JIT) 重编译器
- 采用RISC-V向量扩展实现SSE 4.2级指令集
- 支持`B`、`Zicond`、`Zacas`等多项标准扩展
- 兼容各类自定义扩展
- 可在特定场景调用宿主库提升性能

## 兼容性
兼容性列表可在此处找到： https://felix86.com/compat

## 依赖关系
felix86 依赖于多个优秀项目：

- [FEX](https://github.com/FEX-Emu/FEX) 的综合单元测试套件
- [Biscuit](https://github.com/lioncash/biscuit) 用于 RISC-V 代码排放
- [Zydis](https://github.com/zyantific/zydis) 用于解码和反汇编
- [Catch2](https://github.com/catchorg/Catch2) 用于单元测试
- [fmt](https://github.com/fmtlib/fmt) 用于字符串格式化
- [nlohmann/json](https://github.com/nlohmann/json) 用于 JSON 解析
- [toml11](https://github.com/ToruNiina/toml11) 用于 TOML 解析

我们还采用其他项目的二进制测试来验证正确行为并防止回归。    
[测试用例详见此处](https://github.com/felix86-emu/binary_tests)

## 为什么？
felix86 的启动有几个原因，包括

- 加深对 x86-64、RISC-V、Linux 和高级仿真的理解
- 探索优化编译器和 JIT（SSA、寄存器分配、优化传递等）
- 了解更多低级细节，如信号、系统调用、程序加载
- 开展一个有趣且具有挑战性的项目

## 还可查看

- [Panda3DS](https://github.com/wheremyfoodat/Panda3DS), 3DS 模拟器，适用于 Windows、macOS、Linux 和 Android
- [shadPS4](https://github.com/shadps4-emu/shadPS4), 领先的 PS4 模拟器之一
- [ChonkyStation3](https://github.com/liuk7071/ChonkyStation3), 实验性 HLE PS3 模拟器，适用于 Windows、MacOS 和 Linux