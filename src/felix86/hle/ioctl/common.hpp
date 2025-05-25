#pragma once

#include <cerrno>

#define SIMPLE_CASE(ioctl_name)                                                                                                                      \
    case _IOC_NR(ioctl_name): {                                                                                                                      \
        VERBOSE("Running " #ioctl_name "(%d, %x, %x)", fd, cmd, args);                                                                               \
        int result =                                                                                                                                 \
            ::ioctl(fd, cmd, (u64)args); /* the (u64) is important because otherwise it gets sign extended which is no good with pointers */         \
        if (result == -1) {                                                                                                                          \
            result = -errno;                                                                                                                         \
            VERBOSE("%s failed with %d", #ioctl_name, result);                                                                                       \
        }                                                                                                                                            \
        return result;                                                                                                                               \
    }

#define MARSHAL_CASE(ioctl_name, type)                                                                                                               \
    case _IOC_NR(ioctl_name): {                                                                                                                      \
        VERBOSE("Running " #ioctl_name "(%d, %x, %x)", fd, cmd, args);                                                                               \
        x86_##type* guest = (x86_##type*)(u64)args;                                                                                                  \
        type host = *guest;                                                                                                                          \
        int result = ::ioctl(fd, ioctl_name, &host);                                                                                                 \
        if (result != -1) {                                                                                                                          \
            *guest = host;                                                                                                                           \
        } else {                                                                                                                                     \
            result = -errno;                                                                                                                         \
            VERBOSE("%s failed with %d", #ioctl_name, result);                                                                                       \
        }                                                                                                                                            \
        return result;                                                                                                                               \
    }
