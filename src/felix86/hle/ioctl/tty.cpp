#include <asm/ioctl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include "felix86/common/log.hpp"
#include "felix86/hle/ioctl/common.hpp"
#include "felix86/hle/ioctl/tty.hpp"

int ioctl32_tty(int fd, u32 cmd, u32 args) {
    switch (_IOC_NR(cmd)) {
        SIMPLE_CASE(TCGETS);
        SIMPLE_CASE(TCSETS);
        SIMPLE_CASE(TCSETSW);
        SIMPLE_CASE(TCSETSF);
        SIMPLE_CASE(TIOCGWINSZ);
        SIMPLE_CASE(FIONBIO);
        SIMPLE_CASE(TIOCINQ);
    default: {
        WARN("Unknown TTY ioctl cmd: %x", cmd);
        return ::ioctl(fd, cmd, args);
    }
    }
}