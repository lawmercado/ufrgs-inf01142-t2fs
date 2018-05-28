#include <stdio.h>
#include "../include/t2fs.h"

int main()
{
    char name[200];

    identify2(name, 200);

    printf("%s\n", name);

    int handle = opendir2("/");

    chdir2("/oie/ls/");

    return 0;
}
