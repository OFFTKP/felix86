#pragma once

struct Sudo {
    static bool hasPermissions();

    static bool dropPermissions();

    [[noreturn]] static void requestPermissions(int argc, char** argv);
};