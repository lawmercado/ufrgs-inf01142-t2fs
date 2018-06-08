#include "../include/t2fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*-----------------------------------------------------------------------------
Função: Dado um buffer, lê um valor inteiro do mesmo (expresso em Little Endian)

Entra:
    buffer -> buffer com os valores
    start -> índice onde começa o valor no buffer
	size -> tamanho do valor a ser lido

Saída:
    O valor presente no buffer.
-----------------------------------------------------------------------------*/
int __get_value_from_buffer(BYTE *buffer, int start, int size)
{
    int i = 0;
    unsigned int value = 0;

    for( i = 0; i < size; i++ )
    {
        value += buffer[start + i] << (i*8);
    }

    return value;
}

/*-----------------------------------------------------------------------------
Função: Converte um valor inteiro para um buffer com os bytes (como Little Endian)

Entra:
    value -> valor a ser convertido
    size -> tamanho do buffer desejado

Saída:
    Buffer expressando o valor.
-----------------------------------------------------------------------------*/
BYTE* __convert_value_to_buffer(unsigned int value, int size)
{
    BYTE *buffer = (BYTE*)malloc(sizeof(BYTE) * (size + 1));

    if( size > 2 )
    {
        buffer[0] = value & 0x00ff;
        buffer[1] = (value >> 8) & 0x00ff;
        buffer[2] = (value >> 16) & 0x00ff;
        buffer[3] = (value >> 24) & 0x00ff;
    }
    else
    {
        buffer[0] = value & 0x00ff;
        buffer[1] = (value >> 8) & 0x00ff;
    }

    buffer[size] = '\0';

    return buffer;
}

/*-----------------------------------------------------------------------------
Função: Cria um 't2fs_superbloco' a partir do buffer

Entra:
    buffer -> buffer com os dados para a estrutura

Saída:
    A struct criada a partir dos dados.
-----------------------------------------------------------------------------*/
struct t2fs_superbloco* buffer_to_superblock(BYTE *buffer, int start)
{
    struct t2fs_superbloco *sb = NULL;
    int i = 0;

    sb = (struct t2fs_superbloco*)malloc(sizeof(struct t2fs_superbloco));

    for( i = 0; i < 4; i++ )
    {
        sb->id[i] = (char)buffer[i];
    }

    sb->version = __get_value_from_buffer(buffer, start + 4, 2);
    sb->superblockSize = __get_value_from_buffer(buffer, start + 6, 2);
    sb->freeBlocksBitmapSize = __get_value_from_buffer(buffer, start + 8, 2);
    sb->freeInodeBitmapSize = __get_value_from_buffer(buffer, start + 10, 2);
    sb->inodeAreaSize = __get_value_from_buffer(buffer, start + 12, 2);
    sb->blockSize = __get_value_from_buffer(buffer, start + 14, 2);
    sb->diskSize = __get_value_from_buffer(buffer, start + 16, 4);

    return sb;
}

/*-----------------------------------------------------------------------------
Função: Cria um 't2fs_inode' a partir do buffer

Entra:
    buffer -> buffer com os dados para a estrutura
    start -> começo do dado no buffer

Saída:
    A struct criada a partir dos dados.
-----------------------------------------------------------------------------*/
struct t2fs_inode* buffer_to_inode(BYTE *buffer, int start)
{
    struct t2fs_inode *inode = NULL;
    inode = (struct t2fs_inode*)malloc(sizeof(struct t2fs_inode));

    inode->blocksFileSize = __get_value_from_buffer(buffer, start + 0, 4);
    inode->bytesFileSize = __get_value_from_buffer(buffer, start + 4, 4);
    inode->dataPtr[0] = __get_value_from_buffer(buffer, start + 8, 4);
    inode->dataPtr[1] = __get_value_from_buffer(buffer, start + 12, 4);
    inode->singleIndPtr = __get_value_from_buffer(buffer, start + 16, 4);
    inode->doubleIndPtr = __get_value_from_buffer(buffer, start + 20, 4);

    return inode;
}

/*-----------------------------------------------------------------------------
Função: Cria um 't2fs_record' a partir do buffer

Entra:
    buffer -> buffer com os dados para a estrutura
    start -> começo do dado no buffer

Saída:
    A struct criada a partir dos dados.
-----------------------------------------------------------------------------*/
struct t2fs_record* buffer_to_record(BYTE *buffer, int start)
{
    struct t2fs_record *record = NULL;
    int i;

    record = (struct t2fs_record*)malloc(sizeof(struct t2fs_record));

    record->TypeVal = buffer[start + 0];
    record->inodeNumber = __get_value_from_buffer(buffer, start + 60, 4);

    for( i = 0; i < 58; i++ )
    {
        record->name[i] = buffer[start + 1 + i];
    }

	record->name[58] = '\0';

    return record;
}

