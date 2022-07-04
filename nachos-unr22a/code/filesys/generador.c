

#include "stdio.h"
int main() {

    FILE *ptr = fopen("bigbig.txt", "w");

    for (unsigned i = 0; i < 5000; i++) {
        fprintf(ptr, "%u", i);
        if (i % 100 == 0) {
            fprintf(ptr, "\n");
        }
    }
    fclose(ptr);
    return 0;
}