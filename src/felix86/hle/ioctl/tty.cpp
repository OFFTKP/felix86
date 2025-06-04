#include <asm/ioctl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include "felix86/common/log.hpp"
#include "felix86/hle/guest_types.hpp"
#include "felix86/hle/ioctl/common.hpp"
#include "felix86/hle/ioctl/tty.hpp"

int ioctl32_tty(int fd, u32 cmd, u32 args) {
    switch (_IOC_NR(cmd)) {
        SIMPLE_CASE(TCGETS);
        SIMPLE_CASE(TIOCGWINSZ);
    default: {
        ERROR("Unknown TTY ioctl cmd: %x", cmd);
        return -1;
    }
    }
}