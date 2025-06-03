#include <asm/ioctl.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include "felix86/common/log.hpp"
#include "felix86/hle/ioctl/common.hpp"
#include "felix86/hle/ioctl/fs.hpp"

int ioctl32_fs(int fd, u32 cmd, u32 args) {
    switch (_IOC_NR(cmd)) {
    default: {
        ERROR("Unknown FS ioctl cmd: %x", cmd);
        return -1;
    }
    }
}