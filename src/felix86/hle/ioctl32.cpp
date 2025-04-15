#include <asm/ioctl.h>
#include <sys/ioctl.h>
#include "felix86/common/log.hpp"
#include "felix86/hle/ioctl32.hpp"

int ioctl32(int fd, u32 cmd, u32 args) {
    switch (_IOC_TYPE(cmd)) {
    default: {
        WARN("Unknown ioctl command: %x", cmd);
        return ::ioctl(fd, cmd, args);
    }
    }
}