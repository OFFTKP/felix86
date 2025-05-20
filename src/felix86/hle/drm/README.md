# DRM marshalling
DRM structs in ioctls have different packing/sizes. This is due to two facts:
- 4 byte vs 8 byte pointers in 32-bit and 64-bit mode
- different packing alignments in x86

For example, in x86 the following struct:
```c
struct foo {
    u32 a;
    u64 b;
};
```
will be tightly packed, while in x86-64 there will be padding to push `b` to 64-bit alignment.

We need to marshal any struct that uses pointers or contains members that could cause different alignments.