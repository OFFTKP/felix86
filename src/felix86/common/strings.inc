X("felix86 - a userspace x86 and x86_64 emulator for RISC-V", "felix86 - 运行于 RISC-V 上的用户态 x86 与 x86_64 模拟器")
X("Enter the rootfs through a shell", "通过 shell 进入 rootfs")
X("Enter the rootfs through a shell, enable logging", "通过 shell 进入 rootfs，并启用日志输出")
X("Get a config value from the local felix86 configuration file. Format is '<group.config>'",
  "从本地 felix86 配置文件中读取一个配置项。格式为 '<group.config>'")
X("Set a config value and store it into the local felix86 configuration file. Format is '<group.config>=<value>'",
  "设置一个配置项并保存到本地 felix86 配置文件。格式为 '<group.config>=<value>'")
X("Print system info", "显示系统信息")
X("Print the emulator configurations", "显示模拟器的配置项")
X("Kill all open emulator instances", "结束所有正在运行的模拟器实例")
X("Register the emulator in binfmt_misc so that x86-64 executables can run without prepending the emulator path",
  "将模拟器注册到 binfmt_misc，之后可以直接运行 x86-64 程序，无需在命令前加上模拟器路径")
X("Register the emulator in binfmt_misc so that x86-64 executables can run without prepending the emulator path. Also enables the credentials "
  "flag, which allows for running privileged executables (e.g. setuid programs) with the emulator. This may have security implications, read: "
  "https://felix86.com/docs/users/troubleshooting#privileged-executables-dont-work",
  "将模拟器注册到 binfmt_misc，之后可以直接运行 x86-64 程序，无需在命令前加上模拟器路径。同时启用 credentials 标志，"
  "使模拟器能够运行需要特权的程序（如 setuid 程序）。这可能带来安全风险，详见："
  "https://felix86.com/docs/users/troubleshooting#privileged-executables-dont-work")
X("Check if we are correctly registered in binfmt_misc, returns 0 if ok", "检查模拟器是否已正确注册到 binfmt_misc，正常时返回 0")
X("Unregister the emulator from binfmt_misc", "从 binfmt_misc 中注销模拟器")
X("Start just the log server in the background", "仅在后台启动日志服务器")

X("Rootfs path not set in %s\n", "%s 中尚未设置 rootfs 路径\n")
X("Set it using `sudo felix86 --set-config general.rootfs_path=/path/to/rootfs`.\n",
  "请使用 `sudo felix86 --set-config general.rootfs_path=/path/to/rootfs` 进行设置。\n")
X("Consult the installation guide: https://felix86.com/docs/users/installation-guide/\n\n",
  "请参阅安装指南：https://felix86.com/docs/users/installation-guide/\n\n")
X("If you don't have an x86 rootfs, you can use the rootfs installer script to download and install one:\n",
  "如果你还没有 x86 rootfs，可以使用 rootfs 安装脚本下载并安装：\n")
X("Please install fastfetch for more information\n", "安装 fastfetch 后可查看更多信息\n")
X("Failed to get info from fastfetch\n", "无法从 fastfetch 获取信息\n")
X("Killed process %d\n", "已结束进程 %d\n")
X("Failed to kill process %d: %s\n", "无法结束进程 %d：%s\n")
X("Rootfs path %s does not exist\n", "rootfs 路径 %s 不存在\n")
X("Rootfs path %s is not a directory\n", "rootfs 路径 %s 不是一个目录\n")
X("Couldn't find a shell inside the rootfs\n", "在 rootfs 中找不到 shell\n")
X("Could not determine home directory\n", "无法确定用户主目录\n")
X("Failed to chdir to %s\n", "无法进入目录 %s\n")
X("Failed to start %s, error: %s\n", "无法启动 %s，错误：%s\n")
X("expected argument to get-config to be in the format of '<GROUP.CONFIG>'", "get-config 的参数格式应为 '<GROUP.CONFIG>'")
X("%s.%s is not a valid config tuple", "%s.%s 不是有效的配置项")
X("Setting config requires root permissions. Please re-run with sudo.\n", "修改配置需要 root 权限，请使用 sudo 重新运行。\n")
X("expected argument to set-config to be in the format of '<GROUP.CONFIG>=<VALUE>'", "set-config 的参数格式应为 '<GROUP.CONFIG>=<VALUE>'")
X("%s.%s is not a valid config tuple, or %s is not a valid value for the configuration", "%s.%s 不是有效的配置项，或者 %s 不是该配置项的有效值")
X("Credentials flag enabled. Setuid x86 binaries will run with elevated privileges.\n",
  "已启用 credentials 标志。设置了 setuid 的 x86 程序将以提升后的权限运行。\n")
