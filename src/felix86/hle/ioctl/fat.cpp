#include <asm/ioctl.h>
#include <linux/msdos_fs.h>
#include <sys/ioctl.h>
#include "felix86/common/log.hpp"
#include "felix86/hle/ioctl/common.hpp"
#include "felix86/hle/ioctl/fat.hpp"

int ioctl32_fat(int fd, u32 cmd, u32 args) {
    switch (_IOC_NR(cmd)) {
        SIMPLE_CASE(VFAT_IOCTL_READDIR_BOTH);
        SIMPLE_CASE(VFAT_IOCTL_READDIR_SHORT);
        SIMPLE_CASE(FAT_IOCTL_GET_ATTRIBUTES);
        SIMPLE_CASE(FAT_IOCTL_SET_ATTRIBUTES);
        SIMPLE_CASE(FAT_IOCTL_GET_VOLUME_ID);
    default: {
        WARN("Unknown FAT ioctl cmd: %x", cmd);
        return ::ioctl(fd, cmd, args);
    }
    }
}