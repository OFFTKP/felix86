[中文](./README_CN.md)

# felix86

felix86 是一个 Linux 用户空间模拟器，允许你在 RISC-V 处理器上运行 x86-64 Linux 程序

### 功能
- 即时 (JIT) 重编译器
- 为 SSE 指令使用 RISC-V 向量扩展
- 支持各种可选扩展，如 XThead 自定义扩展

## 兼容性
felix86 尚处于早期开发阶段。

兼容性列表可在此处找到： https://felix86.com/compat

## 依赖关系
felix86 依赖于多个伟大的项目：

- [FEX](https://github.com/FEX-Emu/FEX)的综合单元测试套件和 rootfs 生成工具
- 用于 RISC-V 代码排放的 [Biscuit](https://github.com/lioncash/biscuit)
- 用于解码和反汇编的 [Zydis](https://github.com/zyantific/zydis)
- 用于单元测试的 [Catch2](https://github.com/catchorg/Catch2)
- 用于字符串格式化的 [fmt](https://github.com/fmtlib/fmt)
- 用于 JSON 解析的 [nlohmann/json](https://github.com/nlohmann/json)

## 为什么？
启动 felix86 有几个原因，包括

- 加深对 x86-64、RISC-V、Linux 和高级仿真的理解
- 探索优化编译器和 JIT（SSA、寄存器分配、优化传递等）
- 了解更多底层细节，如信号、系统调用、程序加载等
- 挑战有趣的项目

## 还可查看

- [Panda3DS](https://github.com/wheremyfoodat/Panda3DS)，一款适用于 Windows、macOS、Linux 和 Android 的 3DS 模拟器
- [shadPS4](https://github.com/shadps4-emu/shadPS4)，领先的 PS4 模拟器之一
- [ChonkyStation3](https://github.com/liuk7071/ChonkyStation3)，适用于 Windows、MacOS 和 Linux 的实验性 HLE PS3 模拟器
