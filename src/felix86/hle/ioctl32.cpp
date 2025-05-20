#include <asm/ioctl.h>
#include <sys/ioctl.h>
#include "felix86/common/log.hpp"
#include "felix86/hle/ioctl32.hpp"

#include "felix86/hle/drm/drm.hpp"
#include "felix86/hle/drm/radeon.hpp"

int ioctl32_default(int fd, u32 cmd, u32 args) {
    return ::ioctl(fd, cmd, (u64)args);
}

int ioctl32_unknown(int fd, u32 cmd, u32 args) {
    WARN("Unknown ioctl command: %x", cmd);
    return ioctl32_default(fd, cmd, args);
}

int ioctl32(int fd, u32 cmd, u32 args) {
    switch (_IOC_TYPE(cmd)) {
    case DRM_IOCTL_BASE: {
        return ioctl32_drm(fd, cmd, args);
    }
    default: {
        return ioctl32_unknown(fd, cmd, args);
    }
    }
}

static std::unordered_map<int, ioctl_handler_type> handler_map;

void Ioctl32::registerFd(int fd, const std::string& name) {
    ioctl_handler_type type = ioctl32_default;
    if (name == "radeon") {
        type = ioctl32_radeon;
    } else {
        WARN("Unknown ioctl DRM name: %s", name.c_str());
    }

    auto guard = g_process_globals.states_lock.lock();
    if (handler_map.find(fd) != handler_map.end()) {
        WARN("ioctl handler map already includes fd %d, replacing...", fd);
    }

    PLAIN("Registered %d to %s", fd, name.c_str());
    handler_map[fd] = type;
}

ioctl_handler_type Ioctl32::getHandler(int fd) {
    auto guard = g_process_globals.states_lock.lock();
    if (handler_map.find(fd) != handler_map.end()) {
        return handler_map[fd];
    } else {
        WARN("Fd %d not found in map during ioctl", fd);
        return ioctl32_unknown;
    }
}