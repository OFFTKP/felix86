#include <asm/ioctl.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include "felix86/common/log.hpp"
#include "felix86/hle/ioctl/common.hpp"
#include "felix86/hle/ioctl/fs.hpp"

int ioctl32_fs(int fd, u32 cmd, u32 args) {
    switch (_IOC_NR(cmd)) {
        // This ioctl (and probably others) are defined to take long* as arguments in the fs.h header but in reality
        // take an int* so we don't need to do marshalling
        SIMPLE_CASE(FS_IOC_GETFLAGS);
    default: {
        WARN("Unknown FS ioctl cmd: %x", cmd);
        return ::ioctl(fd, cmd, args);
    }
    }
}