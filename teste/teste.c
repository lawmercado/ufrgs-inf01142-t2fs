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
    //int handle = opendir2("/");
    //int handle = opendir2("/.");
    //int handle = opendir2("/..");
    //int handle = opendir2("/dir1/");
    //int handle = opendir2("/dir1");
    //int handle = opendir2("/dir1/../dir1/dir12");
    //int handle = opendir2("/");

    printf("MK DIR %d\n", mkdir2("dir1/batata"));
    printf("MK DIR %d\n", mkdir2("dir1/batata2"));
    //printf("RM DIR %d\n", rmdir2("dir1/batata"));

    int handle = opendir2("/dir1");

    if( handle != -1 )
    {
        DIRENT2 dentry;

        while( readdir2(handle, &dentry) == 0 )
        {
            print_dentry("Entrada do diretório", &dentry);
        }
    }

    closedir2(handle);

    return 0;
}
