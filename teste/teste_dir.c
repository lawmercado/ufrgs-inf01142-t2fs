#include <stdio.h>
#include "../include/t2fs.h"

void ls(char* label, DIR2 handle)
{
    DIRENT2 dentry;

    if( handle != -1 )
    {
        printf("%s\n", label);
        while( readdir2(handle, &dentry) == 0 )
        {
            printf("N: '%s' -- T: %d -- S: %d\n", dentry.name, dentry.fileType, dentry.fileSize);
        }
    }
}

int main()
{
    int i;
    int erro, test = 0;
    DIR2 dirs[MAX_NUM_HANDLERS];

    erro = 0;
    printf("TESTE: ALOCAÇÃO DE HANDLERS DE DIRETÓRIO. Aloca o máximo de handlers possíveis do mesmo diretório.\n");
    for( i = 0; i < MAX_NUM_HANDLERS; i++ )
    {
        dirs[i] = opendir2("../dir1/dir12/../..");

        if( dirs[i] < 0 )
        {
            erro = 1;
            break;
        }
    }
    printf("----DEBUG: Todos diretórios alocados... Tentando alocar mais um.\n");
    printf("----RESULTADO: %d, val. esperado -1 (atingiu limite de handlers).\n", opendir2("/"));
    printf("----ERRO NO PROCESSO?: %d, val. esperado 0 (todos dirs. alocados).\n", erro);

    printf("\n");

    erro = 0;
    printf("TESTE: DESALOCAÇÃO DE HANDLERS DE DIRETÓRIO. Desalocar todos os handlers, em seguida tentar desalocar handler não alocado.\n");
    for( i = 0; i < MAX_NUM_HANDLERS; i++ )
    {
        if( closedir2(dirs[i]) != 0 )
        {
            erro = 1;
            break;
        }
    }
    printf("----DEBUG: Todos diretórios desalocados... Tentando desalocar handle inexistente de diretório.\n");
    printf("----RESULTADO: %d, val. esperado -1 (handler não alocado).\n", closedir2(9));
    printf("----ERRO NO PROCESSO?: %d, val. esperado 0 (todos dirs. desalocados).\n", erro);

    printf("\n");

    erro = 0;
    printf("TESTE: VERIFICAÇÃO DE PARSING DE CAMINHOS. Todos os diretórios devem resultar na listagem do diretório root.\n");
    dirs[0] = opendir2("/dir1/..");
    ls("----DEBUG: Diretório informado '/dir1/..'", dirs[0]);
    dirs[1] = opendir2("/dir1/../dir1/dir12/../../");
    ls("----DEBUG: Diretório informado '/dir1/../dir1/dir12/../../'", dirs[1]);
    dirs[2] = opendir2("dir1/../dir1/../../");
    ls("----DEBUG: Diretório informado 'dir1/../dir1/../../'", dirs[2]);
    printf("----RESULTADO: Espera-se que todas as listagens sejam do dir. root.\n");

    closedir2(dirs[0]);
    closedir2(dirs[1]);
    closedir2(dirs[2]);

    printf("\n");

    rmdir2("teste_dir1");
    printf("TESTE: CRIAÇÃO DE DIRETÓRIOS. Cria um diretório. Mostra o conteúdo do mesmo.\n");
    test = mkdir2("teste_dir1");
    dirs[0] = opendir2("teste_dir1");
    ls("----DEBUG: Diretório informado '/teste_dir1'", dirs[0]);
    printf("----RESULTADO 1: %d, 0 (dir. criado).\n", test);
    printf("----RESULTADO 2: Espera-se apenas os diretórios de link.\n");

    printf("\n");

    printf("TESTE: CRIAÇÃO DE DIRETÓRIOS. Diretório já existente e caminho inválido.\n");
    printf("----RESULTADO 1: %d, val. esperado -1 (dir. já existente).\n", mkdir2("teste_dir1"));
    printf("----RESULTADO 2: %d, val. esperado -1 (caminho inválido).\n", mkdir2("/dir51/teste_dir1"));

    printf("\n");

    printf("TESTE: REMOVER DIRETÓRIO NÃO VAZIO. Cria um subdiretório no diretório recém criado e tenta remover.\n");
    mkdir2("teste_dir1/teste_dir11");
    printf("----RESULTADO: %d, val. esperado -1 (dir. não vazio).\n", rmdir2("teste_dir1"));

    rmdir2("teste_dir1/teste_dir11");

    printf("\n");

    printf("TESTE: REMOVER DIRETÓRIO CRIADO. Dir ainda aberto. Tenta remover.\n");
    printf("----RESULTADO: %d, val. esperado -1 (remover diretório ainda aberto).\n", rmdir2("teste_dir1"));

    closedir2(dirs[0]);

    printf("\n");

    printf("TESTE: REMOVER DIRETÓRIO CRIADO. Remove o diretório e tenta abrir ele.\n");
    printf("----RESULTADO: %d, val. esperado 0 (rem. bem sucedida).\n", rmdir2("teste_dir1"));

    printf("\n");

    char buffer[100];
    printf("TESTE: MUDAR DE DIRETÓRIO. Abrir relativo e listar conteúdo.\n");
    printf("----RESULTADO 1: %d, val. esperado 0 (rem. bem sucedida).\n", chdir2("/dir1"));
    dirs[0] = opendir2(".");
    ls("----DEBUG: listagem do conteúdo do /dir1", dirs[0]);
    closedir2(dirs[0]);
    getcwd2(buffer, 100);
    printf("----RESULTADO 2: Conteúdo deve ser do dir1.\n");
    printf("----RESULTADO 3: cwd = %s deve ser /dir1.\n", buffer);

    return 0;
}
