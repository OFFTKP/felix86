#include <stdio.h>

void test(void*);

int main() {
    long xmms[32];
    test(xmms);
    for (int i = 0; i < 32; i++) {
        printf("%016lx\n", xmms[i]);
    }
    return 0;
}