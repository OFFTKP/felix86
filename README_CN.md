# felix86

felix86 是一款 Linux 用户空间模拟器。它允许您在 RISC-V 处理器上运行 x86-64 Linux 程序。目前处于初期开发阶段。

编译和使用指南可在 [这里](./docs/how-to-use.md) 找到。

想做贡献却不知道该怎么做？[查看这里](./docs/contributing.md)。

## 功能
- JIT 编译器
- 使用 RISC-V 向量扩展来支持 SSE 指令
- 如果支持则使用 B 扩展来处理低位操作指令，如 `bsr`
- 支持多种可选扩展，包括 XThead 自定义扩展

## 兼容性
felix86 处于初期开发阶段，将不支持 AArch64。

目前，felix86 能运行些基于控制台的应用程序，如 `python3` 或 `lua`。

如果您需要更成熟的 x86-64 用户空间模拟器，可以考虑以下选项：

- [FEX](https://github.com/FEX-Emu/FEX)，在 AArch64 上运行 x86 和 x86-64
- [box64](https://github.com/ptitSeb/box64)，在 AArch64 和 RISC-V 上运行 x86 和 x86-64
- [qemu-user](https://www.qemu.org/docs/master/user/main.html)，基本上能在各种平台上运行所有类型

## 依赖
felix86 依赖于一些優秀的项目：
- [FEX](https://github.com/FEX-Emu/FEX) 的单元测试套件和 rootfs 生成工具
- [Biscuit](https://github.com/lioncash/biscuit) 用于 RISC-V 代码生成
- [Zydis](https://github.com/zyantific/zydis) 用于解码/取消转码
- [Catch2](https://github.com/catchorg/Catch2) 用于单元测试
- [fmt](https://github.com/fmtlib/fmt) 用于字符串格式化
- [nlohmann/json](https://github.com/nlohmann/json) 用于 JSON 解析

## 创建原因
felix86 的创建原因有许多，其中包括：
- 深入了解 x86-64，RISC-V，Linux，高级模拟技术
- 学习关于优化编译器、JIT (如 SSA、RA、优化通过器等)
- 了解不同的内存模型和很多底层细节
- 出于对技术挑战的热情

## 还可以查看
- [Panda3DS](https://github.com/wheremyfoodat/Panda3DS)，面向 Windows、MacOS、Linux 和 Android 的 3DS 模拟器
- [shadPS4](https://github.com/shadps4-emu/shadPS4)，目前最佳的 PS4 模拟器

