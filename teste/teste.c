#include <stdio.h>
#include "../include/t2fs.h"

void print_dentry(char* label, DIRENT2 *dentry)
{
    printf("\n--%s--\n", label);
    printf("Name: %s\n", dentry->name);
    printf("File type: %d\n", dentry->fileType);
    printf("File size: %d\n", dentry->fileSize);
}

int main()
{
    char name[200];
    identify2(name, 200);
    printf("%s\n", name);

    char cwd[200];
    getcwd2(cwd, 200);

    printf("CWD %s\n", cwd);

    //chdir2("dir1/dir12/..");

    getcwd2(cwd, 200);
    printf("CWD PÓS chdir2 %s\n", cwd);

    int handle = opendir2("/dir1/dir12/../..");

    if( handle != -1 )
    {
        DIRENT2 dentry;

        while( readdir2(handle, &dentry) == 0 )
        {
            print_dentry("Entrada do diretório", &dentry);
        }
    }

    return 0;
}
