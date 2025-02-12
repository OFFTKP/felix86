#include <stdio.h>

void test(void*);

int main() {
    long xmms[32];
    test(xmms);
    for (int i = 0; i < 32; i++) {
        printf("%016lx\n", xmms[i]);
    }
    long sum1 = 0;
    long sum2 = 0;
    for (int i = 0; i < 8; i++) {
        sum1 += xmms[i];
        sum2 += xmms[i + 8];
    }
    printf("checksums: %lx %lx", sum1, sum2);
    return 0;
}