#include <stdio.h>
#include <stdlib.h>

#include "const.h"
#include "debug.h"

#ifdef _STRING_H
#error "Do not #include <string.h>. You will get a ZERO."
#endif

#ifdef _STRINGS_H
#error "Do not #include <strings.h>. You will get a ZERO."
#endif

#ifdef _CTYPE_H
#error "Do not #include <ctype.h>. You will get a ZERO."
#endif

int main(int argc, char **argv)
{
    if(validargs(argc, argv) == -1)
    {
        USAGE(*argv, EXIT_FAILURE);
        return EXIT_FAILURE;
    }
    //case -h
    if((global_options & 0x1) == 1)
    {
        USAGE(*argv, EXIT_SUCCESS);
        return EXIT_SUCCESS;
    }
    //case -d
    else if((global_options & 0x4) == 0x4)
    {
        int ret = decompress(stdin, stdout);
        if(ret == EOF)
        {
            return EXIT_FAILURE;
        }
        else
        {
            return EXIT_SUCCESS;
        }
    }
    //case -c
    else if((global_options & 0x2) == 0x2)
    {
        int bsize = (global_options >> 16);
        bsize = (bsize & 0xFFFF);
        int ret = compress(stdin, stdout, bsize);
        if(ret == EOF)
        {
            return EXIT_FAILURE;
        }
        else
        {
            return EXIT_SUCCESS;
        }
    }
    return EXIT_SUCCESS;
 }

/*
 * Just a reminder: All non-main functions should
 * be in another file not named main.c
 */
