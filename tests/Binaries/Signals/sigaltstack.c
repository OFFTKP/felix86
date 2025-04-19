#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include "common.h"

// TODO: make a test where sigaltstack is set and a signal happens inside another signal to make sure the altstack is restored
void signal_handler(int sig, siginfo_t* info, void* ctx) {}

int main() {}