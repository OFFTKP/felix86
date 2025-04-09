[English](./README.md) | [网站](https://felix86.com)

# felix86

felix86 是一个 Linux 用户空间模拟器，允许您在 RISC-V 处理器上运行 x86-64 Linux 程序

> [!NOTE]
> felix86 尚未完全发布。没有编译后的二进制文件可供下载。
>
> 如果你想协助开发，请查看 [汇编和使用指南](./docs/how-to-use.md) 和[软件开发人员小贴士](./docs/contributing.md).

## 特点
- 准时 (JIT) 重编译器
- 使用 RISC-V 向量扩展来处理 SSE 指令
- 在可用的情况下，利用 B 扩展来处理位操作指令，如 `bsr`
- 支持各种可选扩展，如 XThead 自定义扩展


## 兼容性
felix86 尚处于早期开发阶段。

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