/*-----------------------------------------------------------------------------
Função: Cria um valor DWORD a partir de um buffer

Entra:
    buffer -> buffer com o dado
    start -> começo do dado no buffer

Saída:
    O valor gerado.
-----------------------------------------------------------------------------*/
DWORD buffer_to_dword(BYTE *buffer, int start)
{
    return __get_value_from_buffer(buffer, start, 4);
}

/*-----------------------------------------------------------------------------
Função: Cria um buffer a partir de um 't2fs_inode'

Entra:
    inode -> inode a ser transformado

Saída:
    O buffer representando a estrutura.
-----------------------------------------------------------------------------*/
BYTE* inode_to_buffer(struct t2fs_inode *inode)
{
    BYTE *buffer = NULL;
    int i;

    buffer = (BYTE*)calloc(sizeof(struct t2fs_inode), sizeof(BYTE));

    for( i = 0; i < 4; i++ )
    {
        buffer[i] = __convert_value_to_buffer(inode->blocksFileSize, 4)[i];
        buffer[4 + i] = __convert_value_to_buffer(inode->bytesFileSize, 4)[i];
        buffer[8 + i] = __convert_value_to_buffer(inode->dataPtr[0], 4)[i];
        buffer[12 + i] = __convert_value_to_buffer(inode->dataPtr[1], 4)[i];
        buffer[16 + i] = __convert_value_to_buffer(inode->singleIndPtr, 4)[i];
        buffer[20 + i] = __convert_value_to_buffer(inode->doubleIndPtr, 4)[i];
    }

    return buffer;
}

/*-----------------------------------------------------------------------------
Função: Cria um buffer a partir de um 't2fs_record'

Entra:
    record -> record a ser transformado

Saída:
    O buffer representando a estrutura.
-----------------------------------------------------------------------------*/
BYTE* record_to_buffer(struct t2fs_record *record)
{
    BYTE *buffer = NULL;
    int i;

    buffer = (BYTE*)calloc(sizeof(struct t2fs_record), sizeof(BYTE));

    buffer[0] = __convert_value_to_buffer(record->TypeVal, 4)[0];

    for( i = 0; i < RECORD_NAME_SIZE; i++ )
    {
        buffer[1 + i] = record->name[i];
    }

    for( i = 0; i < 4; i++ )
    {
        buffer[60 + i] = __convert_value_to_buffer(record->inodeNumber, 4)[i];
    }

    return buffer;
}

/*-----------------------------------------------------------------------------
Função: Cria um buffer a partir de um 'DWORD'

Entra:
    dword -> dword a ser transformado

Saída:
    O buffer representando a estrutura.
-----------------------------------------------------------------------------*/
unsigned char* dword_to_buffer(DWORD dword)
{
    return __convert_value_to_buffer(dword, 4);
}

/*-----------------------------------------------------------------------------
Função: Gera o caminho absoluto (sem simplicifação)

Entra:
    path -> caminho a ser processado
    cwdPath -> caminho corrente

Saída:
    Se a operação foi realizada corretamente, o caminho absoluto
    Se houver algum erro, NULL.
-----------------------------------------------------------------------------*/
char* __get_abspath(char *path, char *cwdPath)
{
    char *auxPathname, *absPathname;

    auxPathname = strdup(path);

    absPathname = (char*)calloc(strlen(path) + strlen(cwdPath) + 2, sizeof(char));
    strcpy(absPathname, "");

    if( strlen(path) > 0 )
    {
        if( path[0] == '/' )
        {
            free(absPathname);
            absPathname = auxPathname;

            // Caso seja o dir. raiz
            if( strlen(path) == 1 )
            {
                return absPathname;
            }
        }
        else
        {
            strcpy(absPathname, cwdPath);
            if( cwdPath[strlen(cwdPath) - 1] != '/' )
            {
                strcat(absPathname, "/");
            }

            strcat(absPathname, auxPathname);
        }

        if( absPathname[strlen(absPathname) - 1] == '/' )
        {
            absPathname[strlen(absPathname) - 1] = '\0';
        }

        return absPathname;
    }

    return NULL;
}

/*-----------------------------------------------------------------------------
Função: Conta o número de records num dado path

Entra:
    absPathname -> caminho absoluto ser processado

Saída:
    Contagem dos diretórios.
-----------------------------------------------------------------------------*/
int __count_records(char *absPathname)
{
    int numRecords = 0;
    int i;

    for( i = 0; i < strlen(absPathname); i++ )
    {
        if( absPathname[i] == '/' )
        {
            numRecords++;
        }
    }

    return numRecords;
}

