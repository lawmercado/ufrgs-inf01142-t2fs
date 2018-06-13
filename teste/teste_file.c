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
    int hdir, test = 0;
    char nomes[200];
    FILE2 files[MAX_NUM_HANDLERS];

    printf("----TESTES DAS FUNÇÕES DE ARQUIVO----\n");

    identify2(nomes, 200);
    printf("NOMES: \n%s", nomes);

    // Abre o diretório root
    hdir = opendir2("/");

    printf("\n");

    printf("TESTE: ALOCAÇÃO DE HANDLERS DE ARQUIVO. Aloca o máximo de handlers possíveis do mesmo arquivo.\n");
    for( i = 0; i < MAX_NUM_HANDLERS; i++ )
    {
        files[i] = open2("file3");

        if( files[i] < 0 )
        {
            break;
        }
    }
    printf("----DEBUG: Todos handlers de arquivos alocados... Tentando alocar mais um.\n");
    printf("----RESULTADO: %s (atingiu limite de handlers).\n", test_verification_int(open2("file3"), -1));

    printf("\n");
    printf("TESTE: DESALOCAÇÃO DE HANDLERS DE ARQUIVO. Desalocar todos os handlers, em seguida tentar desalocar handler não alocado.\n");
    for( i = 0; i < MAX_NUM_HANDLERS; i++ )
    {
        if( close2(files[i]) != 0 )
        {
            break;
        }
    }
    printf("----DEBUG: Todos handlers de arquivos desalocados... Tentando desalocar handle inexistente de diretório.\n");
    printf("----RESULTADO: %s (handler não alocado).\n", test_verification_int(close2(9), -1));

    printf("\n");

    printf("TESTE: DESALOCAÇÃO DE HANDLERS DE ARQUIVO. Desalocar handler inválido.\n");
    printf("----RESULTADO: %s (handler inválido).\n", test_verification_int(close2(10), -1));

    printf("\n");

    delete2("teste_file1");
    printf("TESTE: CRIAÇÃO DE ARQUIVO. Cria um arquivo. Mostra o conteúdo do diretório em que foi criado.\n");
    test = create2("teste_file1");
    ls("----DEBUG: Diretório informado '/'", hdir);
    printf("----RESULTADO: %s (arq. criado).\n", test_verification_int(test, 0));
    printf("----OBSERVAR: Espera-se que o arquivo apareça na listagem do dir '/'.\n");

    printf("\n");

    printf("TESTE: CRIAÇÃO DE ARQUIVO. Tenta criar arquivo com mesmo nome de dir ou arquivo.\n");
    test = create2("teste_file1") == create2("dir1");
    ls("----DEBUG: Diretório informado '/'", hdir);
    printf("----RESULTADO: %s (arq. não criado).\n", test_verification_int(test, 1));

    printf("\n");

    files[0] = open2("teste_file1");
    char bufferLeitura[265217];
    char bufferEscrita[265217] = "------0123456789";

    printf("TESTE: LEITURA DO ARQUIVO. Arquivo está vazio.\n");
    seek2(files[0], 0);
    printf("----RESULTADO 1: %s (0 bytes de char. lidos).\n", test_verification_int(read2(files[0], bufferLeitura, 16), 0));
    seek2(files[0], 0);
    printf("----RESULTADO 2: %s (nada lido no buffer).\n", test_verification_str(bufferLeitura, ""));

    printf("\n");

    printf("TESTE: ESCRITA EM ARQUIVO. Escreve 16 bytes no arquivo.\n");
    strcpy(bufferLeitura, "");
    printf("----DEBUG: Escrevendo '%s' em 'teste_file1'.\n", bufferEscrita);
    seek2(files[0], 0);
    printf("----RESULTADO 1: %s (escrita realizada).\n", test_verification_int(write2(files[0], bufferEscrita, 16), 16));
    seek2(files[0], 0);
    printf("----RESULTADO 2: %s (16 bytes de char. lidos).\n", test_verification_int(read2(files[0], bufferLeitura, 16), 16));
    seek2(files[0], 0);
    printf("----RESULTADO 3: %s (conteúdo lido igual ao recém escrito).\n", test_verification_str(bufferLeitura, bufferEscrita));

    printf("\n");

    printf("TESTE: PONTEIRO DO ARQUIVO. Escreve 16 bytes no fim do arquivo, indo para o fim do arquivo com seek2(handle, -1).\n");
    strcpy(bufferLeitura, "");
    seek2(files[0], -1);
    printf("----DEBUG: Escrevendo '%s' em 'teste_file1'.\n", bufferEscrita);
    printf("----RESULTADO 1: %s (escrita realizada no fim do arq.).\n", test_verification_int(write2(files[0], bufferEscrita, 16), 16));
    seek2(files[0], 0);
    printf("----RESULTADO 2: %s (32 bytes de char. lidos).\n", test_verification_int(read2(files[0], bufferLeitura, 32), 32));
    seek2(files[0], 0);
    printf("----OBSERVAR 1: '%s' == '%s%s'?\n", bufferLeitura, bufferEscrita, bufferEscrita);
    ls("----DEBUG: Diretório informado '/'", hdir);
    printf("----OBSERVAR 2: tamanho do arquivo 'teste_file1' deve ser 32.\n");

    printf("\n");

    printf("TESTE: PONTEIRO DO ARQUIVO. Escreve 16 bytes no meio do arquivo.\n");
    strcpy(bufferLeitura, "");
    printf("----DEBUG: Escrevendo '%s' em 'teste_file1'.\n", bufferEscrita);
    seek2(files[0], 8);
    printf("----RESULTADO 1: %s (escrita realizada no meio arq.).\n", test_verification_int(write2(files[0], bufferEscrita, 16), 16));
    seek2(files[0], 0);
    printf("----RESULTADO 2: %s (32 bytes de char. lidos).\n", test_verification_int(read2(files[0], bufferLeitura, 32), 32));
    seek2(files[0], 0);
    printf("----OBSERVAR 1: '%s' tem '%s' escrito no meio?\n", bufferLeitura, bufferEscrita);
    ls("----DEBUG: Diretório informado '/'", hdir);
    printf("----OBSERVAR 2: tamanho do arquivo 'teste_file1' deve continuar 32.\n");

    printf("\n");

    printf("TESTE: ALOCAÇÃO DE BLOCOS PARA ARQUIVO. Escreve 4096 bytes no arquivo direto\n");
    strcpy(bufferLeitura, "");
    strcpy(bufferEscrita, "------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789");
    seek2(files[0], 0);
    printf("----RESULTADO 1: %s (escrita realizada).\n", test_verification_int(write2(files[0], bufferEscrita, 4096), 4096));
    seek2(files[0], 0);
    printf("----RESULTADO 2: %s (4096 bytes de char. lidos).\n", test_verification_int(read2(files[0], bufferLeitura, 4096), 4096));
    seek2(files[0], 0);
    ls("----DEBUG: Diretório informado '/'", hdir);
    printf("----OBSERVAR 2: tamanho do arquivo 'teste_file1' deve ser 4096.\n");
    bufferLeitura[4096] = '\0';
    printf("----DEBUG: O que foi escrito no bloco: \n%s\n", bufferLeitura);

    printf("\n");

    printf("TESTE: TRUNCAGEM DE ARQUIVO\n");
    strcpy(bufferLeitura, "");
    seek2(files[0], 16);
    printf("----RESULTADO 1: %s (truncagem bem sucedida).\n", test_verification_int(truncate2(files[0]), 0));
    seek2(files[0], 0);
    printf("----RESULTADO 2: %s (16 bytes de char. lidos).\n", test_verification_int(read2(files[0], bufferLeitura, 32), 16));
    seek2(files[0], 0);
    bufferLeitura[16] = '\0';
    ls("----DEBUG: Diretório informado '/'", hdir);
    printf("----OBSERVAR: tamanho do arquivo 'teste_file1' deve ser 16.\n");

    printf("\n");

    printf("TESTE: PONTEIRO DO ARQUIVO, PÓS TRUNCAGEM. Escreve 16 bytes no fim do arquivo, indo para o fim do arquivo com seek2(handle, -1).\n");
    strcpy(bufferLeitura, "");
    strcpy(bufferEscrita, "------0123456789");
    printf("----DEBUG: Escrevendo '%s' em 'teste_file1'.\n", bufferEscrita);
    seek2(files[0], -1);
    printf("----RESULTADO 1: %s (escrita realizada no fim do arq.).\n", test_verification_int(write2(files[0], bufferEscrita, 16), 16));
    seek2(files[0], 0);
    printf("----RESULTADO 2: %s (32 bytes de char. lidos).\n", test_verification_int(read2(files[0], bufferLeitura, 32), 32));
    seek2(files[0], 0);
    bufferLeitura[32] = '\0';
    printf("----OBSERVAR 1: '%s' == '%s%s'?\n", bufferLeitura, bufferEscrita, bufferEscrita);
    ls("----DEBUG: Diretório informado '/'", hdir);
    printf("----OBSERVAR 2: tamanho do arquivo 'teste_file1' deve ser 32.\n");

    printf("\n");

    printf("TESTE: ALOCAÇÃO DE BLOCOS PARA ARQUIVO, TESTA A INDIREÇÃO. Escreve 265216 bytes no arquivo de maneira continua (de 16 em 16 bytes), a partir do início.\n");
    strcpy(bufferLeitura, "");
    strcpy(bufferEscrita, "------0123456789");

    seek2(files[0], 0);
    int count = 0;
    for( i = 0; i < 16576; i++ )
    {
        count += write2(files[0], bufferEscrita, 16);

        if( count % 16 != 0 )
        {
            break;
        }
    }

    printf("----RESULTADO 1: %s (escrita realizada).\n", test_verification_int(count, 265216));
    seek2(files[0], 0);
    printf("----RESULTADO 2: %s (265216 bytes de char. lidos).\n", test_verification_int(read2(files[0], bufferLeitura, 265216), 265216));
    seek2(files[0], 0);
    ls("----DEBUG: Diretório informado '/'", hdir);
    printf("----OBSERVAR: tamanho do arquivo 'teste_file1' deve ser 265216.\n");

    printf("\n");

    printf("TESTE: DELEÇÃO DE ARQUIVO\n");
    printf("----RESULTADO 1: %s (arq. aberto).\n", test_verification_int(delete2("teste_file1"), -1));
    close2(files[0]);
    printf("----RESULTADO 2: %s (deleção bem sucedida).\n", test_verification_int(delete2("teste_file1"), 0));
    printf("----RESULTADO 3: %s (arquivo já removido/caminho inválido).\n", test_verification_int(delete2("teste_file1"), -1));
    ls("----DEBUG: Diretório informado '/'", hdir);
    printf("----OBSERVAR: arquivo 'teste_file1' não pode existir.\n");

    closedir2(hdir);

    return 0;
}
