#include "syscall.h"
#include "lib.c"


int
main(int argc, char **argv)
{
    if (argc < 2) {
        Nputs("Error de cantidad de argumentos\n");
        Exit(1);
    }


    for (int i = 1; i < argc; i++) {
        if(Remove(argv[i]) != 0) {
            Nputs("No se puede eliminar: "); 
            Nputs(argv[i]); 
            Nputs("\n"); 
            return -1;   
        }
    }

    return 0;
}
