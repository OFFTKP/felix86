###############################################################################
# Copyright (c) 2019, 2020 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at http://eclipse.org/legal/epl-2.0
# or the Apache License, Version 2.0 which accompanies this distribution
# and is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following Secondary
# Licenses when the conditions for such availability set forth in the
# Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
# version 2 with the GNU Classpath Exception [1] and GNU General Public
# License, version 2 with the OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
#############################################################################

SET(CMAKE_SYSTEM_PROCESSOR riscv64)
SET(CMAKE_SYSTEM_NAME Linux)

#
# Look for RISC-V cross-compilers. The "official" RISC-V GNU Compiler Toolchain [1]
# used prefix "riscv64-unknown-linux-gnu-" whereas Debian / Ubuntu (and possibly other)
# RISC-V toolchains [2] use just "riscv64-linux-gnu-" prefix. 
# 
# Here we use find_program() to make it work on both. 
#
# [1]: https://github.com/riscv/riscv-gnu-toolchain
# [2]: https://packages.debian.org/buster/amd64/gcc-riscv64-linux-gnu/filelist
#
find_program(CMAKE_C_COMPILER NAMES riscv64-unknown-linux-gnu-gcc riscv64-linux-gnu-gcc REQUIRED NO_CMAKE_FIND_ROOT_PATH)
find_program(CMAKE_CXX_COMPILER NAMES riscv64-unknown-linux-gnu-g++ riscv64-linux-gnu-g++ REQUIRED NO_CMAKE_FIND_ROOT_PATH)

#
# Include sysroot /usr/local/include to the path so compiler can find
# `riscv.h` and `riscv-opc.h`. This is required for "official" RISC-V GNU 
# Compiler Toolchain.
# 
include_directories("${CMAKE_FIND_ROOT_PATH}/usr/local/include")

SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)