/*-----------------------------------------------------------------------------
Função: A partir do caminho absoluto, cria um vetor dos nomes dos records

Entra:
    absPathname -> caminho absoluto ser processado

Saída:
    Vetor de nomes dos records.
-----------------------------------------------------------------------------*/
char** __get_path_array(char *absPathname)
{
    char **records = NULL;
    char *currSlash = absPathname, *nextSlash = NULL;
    int numRecords = __count_records(absPathname);
    int i, length;

    records = (char**)calloc(numRecords, sizeof(char*));

    for( i = 0; i < numRecords; i++ )
    {
        records[i] = (char *)calloc(RECORD_NAME_SIZE, sizeof(char));
        strcpy(records[i], "");
    }

    for( i = 0; i < numRecords && currSlash != NULL; i++ )
    {
        nextSlash = strstr(strstr(currSlash, "/") + 1, "/");

        if( nextSlash != NULL )
        {
            length = (nextSlash - currSlash) - 1;
        }
        else
        {
            length = ((absPathname + strlen(absPathname)) - currSlash) - 1;
        }

        strncpy(records[i], currSlash + 1, length);
        records[i][length] = '\0';

        currSlash = nextSlash;
    }

    return records;
}

/*-----------------------------------------------------------------------------
Função: Simplifica o dado caminho absoluto, retirando os links

Entra:
    absPathname -> caminho absoluto ser processado

Saída:
    Caminho simplificado.
-----------------------------------------------------------------------------*/
char* __get_simplified_path(char *absPathname)
{
    char **records;
    char *parsedPathname, *auxRecord;
    int i, numRecords, alocatedRecords, idxRelative, hasRelative;

    records = __get_path_array(absPathname);
    numRecords = __count_records(absPathname);
    alocatedRecords = numRecords;

    parsedPathname = (char*)calloc(strlen(absPathname) + 1, sizeof(char));
    strcpy(parsedPathname, "/");
    auxRecord = (char*)calloc(RECORD_NAME_SIZE, sizeof(char));

    do
    {
        hasRelative = 0;

        for( i = 0; i < numRecords && hasRelative == 0; i++ )
        {
            if( strcmp(records[i], "..") == 0 )
            {
                idxRelative = i;
                hasRelative = 1;
            }
        }

        if( hasRelative )
        {
            for( i = idxRelative; i < (numRecords - 1); i++ )
            {
                strcpy(auxRecord, records[i+1]);
                strcpy(records[i+1], records[i]);
                strcpy(records[i-1], auxRecord);
            }

            numRecords -= 2;
        }

    } while( hasRelative );

    // Monta o caminho simplificando
    for( i = 0; i < numRecords; i++ )
    {
        if( strcmp(records[i], ".") != 0 )
        {
            strcat(parsedPathname, records[i]);

            if( i < (numRecords - 1) )
            {
                strcat(parsedPathname, "/");
            }
        }
    }

    if( strlen(parsedPathname) > 1 && parsedPathname[strlen(parsedPathname) - 1] == '/' )
    {
        parsedPathname[strlen(parsedPathname) - 1] = '\0';
    }

    for( i = 0; i < alocatedRecords; i++ )
    {
        free(records[i]);
    }

    free(records);
    free(auxRecord);

    return parsedPathname;
}

/*-----------------------------------------------------------------------------
Função: Gera o caminho absoluto, removendo os links entre diretórios

Entra:
    path -> caminho a ser processado
    cwdPath -> caminho corrente

Saída:
    Se a operação foi realizada corretamente, o caminho absoluto direto
    Se houver algum erro, NULL.
-----------------------------------------------------------------------------*/
char* parse_path(char *path, char* cwdPath)
{
    char *absPathname, *parsedPathname;

    absPathname = __get_abspath(path, cwdPath);

    if( absPathname != NULL )
    {
        // Caso for o dir. raiz
        if( (strlen(absPathname) == 1) && (absPathname[0] == '/') )
        {
            free(absPathname);

            return "/";
        }

        parsedPathname =  __get_simplified_path(absPathname);

        free(absPathname);

        return parsedPathname;
    }

    return NULL;
}

/*-----------------------------------------------------------------------------
Função: Extrai o nome do 'record', de trás para frente, modificando o caminho informado

Entra:
    parsedPath -> Caminho já normalizado

Saída:
    Nome do 'record'.
-----------------------------------------------------------------------------*/
char* extract_recordname(char* parsedPath)
{
    int i;
    char* recordName;

    recordName = (char*)calloc(RECORD_NAME_SIZE, sizeof(char));
    strcpy(recordName, "");

    if( strlen(parsedPath) > 1 )
    {
        for(i = (strlen(parsedPath) - 1); i >= 0; i-- )
        {
            if( parsedPath[i] == '/' )
            {
                strcpy(recordName, parsedPath + (i+1));
                strcpy(parsedPath + i, "");

                break;
            }
        }

        if( strlen(parsedPath) == 0 )
        {
            strcpy(parsedPath, "/");
        }
    }

    return recordName;
}
