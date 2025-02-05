[English](./README.md)

# felix86
felix86 是一个 Linux 用户空间模拟器。它允许你在 RISC-V 处理器上运行 x86-64 Linux 程序。开发初期。

编译和使用说明可在 [此处]（./docs/how-to-use.md）找到。

想投稿但不知道怎么做？[看看这个](./docs/contributing.md)。

### 功能
- JIT 重编译器
- 对 SSE 指令使用 RISC-V 向量扩展
- 如果位操作指令（如 `bsr` ）可用，则使用 B 扩展
- 支持多种可选扩展，如 XThead 自定义扩展

## 兼容性
felix86 尚处于开发初期，不支持 AArch64。

目前，felix86 可以运行一些基于控制台的应用程序，如 `python3` 或 `lua`。

如果你想要一个更成熟的 x86-64 用户空间模拟器，请使用其中之一：

- [FEX](https://github.com/FEX-Emu/FEX), 适用于 AArch64 上的 x86 & x86-64
- [box64](https://github.com/ptitSeb/box64), 适用于 AArch64 和 RISC-V 上的 x86 和 x86-64
- [qemu-user](https://www.qemu.org/docs/master/user/main.html)，基本上适用于所有操作系统

## 依赖
felix86 依赖于一些優秀的项目：
- [FEX](https://github.com/FEX-Emu/FEX) 的单元测试套件和 rootfs 生成工具
- [Biscuit](https://github.com/lioncash/biscuit) 用于 RISC-V 代码生成
- [Zydis](https://github.com/zyantific/zydis) 用于解码/取消转码
- [Catch2](https://github.com/catchorg/Catch2) 用于单元测试
- [fmt](https://github.com/fmtlib/fmt) 用于字符串格式化
- [nlohmann/json](https://github.com/nlohmann/json) 用于 JSON 解析

## 为什么？
启动 felix86 有几个原因，仅举几例：
- 学习更多关于 x86-64、RISC-V、Linux、高级仿真的知识
- 学习优化编译器、JIT（SSA、RA、优化通路......）。
- 进一步了解不同的内存模型和底层细节
- 有趣的挑战

## 还可以查看
- [Panda3DS](https://github.com/wheremyfoodat/Panda3DS), 一款适用于 Windows、MacOS、Linux 和 Android 的 3DS 模拟器
- [shadPS4](https://github.com/shadps4-emu/shadPS4)，目前最佳的 PS4 模拟器

