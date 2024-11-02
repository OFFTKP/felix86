# Coding convention

Coding convention is currently all over the place because the project started as a C only project and slowly
morphed into a C++ish project. Moving some global functions to classes is welcome. Renaming variables to match coding style is welcome.
Moving towards more idiomatic C++ is welcome, but try to keep code readability in mind. Moving stuff around is welcome if it improves
compilation speed, but keep in mind that locality of behavior is generally preferred.

# Submodules

There will be no submodules in the project. Historically I've seen git repositories get taken down and the submodules no longer work, and then bisecting no longer works. Also git submodules suck in general.

# Idiomatic commit messages

There's no commit message guide to follow for this project. Describe what your commit does in few words. If it's a complex commit, add a longer description. Avoid *only* describing things in PR descriptions as those can get lost with time.

# OS specific libraries or code

You can use POSIX-only and Linux-only code. This is an emulator that targets Linux only. So if something is technically not POSIX-standard but works on Linux, use it. An example is the /proc/ filesystem which we use for /proc/self/fd/ to find the path
of file descriptors.

# Sandboxing

felix86 makes a faithful attempt to sandbox the emulated application, but should *not* be considered a security application and has absolutely no security guarantees.
That being said, we try to make sure that syscalls that modify files only do so on files inside the sandbox (inside the rootfs).