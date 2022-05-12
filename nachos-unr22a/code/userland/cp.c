#include "syscall.h"
#include "lib.c"


int
main(int argc, char **argv)
{
    if (argc != 3) {
        Nputs("Error de cantidad de argumentos\n");
        Exit(1);
    }

    OpenFileId fid1 = Open(argv[1]);

    if (fid1 < 0) {
        Nputs("Archivo 1 inexistente\n");
        Exit(1);
    }

    OpenFileId fid2 = Open(argv[2]);

    if (fid2 < 0) {
        Nputs("Archivo 2 inexistente\n");
        Exit(1);
    }

    char buff;
    while (Read(&buff, 1, fid1) == 1) {
        Write(&buff, 1, fid2);
    }

    Close(fid1);
    Close(fid2);

    return 0;
}
