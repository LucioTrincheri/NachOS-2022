#include "syscall.h"

unsigned int Nstrlen (const char *s) {
    if (!s) {
        return 0;
    }

    unsigned int i = 0;
    for(; *(s + i) != '\0'; i++){}
    return i;
}


void Nputs (const char *s) {
    // Console output fid = 1
    Write(s, Nstrlen(s), CONSOLE_OUTPUT);
}

void Nitoa (int n , char *st ) {
    char buff[Nstrlen(st)];
    int i = 0;
    while (n != 0) {
        buff[i++] = (n % 10) + 48;
        n /= 10;
    }
    int j = 0;
    for(j = 0; j < i; j++){
        st[j] = buff[i - j - 1];
    }
    st[j] = '\0';
}