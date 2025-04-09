#pragma once

#include "felix86/hle/guest_types.hpp"

int sendmsg32(int fd, const x86_msghdr* msg, int flags);