#include "syscall.h"
#include "lib.c"

#define NULL  ((void *) 0)

int
main(int argc, char **argv)
{
    if (argc < 2) {
        Nputs("Error de cantidad de argumentos\n");
        Exit(-1);
    }

    for (int i = 1; i < argc; i++) {
        OpenFileId fid = Open(argv[i]);

        if (fid < 0) {
            Nputs("Archivo inexistente\n");
            Exit(-1);
        }

        char buff;
        while (Read(&buff, 1, fid) == 1) {
            Write(&buff, 1, CONSOLE_OUTPUT);
        }

        Close(fid);
    }
    
    return 0;
}
