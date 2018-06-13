#include <stdio.h>
#include <string.h>
#include "../include/t2fs.h"

void ls(char* label, DIR2 handle)
{
    DIRENT2 dentry;

    if( handle != -1 )
    {
        printf("%s\n", label);
        while( readdir2(handle, &dentry) == 0 )
        {
            printf("\tN: '%s' -- T: %d -- S: %d\n", dentry.name, dentry.fileType, dentry.fileSize);
        }
    }
}

char* test_verification_int(int result, int expected)
{
    if( result == expected )
    {
        return "PASSOU";
    }
    else
    {
        return "NÃO PASSOU";
    }
}

char* test_verification_str(char *result, char *expected)
{
    if( strcmp(result, expected) == 0 )
    {
        return "PASSOU";
    }
    else
    {
        return "NÃO PASSOU";
    }
}

int main()
{
    int i;
    int test = 0;
    char nomes[200];
    DIR2 dirs[MAX_NUM_HANDLERS];

    printf("----TESTES DAS FUNÇÕES DE DIRETÓRIO----\n");

    identify2(nomes, 200);
    printf("NOMES: \n%s", nomes);

    printf("\n");

    printf("TESTE: ALOCAÇÃO DE HANDLERS DE DIRETÓRIO. Aloca o máximo de handlers possíveis do mesmo diretório.\n");
    for( i = 0; i < MAX_NUM_HANDLERS; i++ )
    {
        dirs[i] = opendir2("../dir1/dir12/../..");

        if( dirs[i] < 0 )
        {
            break;
        }
    }
    printf("----DEBUG: Todos handlers diretórios alocados... Tentando alocar mais um.\n");
    printf("----RESULTADO: %s (atingiu limite de handlers).\n", test_verification_int(opendir2("/"), -1));

    printf("\n");

    printf("TESTE: DESALOCAÇÃO DE HANDLERS DE DIRETÓRIO. Desalocar todos os handlers, em seguida tentar desalocar handler não alocado.\n");
    for( i = 0; i < MAX_NUM_HANDLERS; i++ )
    {
        if( closedir2(dirs[i]) != 0 )
        {
            break;
        }
    }
    printf("----DEBUG: Todos handlers de diretórios desalocados... Tentando desalocar handle inexistente de diretório.\n");
    printf("----RESULTADO: %s (handler não alocado).\n", test_verification_int(closedir2(9), -1));

    printf("\n");

    printf("TESTE: DESALOCAÇÃO DE HANDLERS DE DIRETÓRIO. Desalocar handler inválido.\n");
    printf("----RESULTADO: %s (handler inválido).\n", test_verification_int(closedir2(10), -1));

    printf("\n");

    printf("TESTE: VERIFICAÇÃO DE PARSING DE CAMINHOS. Todos os diretórios devem resultar na listagem do diretório root.\n");
    dirs[0] = opendir2("/dir1/..");
    ls("----DEBUG: Diretório informado '/dir1/..'", dirs[0]);
    dirs[1] = opendir2("/dir1/../dir1/dir12/../../");
    ls("----DEBUG: Diretório informado '/dir1/../dir1/dir12/../../'", dirs[1]);
    dirs[2] = opendir2("dir1/../dir1/../../");
    ls("----DEBUG: Diretório informado 'dir1/../dir1/../../'", dirs[2]);
    printf("----OBSERVAR: Espera-se que todas as listagens sejam do dir. root.\n");

    closedir2(dirs[0]);
    closedir2(dirs[1]);
    closedir2(dirs[2]);

    printf("\n");

    rmdir2("teste_dir1");
    printf("TESTE: CRIAÇÃO DE DIRETÓRIOS. Cria um diretório. Mostra o conteúdo do mesmo.\n");
    test = mkdir2("teste_dir1");
    dirs[0] = opendir2("teste_dir1");
    ls("----DEBUG: Diretório informado '/teste_dir1'", dirs[0]);
    printf("----RESULTADO: %s (dir. criado).\n", test_verification_int(test, 0));
    printf("----OBSERVAR: Espera-se apenas os diretórios de link.\n");

    printf("\n");

    printf("TESTE: CRIAÇÃO DE DIRETÓRIOS. Diretório já existente e caminho inválido.\n");
    printf("----RESULTADO 1: %s (dir. já existente).\n", test_verification_int(mkdir2("teste_dir1"), -1));
    printf("----RESULTADO 2: %s (caminho inválido).\n", test_verification_int(mkdir2("/dir51/teste_dir1"), -1));

    printf("\n");

    printf("TESTE: REMOVER DIRETÓRIO NÃO VAZIO. Cria um subdiretório no diretório recém criado e tenta remover.\n");
    mkdir2("teste_dir1/teste_dir11");
    printf("----RESULTADO: %s (dir. não vazio).\n", test_verification_int(rmdir2("teste_dir1"), -1));

    rmdir2("teste_dir1/teste_dir11");

    printf("\n");

    printf("TESTE: REMOVER DIRETÓRIO CRIADO. Dir ainda aberto. Tenta remover.\n");
    printf("----RESULTADO: %s (remover diretório ainda aberto).\n", test_verification_int(rmdir2("teste_dir1"), -1));

    closedir2(dirs[0]);

    printf("\n");

    printf("TESTE: REMOVER DIRETÓRIO CRIADO. Remove o diretório e tenta abrir ele.\n");
    printf("----RESULTADO: %s (rem. bem sucedida).\n", test_verification_int(rmdir2("teste_dir1"), 0));

    printf("\n");

    char buffer[100];
    printf("TESTE: MUDAR DE DIRETÓRIO. Abrir relativo e listar conteúdo.\n");
    printf("----RESULTADO 1: %s (mudança bem sucedida).\n", test_verification_int(chdir2("/dir1"), 0));
    getcwd2(buffer, 100);
    printf("----RESULTADO 2: %s (cwd igual ao previsto).\n", test_verification_str(buffer, "/dir1"));
    dirs[0] = opendir2(".");
    ls("----DEBUG: listagem do conteúdo do /dir1", dirs[0]);
    closedir2(dirs[0]);
    printf("----OBSERVAR: Conteúdo deve ser do dir1 (cwd).\n");

    printf("\n");

    return 0;
}
