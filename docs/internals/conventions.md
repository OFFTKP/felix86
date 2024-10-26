# Coding convention

Coding convention is currently all over the place because the project started as a C only project and slowly
morphed into a C++ish project. Moving some global functions to classes is welcome. Renaming variables to match coding style is welcome.

This project is generally not interested in The Correct Way of Doing Things :tm: unless if it's proven to be
a better way of doing things.

This means the following are advised to be avoided until proven to be necessary:

- use of pointless iterator mumbojumbo like std::for_each when a simple for loop is just fine
- use of any fresh modern C++2x techniques that I have to look up what they do on cppreference every time I review code, you shouldn't need anything that isn't \<filesystem\> and C++11 with a few exceptions. To name a few:
    - coroutines/concepts/modules
    - \<ranges\> for the most part
    - bit_cast when memcpy is just fine
    - streams. stringstream, fstream, cout. they are garbage. Use FILE*, fmt::format.
    - \<limits\>
    - [[likely]]/[[unlikely]] without profiling
    - stuff like `if (T x = some_stuff; some_expression)`
- C++-style casts, they provide no benefit (with the exception of dynamic_cast, but no RTTI here) to C casts in most cases. Their supposed benefit is compile time checking, but this is useless 9/10 times and in the 1/10 times it's useful is in templated code which we generally avoid
- making everything a function / excessive separation of concerns - we prefer locality of behavior, we don't have to jump between 4 files and 7 functions to understand what something does
- "Don't repeat yourself" and similar fashionable idioms, duplicate code is welcome if it makes everything more readable, coalesce code into functions at your own discresion
- any TMP use / abuse of generics, especially when no significant runtime benefit
- visitor pattern, use a switch
- excessive polymorphism
- exceptions/rtti
- macros are welcome so long as they make the code more readable or succinct (ie X Macros)
- too much abstraction for no obvious benefit - this is a very narrowly focused project so the code need not be too abstract
- keyword spam when it has no immediate benefit. For example defining functions that will never realistically run at compile time as `constexpr`. Same goes for `if constexpr`. Either use `consteval`, don't use either, or make sure the function is used sometimes during compile-time and sometimes during runtime.
- excessive goto usage. We don't need `goto` for jumping to cleanup code since we have destructors. Goto for exiting nested loops is fine

These are my personal recommendations of things to avoid. These can be ignored for sufficient reason.

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