X("These are the configurations for felix86\n", "以下是 felix86 的配置项\n")
X("You may edit %s or set the corresponding environment variable\n", "你可以编辑 %s，或设置对应的环境变量\n")
X("{} {} = {}, source: {}\n", "{} {} = {}，来源：{}\n")
X("Executable path not specified", "未指定可执行文件路径")
X("Executable %s does not exist", "可执行文件 %s 不存在")
X("Executable not inside rootfs, couldn't canonicalize path %s: %s", "可执行文件不在 rootfs 中，且无法解析路径 %s：%s")
X("Executable not inside rootfs", "可执行文件不在 rootfs 中")
X("This is a privileged executable but the emulator isn't installed in binfmt_misc, might run into problems. Run `felix86 -b` to install it, "
  "make sure to remove other x86/x86-64 emulators from binfmt_misc",
  "这是一个需要特权的程序，但模拟器尚未注册到 binfmt_misc，运行时可能会出现问题。请运行 `felix86 -b` 进行注册，"
  "并确保已从 binfmt_misc 中移除其他 x86/x86-64 模拟器")

X("Config file %s does not exist and cannot be created without root permissions. "
  "Run `sudo felix86 --set-config general.rootfs_path=<path>` to create it.",
  "配置文件 %s 不存在，且没有 root 权限无法创建。请运行 `sudo felix86 --set-config general.rootfs_path=<path>` 来创建它。")
X("Default value", "默认值")
X("Environment variable", "环境变量")
X("Configuration file ", "配置文件 ")
X("Profile ", "配置方案 ")

X("I need root permissions to register felix86 in binfmt_misc, please re-run with root permissions as `sudo -E felix86 -b`\n",
  "将 felix86 注册到 binfmt_misc 需要 root 权限，请使用 `sudo -E felix86 -b` 重新运行\n")
X("I need root permissions to register felix86 in binfmt_misc, please re-run with root permissions as `sudo -E felix86 -B`\n",
  "将 felix86 注册到 binfmt_misc 需要 root 权限，请使用 `sudo -E felix86 -B` 重新运行\n")
X("I need root permissions to unregister felix86 from binfmt_misc, please re-run with root permissions as `sudo -E felix86 -u`\n",
  "从 binfmt_misc 注销 felix86 需要 root 权限，请使用 `sudo -E felix86 -u` 重新运行\n")
X("Unregistered felix86 from binfmt_misc for x86-64 apps\n", "已从 binfmt_misc 注销 felix86（x86-64 程序）\n")
X("Unregistered felix86 from binfmt_misc for i386 apps\n", "已从 binfmt_misc 注销 felix86（i386 程序）\n")
X("felix86 successfully unregistered from binfmt_misc\n", "felix86 已成功从 binfmt_misc 注销\n")
X("Failed to find a binfmt.d directory to put felix86.conf in, the registration will not persist after a reboot\n",
  "找不到可以存放 felix86.conf 的 binfmt.d 目录，重启后注册将会失效\n")
X("felix86 successfully registered to binfmt_misc\n", "felix86 已成功注册到 binfmt_misc\n")
