#include <stdio.h>
#include "felix86/common/log.h"
#include "felix86/common/prompt.h"

u32 prompt_yn_question(const char* question) {
    int response;
    do {
        printf("%s [y/n]:\n", question);
        response = getchar();
        if (getchar() != '\n') {
            while (getchar() != '\n')
                ;
        } else {
            if (response == 'y') {
                return 1;
            } else if (response == 'n') {
                return 0;
            }
        }

        printf("Please answer with 'y' or 'n'\n");
    } while (true);

    ERROR("Unreachable code");
}