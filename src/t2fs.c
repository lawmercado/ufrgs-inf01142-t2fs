#include "../include/t2fs.h"
#include "../include/bitmap2.h"
#include "../include/apidisk.h"
#include "../include/parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define OP_SUCCESS 0
#define OP_ERROR -1

/*-----------------------------------------------------------------------------
Flag de inicialização da biblioteca
-----------------------------------------------------------------------------*/
int g_initialized = 0;

/*-----------------------------------------------------------------------------
Superbloco do disco
-----------------------------------------------------------------------------*/
struct t2fs_superbloco *g_sb;

/*-----------------------------------------------------------------------------
Inode associado ao diretório raiz
-----------------------------------------------------------------------------*/
struct t2fs_inode *g_ri;

/*-----------------------------------------------------------------------------
Handlers dos arquivos.
-----------------------------------------------------------------------------*/
HANDLER g_files[MAX_NUM_HANDLERS];

/*-----------------------------------------------------------------------------
Handlers dos diretórios.
-----------------------------------------------------------------------------*/
HANDLER g_dirs[MAX_NUM_HANDLERS];

/*-----------------------------------------------------------------------------
Caminho corrente de trabalho.
-----------------------------------------------------------------------------*/
char *g_cwd;

/*-----------------------------------------------------------------------------
Record associado ao caminho corrente de trabalho.
-----------------------------------------------------------------------------*/
struct t2fs_record *g_cwd_record;

void __print_superbloco(char *label, struct t2fs_superbloco *bloco)
{
    printf("\n--%s--\n", label);
    printf("ID: %.4s\n", bloco->id);
    printf("Version: 0x%.4x (0x7e2 = 2018; 1 = first semester)\n", bloco->version);
    printf("SuperBlockSize: %d blocks\n", bloco->superblockSize);
    printf("FreeBlocksBitmapSize: %d blocks\n", bloco->freeBlocksBitmapSize);
    printf("FreeInodeBitmapSize: %d blocks\n", bloco->freeInodeBitmapSize);
    printf("InodeAreaSize: %d blocks\n", bloco->inodeAreaSize);
    printf("BlockSize: %d sectors\n", bloco->blockSize);
    printf("DiskSize: %d blocks\n", bloco->diskSize);
}

void __print_inode(char *label, struct t2fs_inode *inode)
{
    printf("\n--%s--\n", label);
    printf("File size: %d blocks\n", inode->blocksFileSize);
    printf("File size: %d bytes\n", inode->bytesFileSize);
    printf("DataPtr[0]: %d\n", inode->dataPtr[0]);
    printf("DataPtr[1]: %d\n", inode->dataPtr[1]);
    printf("Single ind. ptr.: %d\n", inode->singleIndPtr);
    printf("Double ind. ptr.: %d\n", inode->doubleIndPtr);
}

void __print_record(char *label, struct t2fs_record *record)
{
    printf("\n--%s--\n", label);
    printf("Name: %s\n", record->name);
    printf("Type: %d\n", record->TypeVal);
    printf("Inode number: %d\n", record->inodeNumber);
}

void __print_handler(char *label, HANDLER *handler)
{
    printf("\n--%s--\n", label);
    printf("Rec. name: %s\n", handler->record->name);
    printf("Pointer: %d\n", handler->pointer);
    printf("Wd: %s\n", handler->wd);
    printf("Free: %d\n", handler->free);
}

/*-----------------------------------------------------------------------------
Função: Calcula o setor dado o número do inode

Saída:
    Setor do inode informado.
-----------------------------------------------------------------------------*/
unsigned int __inode_get_sector(DWORD inodeNumber)
{
    unsigned int base_sector = (g_sb->superblockSize + g_sb->freeBlocksBitmapSize + g_sb->freeInodeBitmapSize) * g_sb->blockSize;

    return base_sector + ((inodeNumber * sizeof(struct t2fs_inode)) / SECTOR_SIZE);
}

/*-----------------------------------------------------------------------------
Função: Calcula o índice do inode dentro do setor conforme o seu número

Saída:
    Índice do inode no setor.
-----------------------------------------------------------------------------*/
unsigned int __inode_get_sector_idx(DWORD inodeNumber)
{
    return (inodeNumber % (SECTOR_SIZE/sizeof(struct t2fs_inode))) * sizeof(struct t2fs_inode);
}

/*-----------------------------------------------------------------------------
Função: Calcula o setor de um dado bloco

Saída:
    Setor base dos blocos de dados.
-----------------------------------------------------------------------------*/
unsigned int __block_get_sector(DWORD blockNumber)
{
    return blockNumber * g_sb->blockSize;
}

/*-----------------------------------------------------------------------------
Função: Encontra o ponteiro assoc. ao inodeNumber informado

Entra:
    inodeNumber -> número do inode a ser encontrado

Saída:
    Se a operação foi realizada com sucesso, retorna o inode
    Se ocorreu algum erro, retorna NULL.
-----------------------------------------------------------------------------*/
struct t2fs_inode* __inode_get_by_idx(DWORD inodeNumber)
{
    BYTE buffer[SECTOR_SIZE];
    unsigned int idxInode = __inode_get_sector_idx(inodeNumber);

    if( read_sector(__inode_get_sector(inodeNumber), buffer) == OP_SUCCESS )
    {
        return buffer_to_inode(buffer, idxInode);
    }

    return NULL;
}

/*-----------------------------------------------------------------------------
Função: Cria um novo inode

Entra:
    inode -> dados a serem salvos
    inodeNumber -> número do inode

Saída:
    Se a operação foi realizada com sucesso, retorna OP_SUCCESS
    Se ocorreu algum erro, retorna OP_ERROR.
-----------------------------------------------------------------------------*/
int __inode_write(struct t2fs_inode *inode, int inodeNumber)
{
    BYTE buffer[SECTOR_SIZE], *buffer_inode = NULL;
    unsigned int sector = __inode_get_sector(inodeNumber);
    int idxInode = __inode_get_sector_idx(inodeNumber);
    int i;

    if( read_sector(sector, buffer) == OP_SUCCESS )
    {
        buffer_inode = inode_to_buffer(inode);

        for(i = 0; i < sizeof(struct t2fs_inode); i++)
        {
            buffer[idxInode + i] = buffer_inode[i];
        }

        if( write_sector(sector, buffer) == OP_SUCCESS )
        {
            return OP_SUCCESS;
        }
    }

    return OP_ERROR;
}

/*-----------------------------------------------------------------------------
Função: Lê a 'idxEntry'-ézima de tamanho 'size' entrada do bloco especificado

Entra:
    idxEntry -> índice da entrada no bloco
    size -> tamanho da entrada
    blockNumber -> o bloco onde ler a entrada

Saída:
    Se a operação foi realizada com sucesso, retorna um buffer com a entrada
    Se ocorreu algum erro, retorna NULL.
-----------------------------------------------------------------------------*/
BYTE* __block_get_entry(DWORD idxEntry, int size, DWORD blockNumber)
{
    BYTE buffer[SECTOR_SIZE], *entryBuffer;
    DWORD idxSector = (idxEntry * size) / SECTOR_SIZE;
    DWORD idxSectorEntry = (idxEntry * size) % SECTOR_SIZE;
    int i;

    if( read_sector(__block_get_sector(blockNumber) + idxSector, buffer) == OP_SUCCESS )
    {
        entryBuffer = (BYTE*)malloc(size);

        for( i = 0; i < size; i++ )
        {
            entryBuffer[i] = buffer[idxSectorEntry + i];
        }

        return entryBuffer;
    }

    return NULL;
}

/*-----------------------------------------------------------------------------
Função: Escreve um ponteiro para um bloco num dado bloco de indireção

Entra:
    idxPtr -> índice do ponteiro no bloco
    blockNumber -> valor do ponteiro para o bloco
    indBlockNumber -> bloco onde o ponteiro deve ser escrito

Saída:
    Se a operação foi realizada com sucesso, retorna OP_SUCCESS
    Se ocorreu algum erro, retorna OP_ERROR.
-----------------------------------------------------------------------------*/
int __block_write_ptr(DWORD idxPtr, DWORD blockNumber, DWORD indBlockNumber)
{
    BYTE buffer[SECTOR_SIZE], *buffer_ptr;
    unsigned int sector = indBlockNumber * g_sb->blockSize + ((idxPtr * sizeof(DWORD)) / SECTOR_SIZE);
    int idxSectorPtr = (idxPtr % (SECTOR_SIZE/sizeof(DWORD))) * sizeof(DWORD);
    int i;

    if( read_sector(sector, buffer) == OP_SUCCESS )
    {
        buffer_ptr = dword_to_buffer(blockNumber);

        for(i = 0; i < sizeof(DWORD); i++)
        {
            buffer[idxSectorPtr + i] = buffer_ptr[i];
        }

        if( write_sector(sector, buffer) == OP_SUCCESS )
        {
            return OP_SUCCESS;
        }
    }

    return OP_ERROR;
}

/*-----------------------------------------------------------------------------
Função: Encontra o 'idxBlock'ézimo bloco no inode

Entra:
    idxBlock -> índice do bloco relativo ao inode
    inode -> inode onde está o bloco

Saída:
    Se a operação foi realizada com sucesso, retorna o número do ponteiro
    Se ocorreu algum erro, retorna INVALID_PTR.
-----------------------------------------------------------------------------*/
DWORD __block_get_by_idx(DWORD idxBlock, struct t2fs_inode *inode)
{
    DWORD dataBlockNumber = 0;

    if( idxBlock < inode->blocksFileSize )
    {
        if( idxBlock == 0 )
        {
            dataBlockNumber = inode->dataPtr[0];
        }
        else if( idxBlock == 1 )
        {
            dataBlockNumber = inode->dataPtr[1];
        }
        else
        {
            int blockNumberPerBlock = (g_sb->blockSize * SECTOR_SIZE) / sizeof(DWORD);
            int idxBase = idxBlock - 2;

            if( idxBase < blockNumberPerBlock  )
            {
                dataBlockNumber = buffer_to_dword(__block_get_entry(idxBase, sizeof(DWORD), inode->singleIndPtr), 0);
            }
            else
            {
                idxBase = idxBlock - blockNumberPerBlock - 2;
                int idxIndBlockList = idxBase / blockNumberPerBlock;
                int idxIndBlock = idxBase % blockNumberPerBlock;

                DWORD ptrBlockNumber = buffer_to_dword(__block_get_entry(idxIndBlockList, sizeof(DWORD), inode->doubleIndPtr), 0);

                dataBlockNumber = buffer_to_dword(__block_get_entry(idxIndBlock, sizeof(DWORD), ptrBlockNumber), 0);
            }
        }

        return dataBlockNumber;
    }

    return INVALID_PTR;
}

/*-----------------------------------------------------------------------------
Função: Inicializa o bloco com dado valor

Entra:
    blockNumber -> número do bloco
    value -> valor a ser escrito

Saída:
    Se a operação foi realizada com sucesso, retorna OP_SUCCESS
    Se ocorreu algum erro, retorna OP_ERROR.
-----------------------------------------------------------------------------*/
int __block_init(DWORD blockNumber, char value)
{
    int i, j;
    BYTE buffer[SECTOR_SIZE];

    for( i = 0; i < g_sb->blockSize; i++ )
    {
        if( read_sector(__block_get_sector(blockNumber) + i, buffer) != OP_SUCCESS )
        {
            return OP_ERROR;
        }
        else
        {
            for( j = 0; j < SECTOR_SIZE; j++ )
            {
                buffer[j] = (BYTE)value;
            }

            if( write_sector(__block_get_sector(blockNumber) + i, buffer) != OP_SUCCESS )
            {
                return OP_ERROR;
            }
        }
    }

    return OP_SUCCESS;
}

/*-----------------------------------------------------------------------------
Função: Navega sequencialmente pelos registros até encontrar o registro apontado
        por 'pointer', retornando o bloco onde este se encontra

Entra:
    pointer -> 'pointer'-ézimo item
    size -> tamanho do 'pointer'-ézimo item
    inode -> inode que contém os ponteiros para os blocos de dados

Saída:
    Se a operação foi realizada com sucesso, retorna o número do bloco
    Se ocorreu algum erro, retorna INVALID_PTR.
-----------------------------------------------------------------------------*/
DWORD __block_navigate(DWORD pointer, int size, struct t2fs_inode *inode)
{
    int entryPerBlock = (g_sb->blockSize * SECTOR_SIZE) / size;
    int entryBlock = pointer / entryPerBlock;

    return __block_get_by_idx(entryBlock, inode);
}

/*-----------------------------------------------------------------------------
Função: Navega sequencialmente pelos registros até encontrar o registro apontado
        por 'pointer', retornando o bloco onde este se encontra

Entra:
    pointer -> 'pointer'-ézimo item
    buffer -> buffer a ser escrito
    size -> tamanho do buffer a ser escrito
    inode -> inode que contém as informações de onde escrever

Saída:
    Se a operação foi realizada com sucesso, retorna OP_SUCCESS
    Se ocorreu algum erro, retorna OP_ERROR.
-----------------------------------------------------------------------------*/
int __inode_write_bytes(DWORD pointer, char *buffer, int size, struct t2fs_inode *inode)
{
    BYTE readBuffer[SECTOR_SIZE];
    DWORD idxBloco = pointer / (g_sb->blockSize * SECTOR_SIZE);
    DWORD idxSector = (pointer * sizeof(BYTE)) / SECTOR_SIZE;
    DWORD idxSectorStart = (pointer * sizeof(BYTE)) % SECTOR_SIZE;
    int idxBuffer = 0;
    int i, j, k;

    for( k = 0; k <= size / (g_sb->blockSize * SECTOR_SIZE); k++ )
    {
        DWORD blockNumber = __block_get_by_idx(idxBloco + k, inode);

        for( i = idxSector; i < g_sb->blockSize && idxBuffer <= size; i++ )
        {
            if( read_sector(__block_get_sector(blockNumber) + i, readBuffer) == OP_SUCCESS )
            {
                for( j = idxSectorStart; j < SECTOR_SIZE && idxBuffer <= size; j++ )
                {
                    readBuffer[j] = (BYTE) buffer[idxBuffer];
                    idxBuffer++;
                }

                if( write_sector(__block_get_sector(blockNumber) + i, readBuffer) == OP_SUCCESS )
                {
                    idxSectorStart = 0;
                }
                else
                {
                    return OP_ERROR;
                }
            }
            else
            {
                return OP_ERROR;
            }
        }
    }

    return OP_SUCCESS;
}

/*-----------------------------------------------------------------------------
Função: Aloca um novo bloco de dados para o dado inode

Entra:
    inode -> inode no qual deve ser alocado o bloco
    inodeNumber -> índice do inode

Saída:
    Se a operação foi realizada com sucesso, retorna OP_SUCCESS
    Se ocorreu algum erro, retorna OP_ERROR.
-----------------------------------------------------------------------------*/
int __block_alocate(struct t2fs_inode *inode, DWORD inodeNumber)
{
    int idxBlockBase = inode->blocksFileSize;
    int blockNumberPerBlock = (g_sb->blockSize * SECTOR_SIZE) / sizeof(DWORD);

    DWORD dataBlockNumber = searchBitmap2(BITMAP_DADOS, 0);

    if( dataBlockNumber > 0 )
    {
        if( idxBlockBase == 0 )
        {
            inode->dataPtr[0] = dataBlockNumber;

            setBitmap2(BITMAP_DADOS, dataBlockNumber, 1);

            __block_init(dataBlockNumber, 0);
        }
        else if( idxBlockBase == 1 )
        {
            inode->dataPtr[1] = dataBlockNumber;

            setBitmap2(BITMAP_DADOS, dataBlockNumber, 1);

            __block_init(dataBlockNumber, 0);
        }
        else
        {
            int idxBase = idxBlockBase - 2;

            if( idxBase < blockNumberPerBlock )
            {
                if( inode->singleIndPtr == INVALID_PTR )
                {
                    int indBlockNumber = dataBlockNumber;

                    setBitmap2(BITMAP_DADOS, indBlockNumber, 1);

                    dataBlockNumber = searchBitmap2(BITMAP_DADOS, 0);

                    if( dataBlockNumber <= 0 )
                    {
                        setBitmap2(BITMAP_DADOS, indBlockNumber, 0);

                        return OP_ERROR;
                    }
                    else
                    {
                        inode->singleIndPtr = indBlockNumber;
                    }
                }

                if( __block_write_ptr(idxBase, dataBlockNumber, inode->singleIndPtr) == OP_SUCCESS )
                {
                    setBitmap2(BITMAP_DADOS, dataBlockNumber, 1);
                    __block_init(dataBlockNumber, 0);
                }
            }
            else
            {
                idxBase = idxBlockBase - blockNumberPerBlock - 2;
                int idxIndBlockList = idxBase / blockNumberPerBlock;
                int idxBlock = idxBase % blockNumberPerBlock;

                if( inode->doubleIndPtr == INVALID_PTR )
                {
                    int doubleIndBlockNumber = dataBlockNumber;

                    setBitmap2(BITMAP_DADOS, doubleIndBlockNumber, 1);

                    dataBlockNumber = searchBitmap2(BITMAP_DADOS, 0);

                    if( dataBlockNumber <= 0 )
                    {
                        setBitmap2(BITMAP_DADOS, doubleIndBlockNumber, 0);

                        return OP_ERROR;
                    }
                    else
                    {
                        inode->doubleIndPtr = doubleIndBlockNumber;

                        int i;

                        for( i = 0; i < blockNumberPerBlock; i++ )
                        {
                            __block_write_ptr(i, INVALID_PTR, inode->doubleIndPtr);
                        }
                    }
                }

                DWORD ptrBlockNumber = buffer_to_dword(__block_get_entry(idxIndBlockList, sizeof(DWORD), inode->doubleIndPtr), 0);

                if( ptrBlockNumber == INVALID_PTR )
                {
                    int indBlockNumber = dataBlockNumber;

                    setBitmap2(BITMAP_DADOS, indBlockNumber, 1);

                    dataBlockNumber = searchBitmap2(BITMAP_DADOS, 0);

                    if( dataBlockNumber <= 0 )
                    {
                        setBitmap2(BITMAP_DADOS, indBlockNumber, 0);

                        return OP_ERROR;
                    }
                    else
                    {
                        ptrBlockNumber = indBlockNumber;
                        __block_write_ptr(idxIndBlockList, ptrBlockNumber, inode->doubleIndPtr);
                    }
                }

                if( __block_write_ptr(idxBlock, dataBlockNumber, ptrBlockNumber) == OP_SUCCESS )
                {
                    setBitmap2(BITMAP_DADOS, dataBlockNumber, 1);
                    __block_init(dataBlockNumber, 0);
                }
            }
        }

        inode->blocksFileSize += 1;

        if( __inode_write(inode, inodeNumber) == OP_SUCCESS )
        {
            return OP_SUCCESS;
        }
        else
        {
            setBitmap2(BITMAP_DADOS, dataBlockNumber, 0);
        }
    }

    return OP_ERROR;
}

/*-----------------------------------------------------------------------------
Função: Remove o bloco do dado inode

Entra:
    inode -> inode de onde remover o bloco
    inodeNumber -> número do inode
    blockNumber -> número do bloco

Saída:
    Se a operação foi realizada com sucesso, retorna OP_SUCCESS
    Se ocorreu algum erro, retorna OP_ERROR.
-----------------------------------------------------------------------------*/
int __inode_remove_block(struct t2fs_inode *inode, DWORD inodeNumber, DWORD blockNumber)
{
    __block_init(blockNumber, 0);
    setBitmap2(BITMAP_DADOS, blockNumber, 0);

    int newBlocksSize = inode->blocksFileSize - 1;

    if( newBlocksSize == 0 )
    {
        inode->dataPtr[0] = INVALID_PTR;
    }
    else if( newBlocksSize == 1 )
    {
        inode->dataPtr[1] = INVALID_PTR;
    }
    else if( newBlocksSize == 2 )
    {
        setBitmap2(BITMAP_DADOS, inode->singleIndPtr, 0);
        inode->singleIndPtr = INVALID_PTR;
    }
    else if( newBlocksSize == 258 )
    {
        setBitmap2(BITMAP_DADOS, inode->doubleIndPtr, 0);
        inode->doubleIndPtr = INVALID_PTR;
    }
    else if( newBlocksSize > 258 )
    {
        int blockNumberPerBlock = (g_sb->blockSize * SECTOR_SIZE) / sizeof(DWORD);
        int idxIndBlockList = inode->blocksFileSize / blockNumberPerBlock;
        setBitmap2(BITMAP_DADOS, buffer_to_dword(__block_get_entry(idxIndBlockList, sizeof(DWORD), inode->doubleIndPtr), 0), 0);
        __block_write_ptr(idxIndBlockList, INVALID_PTR, inode->doubleIndPtr);
    }

    inode->blocksFileSize = newBlocksSize;
    inode->bytesFileSize = (inode->blocksFileSize * SECTOR_SIZE * g_sb->blockSize) < inode->bytesFileSize ? (inode->blocksFileSize * SECTOR_SIZE * g_sb->blockSize) : inode->bytesFileSize;

    if( __inode_write(inode, inodeNumber) == OP_SUCCESS )
    {
        return OP_SUCCESS;
    }

    return OP_ERROR;
}

/*-----------------------------------------------------------------------------
Função: Desaloca um novo bloco de dados para o dado inode

Entra:
    inode -> inode no qual deve ser desalocado o bloco
    inodeNumber -> índice do inode

Saída:
    Se a operação foi realizada com sucesso, retorna OP_SUCCESS
    Se ocorreu algum erro, retorna OP_ERROR.
-----------------------------------------------------------------------------*/
int __block_free(struct t2fs_inode *inode, DWORD inodeNumber)
{
    int idxBlock = inode->blocksFileSize - 1;

    DWORD dataBlockNumber = __block_get_by_idx(idxBlock, inode);

    return __inode_remove_block(inode, inodeNumber, dataBlockNumber);
}

/*-----------------------------------------------------------------------------
Função: Lê a entrada referenciada por pointer

Entra:
    pointer -> ponteiro de entrada com referencia ao handler
    size -> tamanho da entrada
    inode -> inode onde procurar

Saída:
    Se a operação foi realizada com sucesso, retorna um buffer com a entrada
    Se ocorreu algum erro, retorna NULL.
-----------------------------------------------------------------------------*/
BYTE* __entry_read(DWORD pointer, int size, struct t2fs_inode *inode)
{
    DWORD dataBlockNumber = __block_navigate(pointer, size, inode);

    if( dataBlockNumber != INVALID_PTR )
    {
        int entryPerBlock = (g_sb->blockSize * SECTOR_SIZE) / size;
        int idxEntry = pointer % entryPerBlock;

        return __block_get_entry(idxEntry, size, dataBlockNumber);
    }

    return NULL;
}

/*-----------------------------------------------------------------------------
Função: Encontra o record pelo ponteiro

Entra:
    idxRecord -> índice do record no bloco
    inode -> inode associado

Saída:
    Se a operação foi realizada com sucesso, retorna o record
    Se ocorreu algum erro, retorna NULL.
-----------------------------------------------------------------------------*/
struct t2fs_record* __record_get_by_idx(DWORD pointer, struct t2fs_inode *inode)
{
    BYTE* entryBuffer = __entry_read(pointer, sizeof(struct t2fs_record), inode);
    struct t2fs_record *record = NULL;

    if( entryBuffer != NULL )
    {
        record = buffer_to_record(entryBuffer, 0);

        return record;
    }

    return NULL;
}

/*-----------------------------------------------------------------------------
Função: Encontra o registro associado ao nome no inode especificado

Entra:
    name -> nome da entrada
    inode -> inode para procurar

Saída:
    Se a operação foi realizada com sucesso, retorna o record
    Se ocorreu algum erro, retorna NULL.
-----------------------------------------------------------------------------*/
struct t2fs_record* __record_get_by_name(char *name, struct t2fs_inode *inode)
{
    int pointer = 0;
    struct t2fs_record *record = NULL, *foundRecord = NULL;

    do
    {
        record = __record_get_by_idx(pointer, inode);

        if( record != NULL )
        {
            if( record->TypeVal == TYPEVAL_REGULAR || record->TypeVal == TYPEVAL_DIRETORIO )
            {
                if( strcmp(name, record->name) == 0 )
                {
                    foundRecord = record;

                    return foundRecord;
                }
            }
        }

        pointer++;

    } while( record != NULL );

    return foundRecord;
}

/*-----------------------------------------------------------------------------
Função: Encontra o índice de um 'record' pelo seu nome

Entra:
    name -> nome pelo qual procurar
    inode -> inode onde procurar

Saída:
    Se a operação foi realizada com sucesso, retorna o índice (int. >= 0)
    Se ocorreu algum erro, retorna INVALID_PTR.
-----------------------------------------------------------------------------*/
DWORD __record_get_idx_by_name(char *name, struct t2fs_inode *inode)
{
    int pointer = 0;
    int idxFound = INVALID_PTR;
    struct t2fs_record *record = NULL;

    do
    {
        record = __record_get_by_idx(pointer, inode);

        if( record != NULL )
        {
            if( record->TypeVal == TYPEVAL_REGULAR || record->TypeVal == TYPEVAL_DIRETORIO )
            {
                if( strcmp(name, record->name) == 0)
                {
                    idxFound = pointer;

                    return idxFound;
                }
            }
        }

        pointer++;

    } while( record != NULL );

    return idxFound;
}

/*-----------------------------------------------------------------------------
Função: Encontra o primeiro registro de 'record' livre

Entra:
    inode -> inode onde procurar

Saída:
    Se a operação foi realizada com sucesso, retorna o índice (int. >= 0)
    Se ocorreu algum erro, retorna INVALID_PTR.
-----------------------------------------------------------------------------*/
DWORD __record_get_free_idx(struct t2fs_inode *inode)
{
    int pointer = 0;
    struct t2fs_record *record = NULL;

    do
    {
        record = __record_get_by_idx(pointer, inode);

        if( record != NULL )
        {
            if( record->TypeVal == TYPEVAL_INVALIDO )
            {
                return pointer;
            }
        }

        pointer++;

    } while( record != NULL );

    return INVALID_PTR;
}

/*-----------------------------------------------------------------------------
Função: Cria um record no índice do bloco indicado

Entra:
    record -> dados a serem salvos
    idxFreeRecord -> índice do espaço livre no bloco
    blockNumber -> número do bloco onde salvar

Saída:
    Se a operação foi realizada com sucesso, retorna OP_SUCCESS
    Se ocorreu algum erro, retorna OP_ERROR.
-----------------------------------------------------------------------------*/
int __record_write(struct t2fs_record *record, int idxFreeRecord, int blockNumber)
{
    BYTE buffer[SECTOR_SIZE], *buffer_record = NULL;
    unsigned int sector = __block_get_sector(blockNumber) + (idxFreeRecord / (SECTOR_SIZE/sizeof(struct t2fs_record)));
    int idxRecord = (idxFreeRecord % (SECTOR_SIZE/sizeof(struct t2fs_record))) * sizeof(struct t2fs_record);
    int i;

    if( read_sector(sector, buffer) == OP_SUCCESS )
    {
        buffer_record = record_to_buffer(record);

        for(i = 0; i < sizeof(struct t2fs_record); i++)
        {
            buffer[idxRecord + i] = buffer_record[i];
        }

        if( write_sector(sector, buffer) == OP_SUCCESS )
        {
            return OP_SUCCESS;
        }
    }

    return OP_ERROR;
}

/*-----------------------------------------------------------------------------
Função: Realiza a alocação dos diversos recursos necessários

Entra:
    name -> nome do record
    type -> tipo do record
    parentRecord -> onde escrever o record

Saída:
    Se a operação foi realizada com sucesso, retorna OP_SUCCESS
    Se ocorreu algum erro, retorna OP_ERROR.
-----------------------------------------------------------------------------*/
int __record_alocate(char *name, int type, struct t2fs_record* parentRecord)
{
    struct t2fs_record *record = NULL;
    struct t2fs_inode *inode = NULL;

    struct t2fs_inode *parentInode = __inode_get_by_idx(parentRecord->inodeNumber);
    int idxFreeRecord = __record_get_free_idx(parentInode);

    if( idxFreeRecord == OP_ERROR )
    {
        if( __block_alocate(parentInode, parentRecord->inodeNumber) == OP_SUCCESS )
        {
            if( type == TYPEVAL_DIRETORIO )
            {
                parentInode->bytesFileSize = parentInode->blocksFileSize * SECTOR_SIZE;
                __inode_write(parentInode, parentRecord->inodeNumber);
            }

            idxFreeRecord = __record_get_free_idx(parentInode);
        }
    }

    if( idxFreeRecord != OP_ERROR )
    {
        record = (struct t2fs_record*)calloc(1, sizeof(struct t2fs_record));

        if( strlen(name) <= (RECORD_NAME_SIZE - 1) )
        {
            int b_inode = searchBitmap2(BITMAP_INODE, 0);
            int b_dados = searchBitmap2(BITMAP_DADOS, 0);

            if( b_inode > 0 && b_dados > 0 )
            {
                strncpy(record->name, name, RECORD_NAME_SIZE - 1);
                record->TypeVal = type;
                record->inodeNumber = b_inode;

                inode = (struct t2fs_inode*)calloc(1, sizeof(struct t2fs_inode));

                inode->blocksFileSize = 1;
                inode->bytesFileSize = type == TYPEVAL_DIRETORIO ? g_sb->blockSize * SECTOR_SIZE : 0;
                inode->dataPtr[0] = b_dados;
                inode->dataPtr[1] = INVALID_PTR;
                inode->singleIndPtr = INVALID_PTR;
                inode->doubleIndPtr = INVALID_PTR;

                if( __inode_write(inode, b_inode) == OP_SUCCESS )
                {
                    if( __record_write(record, idxFreeRecord, __block_navigate(idxFreeRecord, sizeof(struct t2fs_record), parentInode)) == OP_SUCCESS )
                    {
                        setBitmap2(BITMAP_INODE, b_inode, 1);
                        setBitmap2(BITMAP_DADOS, b_dados, 1);

                        __block_init(b_dados, 0);

                        if( type == TYPEVAL_DIRETORIO )
                        {
                            struct t2fs_record *selfRecord = (struct t2fs_record*)calloc(1, sizeof(struct t2fs_record));
                            struct t2fs_record *selfParentRecord = (struct t2fs_record*)calloc(1, sizeof(struct t2fs_record));

                            strcpy(selfRecord->name, ".");
                            selfRecord->TypeVal = TYPEVAL_DIRETORIO;
                            selfRecord->inodeNumber = b_inode;

                            strcpy(selfParentRecord->name, "..");
                            selfParentRecord->TypeVal = TYPEVAL_DIRETORIO;
                            selfParentRecord->inodeNumber = parentRecord->inodeNumber;

                            __record_write(selfRecord, 0, b_dados);
                            __record_write(selfParentRecord, 1, b_dados);

                            free(selfRecord);
                            free(selfParentRecord);
                        }

                        free(inode);
                        free(record);


                        return OP_SUCCESS;
                    }
                }
            }
        }
    }

    return OP_ERROR;
}

/*-----------------------------------------------------------------------------
Função: Encontra o record indicado pelo caminho informado

Entra:
    parsedPath -> caminho já normalizado

Saída:
    Se a operação foi realizada com sucesso, retorna o record associado
    Se ocorreu algum erro, retorna NULL.
-----------------------------------------------------------------------------*/
struct t2fs_record* __record_navigate(char *parsedPath)
{
    char *auxPathname;
    struct t2fs_record *record = NULL;
    struct t2fs_inode *inode = NULL;

    if( parsedPath != NULL )
    {
        auxPathname = (char*)calloc(strlen(parsedPath) + 1, sizeof(char));
        strcpy(auxPathname, parsedPath);


        // Caminho absoluto
        if( auxPathname[0] == '/' )
        {
            record = __record_get_by_name(".", g_ri);
            inode = g_ri;
        }
        else
        {
            record = g_cwd_record;
            inode = __inode_get_by_idx(g_cwd_record->inodeNumber);
        }

        char *token;

        while( (token = strsep(&auxPathname, "/")) )
        {
            if( strlen(token) > 0 )
            {
                record = __record_get_by_name(token, inode);

                if( record != NULL )
                {
                    inode = __inode_get_by_idx(record->inodeNumber);
                }
                else
                {
                    return NULL;
                }
            }
        }

        return record;
    }

    return NULL;
}

/*-----------------------------------------------------------------------------
Função: Cria um record no caminho especificado

Entra:
    path -> caminho para o record
    type -> tipo do record

Saída:
    Se a operação foi realizada com sucesso, retorna OP_SUCCESS
    Se ocorreu algum erro, retorna OP_ERROR.
-----------------------------------------------------------------------------*/
int __record_create(char *pathname, int type)
{
    char* parsedPath = parse_path(pathname, g_cwd);

    if( parsedPath != NULL )
    {
        struct t2fs_record *record = __record_navigate(parsedPath);

        // Se já não existe algum record com esse nome
        if( record == NULL )
        {
            char* recordName = extract_recordname(parsedPath);
            struct t2fs_record *parentRecord = __record_navigate(parsedPath);

            // Se o record pai existe
            if( parentRecord != NULL && strlen(recordName) > 0 )
            {
                return __record_alocate(recordName, type, parentRecord);
            }
        }
    }

    return OP_ERROR;
}

/*-----------------------------------------------------------------------------
Função: Verifica se um dado record está aberto

Entra:
    recordName -> nome do record
    type -> tipo do record
    recordWd-> "contexto" do record

Saída:
    Se veradeiro, retorna 1
    Se falso 0.
-----------------------------------------------------------------------------*/
int __record_is_opened(char* recordName, int type, char* recordWd)
{
    HANDLER *handler = NULL;
    int i;

    if( type == TYPEVAL_REGULAR )
    {
        handler = g_files;
    }
    else
    {
        handler = g_dirs;
    }

    for( i = 0; i < MAX_NUM_HANDLERS; i++ )
    {
        if( !handler[i].free )
        {
            if( handler[i].record != NULL )
            {
                if( strcmp(handler[i].record->name, recordName) == 0 && strcmp(handler[i].wd, recordWd) == 0 )
                {
                    return 1;
                }
            }
        }
    }

    return 0;
}

/*-----------------------------------------------------------------------------
Função: Verifica se um dado diretório está vazio

Entra:
    recordName -> nome do record

Saída:
    Se veradeiro, retorna 1
    Se falso 0.
-----------------------------------------------------------------------------*/
int __record_is_dir_empty(struct t2fs_record* dir)
{
    struct t2fs_record *record;
    struct t2fs_inode *inode = __inode_get_by_idx(dir->inodeNumber);
    int pointer = 0, recordCounter = 0;

    record = __record_get_by_idx(pointer, inode);

    while( record != NULL )
    {
        if( record->TypeVal == TYPEVAL_REGULAR || record->TypeVal == TYPEVAL_DIRETORIO )
        {
            if( strcmp(record->name, ".") != 0 && strcmp(record->name, "..") != 0 )
            {
                recordCounter++;
            }
        }
        else
        {
            break;
        }

        pointer++;
        record = __record_get_by_idx(pointer, inode);
    }

    return recordCounter == 0;
}

/*-----------------------------------------------------------------------------
Função: Remove o record pelo nome

Entra:
    name -> nome do record
    parentRecord -> record pai

Saída:
    Se a operação foi realizada com sucesso, retorna OP_SUCCESS
    Se ocorreu algum erro, retorna OP_ERROR.
-----------------------------------------------------------------------------*/
int __record_free(char* name, struct t2fs_record* parentRecord)
{
    struct t2fs_inode *inode;
    struct t2fs_inode *parentInode = __inode_get_by_idx(parentRecord->inodeNumber);
    struct t2fs_record *record;
    int i;
    int idxRecord = __record_get_idx_by_name(name, parentInode);

    if( idxRecord != OP_ERROR )
    {
        record = __record_get_by_name(name, parentInode);
        inode = __inode_get_by_idx(record->inodeNumber);

        record->TypeVal = TYPEVAL_INVALIDO;

        __record_write(record, idxRecord, __block_navigate(idxRecord, sizeof(struct t2fs_record), parentInode));

        for( i = 0; i < inode->blocksFileSize; i++ )
        {
            __block_free(inode, record->inodeNumber);
        }

        setBitmap2(BITMAP_INODE, record->inodeNumber, 0);

        return OP_SUCCESS;
    }

    return OP_ERROR;
}

/*-----------------------------------------------------------------------------
Função: Deleta o record

Entra:
    path -> caminho para o record
    type -> tipo do record

Saída:
    Se a operação foi realizada com sucesso, retorna OP_SUCCESS
    Se ocorreu algum erro, retorna OP_ERROR.
-----------------------------------------------------------------------------*/
int __record_delete(char *pathname, int type)
{
    char* parsedPath = parse_path(pathname, g_cwd);

    if( parsedPath != NULL )
    {
        struct t2fs_record *record = __record_navigate(parsedPath);

        // Se existe algum record com esse nome
        if( record != NULL )
        {
            char* recordName = extract_recordname(parsedPath);
            struct t2fs_record *parentRecord = __record_navigate(parsedPath);

            // Se é válido e se é do tipo indicado
            if( strlen(recordName) > 0 && record->TypeVal == type )
            {
                if( !__record_is_opened(recordName, type, parsedPath) )
                {
                    if( type == TYPEVAL_DIRETORIO )
                    {
                        if( !__record_is_dir_empty(record) )
                        {

                            return OP_ERROR;
                        }
                    }

                    return __record_free(recordName, parentRecord);
                }
            }
        }
    }

    return OP_ERROR;
}

/*-----------------------------------------------------------------------------
Função: Encontra (ou não) um handler de diretório e ou arquivo disponível

Entra:
    type -> tipo do record (o que implica no tipo do handler)

Saída:
    Se há handler disponível, retorna o índice
    Se ocorreu algum erro, retorn OP_ERROR.
-----------------------------------------------------------------------------*/
int __handler_get_free_idx(int type)
{
    int i;
    HANDLER *handler;

    if( type == TYPEVAL_REGULAR || type == TYPEVAL_DIRETORIO )
    {
        handler = type == TYPEVAL_REGULAR ? g_files : g_dirs;

        for(i = 0; i < MAX_NUM_HANDLERS; i++)
        {
            if( handler[i].free )
            {
                return i;
            }
        }
    }

    return INVALID_PTR;
}

/*-----------------------------------------------------------------------------
Função: Aloca um dado caminho

Entra:
    pathname -> caminho a ser alocado
    type -> tipo do caminho (o que implica no tipo do handler)

Saída:
    Se há handler disponível, retorna o índice
    Se ocorreu algum erro, retorn OP_ERROR.
-----------------------------------------------------------------------------*/
int __handler_alocate(char* pathname, int type)
{
    char* parsedPath = parse_path(pathname, g_cwd);

    if( parsedPath != NULL )
    {
        struct t2fs_record *record = __record_navigate(parsedPath);

        if( record != NULL )
        {
            int freeHandler = -1;
            HANDLER *handler = NULL;

            freeHandler = __handler_get_free_idx(type);
            handler = type == TYPEVAL_REGULAR ? g_files : g_dirs;

            if( freeHandler != INVALID_PTR )
            {
                handler[freeHandler].record = record;
                handler[freeHandler].pointer = 0;
                handler[freeHandler].free = 0;

                // Tira o nome do record sendo adicionado do path
                extract_recordname(parsedPath);
                handler[freeHandler].wd = parsedPath;

                return freeHandler;
            }
        }
    }

    return OP_ERROR;
}

/*-----------------------------------------------------------------------------
Função: Desaloca um dado caminho

Entra:
    handle -> id no handler
    type -> tipo do caminho (o que implica no tipo do handler)

Saída:
    Se há handler disponível, retorna OP_SUCCESS
    Se ocorreu algum erro, retorn OP_ERROR.
-----------------------------------------------------------------------------*/
int __handler_free(int handle, int type)
{
    HANDLER *handler = NULL;

    if( handle >= 0 )
    {
        if( type == TYPEVAL_REGULAR || type == TYPEVAL_DIRETORIO )
        {
            handler = type == TYPEVAL_REGULAR ? g_files : g_dirs;

            if( !(handler[handle].free || handler[handle].record == NULL) )
            {
                handler[handle].record = NULL;
                handler[handle].wd = NULL;
                handler[handle].pointer = 0;
                handler[handle].free = 1;

                return OP_SUCCESS;
            }
        }
    }

    return OP_ERROR;
}

/*-----------------------------------------------------------------------------
Função: Realiza a leitura do superbloco do disco

Saída:
    Se a operação foi realizada com sucesso, retorna OP_SUCCESS
    Se ocorreu algum erro, retorna OP_ERROR.
-----------------------------------------------------------------------------*/
int __init_superblock_read()
{
    BYTE buffer[SECTOR_SIZE];
    unsigned int sector_superblock = 0;

    if( read_sector(sector_superblock, buffer) == OP_SUCCESS )
    {
        g_sb = buffer_to_superblock(buffer, 0);

        return OP_SUCCESS;
    }

    return OP_ERROR;
}

/*-----------------------------------------------------------------------------
Função: Realiza a leitura do inode referente ao diretório raiz

Saída:
    Se a operação foi realizada com sucesso, retorna OP_SUCCESS
    Se ocorreu algum erro, retorna OP_ERROR.
-----------------------------------------------------------------------------*/
int __init_rootinode_read()
{
    g_ri = __inode_get_by_idx(0);

    if( g_ri != NULL )
    {
        return OP_SUCCESS;
    }

    return OP_ERROR;
}

/*-----------------------------------------------------------------------------
Função: Inicializa as varíaveis necessárias para correta execução

Saída:
    Se a operação foi realizada com sucesso, retorna OP_SUCCESS
    Se ocorreu algum erro, retorna OP_ERROR.
-----------------------------------------------------------------------------*/
int __init()
{
    int i;

    if( __init_superblock_read() == OP_SUCCESS && __init_rootinode_read() == OP_SUCCESS )
    {
        for(i = 0; i < MAX_NUM_HANDLERS; i++)
        {
            g_files[i].free = 1;
            g_files[i].record = NULL;
            g_files[i].wd = NULL;

            g_dirs[i].free = 1;
            g_dirs[i].record = NULL;
            g_dirs[i].wd = NULL;
        }

        g_cwd = "/";
        g_cwd_record = __record_get_by_name(".", g_ri);

        g_initialized = 1;

        return OP_SUCCESS;
    }

    return OP_ERROR;
}

/*-----------------------------------------------------------------------------
Função: Usada para identificar os desenvolvedores do T2FS.
	Essa função copia um string de identificação para o ponteiro indicado por "name".
	Essa cópia não pode exceder o tamanho do buffer, informado pelo parâmetro "size".
	O string deve ser formado apenas por caracteres ASCII (Valores entre 0x20 e 0x7A) e terminado por \0.
	O string deve conter o nome e número do cartão dos participantes do grupo.

Entra:	name -> buffer onde colocar o string de identificação.
	size -> tamanho do buffer "name" (número máximo de bytes a serem copiados).

Saída:	Se a operação foi realizada com sucesso, a função retorna "0" (zero).
	Em caso de erro, será retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int identify2 (char *name, int size)
{
    char names[150] = "Luís Augusto Weber Mercado - 265041\nMatheus Tavares Frigo - 262521\nNicholas de Aquino Lau - 268618\n";

    if( size >= strlen(names) )
    {
        strcpy(name, names);

        return OP_SUCCESS;
    }

    return OP_ERROR;
}

/*-----------------------------------------------------------------------------
Função: Criar um novo arquivo.
	O nome desse novo arquivo é aquele informado pelo parâmetro "filename".
	O contador de posição do arquivo (current pointer) deve ser colocado na posição zero.
	Caso já exista um arquivo ou diretório com o mesmo nome, a função deverá retornar um erro de criação.
	A função deve retornar o identificador (handle) do arquivo.
	Esse handle será usado em chamadas posteriores do sistema de arquivo para fins de manipulação do arquivo criado.

Entra:	filename -> nome do arquivo a ser criado.

Saída:	Se a operação foi realizada com sucesso, a função retorna o handle do arquivo (número positivo).
	Em caso de erro, deve ser retornado um valor negativo.
-----------------------------------------------------------------------------*/
FILE2 create2 (char *filename)
{
    if( !g_initialized )
    {
        if( __init() != 0 )
        {
            return OP_ERROR;
        }

    }

    return __record_create(filename, TYPEVAL_REGULAR);
}

/*-----------------------------------------------------------------------------
Função:	Apagar um arquivo do disco.
	O nome do arquivo a ser apagado é aquele informado pelo parâmetro "filename".

Entra:	filename -> nome do arquivo a ser apagado.

Saída:	Se a operação foi realizada com sucesso, a função retorna "0" (zero).
	Em caso de erro, será retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int delete2 (char *filename)
{
    if( !g_initialized )
    {
        if( __init() != 0 )
        {
            return OP_ERROR;
        }
    }

    return __record_delete(filename, TYPEVAL_REGULAR);
}

/*-----------------------------------------------------------------------------
Função:	Abre um arquivo existente no disco.
	O nome desse novo arquivo é aquele informado pelo parâmetro "filename".
	Ao abrir um arquivo, o contador de posição do arquivo (current pointer) deve ser colocado na posição zero.
	A função deve retornar o identificador (handle) do arquivo.
	Esse handle será usado em chamadas posteriores do sistema de arquivo para fins de manipulação do arquivo criado.
	Todos os arquivos abertos por esta chamada são abertos em leitura e em escrita.
	O ponto em que a leitura, ou escrita, será realizada é fornecido pelo valor current_pointer (ver função seek2).

Entra:	filename -> nome do arquivo a ser apagado.

Saída:	Se a operação foi realizada com sucesso, a função retorna o handle do arquivo (número positivo)
	Em caso de erro, deve ser retornado um valor negativo
-----------------------------------------------------------------------------*/
FILE2 open2 (char *filename)
{
    if( !g_initialized )
    {
        if( __init() != 0 )
        {
            return OP_ERROR;
        }
    }

    return __handler_alocate(filename, TYPEVAL_REGULAR);
}

/*-----------------------------------------------------------------------------
Função:	Fecha o arquivo identificado pelo parâmetro "handle".

Entra:	handle -> identificador do arquivo a ser fechado

Saída:	Se a operação foi realizada com sucesso, a função retorna "0" (zero).
	Em caso de erro, será retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int close2 (FILE2 handle)
{
    if( !g_initialized )
    {
        if( __init() != 0 )
        {
            return OP_ERROR;
        }
    }

    return __handler_free(handle, TYPEVAL_REGULAR);
}

/*-----------------------------------------------------------------------------
Função:	Realiza a leitura de "size" bytes do arquivo identificado por "handle".
	Os bytes lidos são colocados na área apontada por "buffer".
	Após a leitura, o contador de posição (current pointer) deve ser ajustado para o byte seguinte ao último lido.

Entra:	handle -> identificador do arquivo a ser lido
	buffer -> buffer onde colocar os bytes lidos do arquivo
	size -> número de bytes a serem lidos

Saída:	Se a operação foi realizada com sucesso, a função retorna o número de bytes lidos.
	Se o valor retornado for menor do que "size", então o contador de posição atingiu o final do arquivo.
	Em caso de erro, será retornado um valor negativo.
-----------------------------------------------------------------------------*/
int read2 (FILE2 handle, char *buffer, int size)
{
    if( !g_initialized )
    {
        if( __init() != 0 )
        {
            return OP_ERROR;
        }
    }

    if( handle >= 0 )
    {
        if( !(g_files[handle].free || g_files[handle].record == NULL) )
        {
            int i;
            int isEOF = 0;
            BYTE *readBuffer;
            struct t2fs_inode *inode = __inode_get_by_idx(g_files[handle].record->inodeNumber);

            for( i = 0; i < size && isEOF == 0; i++ )
            {
                readBuffer = __entry_read(g_files[handle].pointer, sizeof(BYTE), inode);

                if( readBuffer != NULL )
                {
                    char readChar = (char)*(readBuffer);

                    isEOF = readChar == 0;

                    if( !isEOF )
                    {
                        buffer[i] = readChar;
                        g_files[handle].pointer++;
                    }
                    else
                    {
                        buffer[i] = '\0';
                    }
                }
                else
                {
                    return i;
                }
            }

            return i;
        }
    }

    return OP_ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Realiza a escrita de "size" bytes no arquivo identificado por "handle".
	Os bytes a serem escritos estão na área apontada por "buffer".
	Após a escrita, o contador de posição (current pointer) deve ser ajustado para o byte seguinte ao último escrito.

Entra:	handle -> identificador do arquivo a ser escrito
	buffer -> buffer de onde pegar os bytes a serem escritos no arquivo
	size -> número de bytes a serem escritos

Saída:	Se a operação foi realizada com sucesso, a função retorna o número de bytes efetivamente escritos.
	Em caso de erro, será retornado um valor negativo.
-----------------------------------------------------------------------------*/
int write2 (FILE2 handle, char *buffer, int size)
{
    if( !g_initialized )
    {
        if( __init() != 0 )
        {
            return OP_ERROR;
        }
    }

    if( handle >= 0 )
    {
        if( !(g_files[handle].free || g_files[handle].record == NULL) )
        {
            struct t2fs_inode *inode = __inode_get_by_idx(g_files[handle].record->inodeNumber);

            if( (inode->bytesFileSize + size) > (inode->blocksFileSize * SECTOR_SIZE * g_sb->blockSize) )
            {
                if ( __block_alocate(inode, g_files[handle].record->inodeNumber) != OP_SUCCESS )
                {
                    return OP_ERROR;
                }

            }

            if( __inode_write_bytes(g_files[handle].pointer, buffer, size, inode) != OP_SUCCESS )
            {
                return OP_ERROR;
            }

            int newFileSize = ((size + g_files[handle].pointer) - inode->bytesFileSize);

            if( newFileSize > 0 )
            {
                inode->bytesFileSize += (size + g_files[handle].pointer) - inode->bytesFileSize;
            }

            g_files[handle].pointer += size;

            __inode_write(inode, g_files[handle].record->inodeNumber);

            return size;
        }
    }

    return OP_ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para truncar um arquivo.
	Remove do arquivo todos os bytes a partir da posição atual do contador de posição (CP)
	Todos os bytes a partir da posição CP (inclusive) serão removidos do arquivo.
	Após a operação, o arquivo deverá contar com CP bytes e o ponteiro estará no final do arquivo

Entra:	handle -> identificador do arquivo a ser truncado

Saída:	Se a operação foi realizada com sucesso, a função retorna "0" (zero).
	Em caso de erro, será retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int truncate2 (FILE2 handle)
{
    if( !g_initialized )
    {
        if( __init() != 0 )
        {
            return OP_ERROR;
        }
    }

    if( handle >= 0 )
    {
        if( !(g_files[handle].free || g_files[handle].record == NULL) )
        {
            struct t2fs_inode *inode = __inode_get_by_idx(g_files[handle].record->inodeNumber);

            int i;

            for( i = (g_files[handle].pointer / (g_sb->blockSize * SECTOR_SIZE)) + 1; i < inode->blocksFileSize; i++ )
            {
                if( __inode_remove_block(inode, g_files[handle].record->inodeNumber, __block_get_by_idx(i, inode)) != OP_SUCCESS )
                {
                    return OP_ERROR;
                }
            }

            int size = (g_sb->blockSize * SECTOR_SIZE) - (g_files[handle].pointer % (g_sb->blockSize * SECTOR_SIZE));
            char *buffer = (char*) calloc(size, sizeof(char));

            if( __inode_write_bytes(g_files[handle].pointer, buffer, size, inode) == OP_SUCCESS )
            {
                inode->bytesFileSize = g_files[handle].pointer;
            }

            return __inode_write(inode, g_files[handle].record->inodeNumber);
        }
    }

    return OP_ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Reposiciona o contador de posições (current pointer) do arquivo identificado por "handle".
	A nova posição é determinada pelo parâmetro "offset".
	O parâmetro "offset" corresponde ao deslocamento, em bytes, contados a partir do início do arquivo.
	Se o valor de "offset" for "-1", o current_pointer deverá ser posicionado no byte seguinte ao final do arquivo,
		Isso é útil para permitir que novos dados sejam adicionados no final de um arquivo já existente.

Entra:	handle -> identificador do arquivo a ser escrito
	offset -> deslocamento, em bytes, onde posicionar o "current pointer".

Saída:	Se a operação foi realizada com sucesso, a função retorna "0" (zero).
	Em caso de erro, será retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int seek2 (FILE2 handle, DWORD offset)
{
    if( !g_initialized )
    {
        if( __init() != 0 )
        {
            return OP_ERROR;
        }
    }

    if( handle >= 0 )
    {
        if( !(g_files[handle].free || g_files[handle].record == NULL) )
        {
            if( offset != -1 )
            {
                g_files[handle].pointer = offset;
            }
            else
            {
                g_files[handle].pointer = __inode_get_by_idx(g_files[handle].record->inodeNumber)->bytesFileSize;
            }

            return OP_SUCCESS;
        }
    }

    return OP_ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Criar um novo diretório.
	O caminho desse novo diretório é aquele informado pelo parâmetro "pathname".
		O caminho pode ser ser absoluto ou relativo.
	São considerados erros de criação quaisquer situações em que o diretório não possa ser criado.
		Isso inclui a existência de um arquivo ou diretório com o mesmo "pathname".

Entra:	pathname -> caminho do diretório a ser criado

Saída:	Se a operação foi realizada com sucesso, a função retorna "0" (zero).
	Em caso de erro, será retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int mkdir2 (char *pathname)
{
    if( !g_initialized )
    {
        if( __init() != 0 )
        {
            return OP_ERROR;
        }
    }

    return __record_create(pathname, TYPEVAL_DIRETORIO);
}

/*-----------------------------------------------------------------------------
Função:	Apagar um subdiretório do disco.
	O caminho do diretório a ser apagado é aquele informado pelo parâmetro "pathname".
	São considerados erros quaisquer situações que impeçam a operação.
		Isso inclui:
			(a) o diretório a ser removido não está vazio;
			(b) "pathname" não existente;
			(c) algum dos componentes do "pathname" não existe (caminho inválido);
			(d) o "pathname" indicado não é um arquivo;

Entra:	pathname -> caminho do diretório a ser criado

Saída:	Se a operação foi realizada com sucesso, a função retorna "0" (zero).
	Em caso de erro, será retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int rmdir2 (char *pathname)
{
    if( !g_initialized )
    {
        if( __init() != 0 )
        {
            return OP_ERROR;
        }
    }

    return __record_delete(pathname, TYPEVAL_DIRETORIO);
}

/*-----------------------------------------------------------------------------
Função:	Altera o diretório atual de trabalho (working directory).
		O caminho desse diretório é informado no parâmetro "pathname".
		São considerados erros:
			(a) qualquer situação que impeça a realização da operação
			(b) não existência do "pathname" informado.

Entra:	pathname -> caminho do novo diretório de trabalho.

Saída:	Se a operação foi realizada com sucesso, a função retorna "0" (zero).
		Em caso de erro, será retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int chdir2 (char *pathname)
{
    if( !g_initialized )
    {
        if( __init() != 0 )
        {
            return OP_ERROR;
        }
    }

    char* parsedPath = parse_path(pathname, g_cwd);

    if( parsedPath != NULL )
    {
        struct t2fs_record *record = __record_navigate(parsedPath);

        if( record != NULL )
        {
            if( record->TypeVal == TYPEVAL_DIRETORIO )
            {
                g_cwd = parsedPath;
                g_cwd_record = record;

                return OP_SUCCESS;
            }
        }
    }

    return OP_ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Informa o diretório atual de trabalho.
		O "pathname" do diretório de trabalho deve ser copiado para o buffer indicado por "pathname".
			Essa cópia não pode exceder o tamanho do buffer, informado pelo parâmetro "size".
		São considerados erros:
			(a) quaisquer situações que impeçam a realização da operação
			(b) espaço insuficiente no buffer "pathname", cujo tamanho está informado por "size".

Entra:	pathname -> buffer para onde copiar o pathname do diretório de trabalho
		size -> tamanho do buffer pathname

Saída:	Se a operação foi realizada com sucesso, a função retorna "0" (zero).
		Em caso de erro, será retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int getcwd2 (char *pathname, int size)
{
    if( !g_initialized )
    {
        if( __init() != 0 )
        {
            return OP_ERROR;
        }
    }

    if( size >= strlen(g_cwd) )
    {
        strncpy(pathname, g_cwd, strlen(g_cwd));

        pathname[strlen(g_cwd)] = '\0';

        return OP_SUCCESS;
    }

    return OP_ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Abre um diretório existente no disco.
	O caminho desse diretório é aquele informado pelo parâmetro "pathname".
	Se a operação foi realizada com sucesso, a função:
		(a) deve retornar o identificador (handle) do diretório
		(b) deve posicionar o ponteiro de entradas (current entry) na primeira posição válida do diretório "pathname".
	O handle retornado será usado em chamadas posteriores do sistema de arquivo para fins de manipulação do diretório.

Entra:	pathname -> caminho do diretório a ser aberto

Saída:	Se a operação foi realizada com sucesso, a função retorna o identificador do diretório (handle).
	Em caso de erro, será retornado um valor negativo.
-----------------------------------------------------------------------------*/
DIR2 opendir2 (char *pathname)
{
    if( !g_initialized )
    {
        if( __init() != 0 )
        {
            return OP_ERROR;
        }
    }

    return __handler_alocate(pathname, TYPEVAL_DIRETORIO);
}

/*-----------------------------------------------------------------------------
Função:	Realiza a leitura das entradas do diretório identificado por "handle".
	A cada chamada da função é lida a entrada seguinte do diretório representado pelo identificador "handle".
	Algumas das informações dessas entradas devem ser colocadas no parâmetro "dentry".
	Após realizada a leitura de uma entrada, o ponteiro de entradas (current entry) deve ser ajustado para a próxima entrada válida, seguinte à última lida.
	São considerados erros:
		(a) qualquer situação que impeça a realização da operação
		(b) término das entradas válidas do diretório identificado por "handle".

Entra:	handle -> identificador do diretório cujas entradas deseja-se ler.
	dentry -> estrutura de dados onde a função coloca as informações da entrada lida.

Saída:	Se a operação foi realizada com sucesso, a função retorna "0" (zero)
	Em caso de erro, será retornado um valor diferente de zero ( e "dentry" não será válido).
-----------------------------------------------------------------------------*/
int readdir2 (DIR2 handle, DIRENT2 *dentry)
{
    if( !g_initialized )
    {
        if( __init() != 0 )
        {
            return OP_ERROR;
        }
    }

    if( handle >= 0 )
    {
        if( !(g_dirs[handle].free || g_dirs[handle].record == NULL) )
        {
            struct t2fs_record *record;
            struct t2fs_inode *inode = __inode_get_by_idx(g_dirs[handle].record->inodeNumber);

            record = __record_get_by_idx(g_dirs[handle].pointer, inode);

            if( record != NULL )
            {
                while( record->TypeVal == TYPEVAL_INVALIDO )
                {
                    g_dirs[handle].pointer += 1;
                    record = __record_get_by_idx(g_dirs[handle].pointer, inode);

                    if( record == NULL )
                    {
                        break;
                    }
                }

                if( record != NULL )
                {
                    if( record->TypeVal == TYPEVAL_REGULAR || record->TypeVal == TYPEVAL_DIRETORIO )
                    {
                        strcpy(dentry->name, record->name);
                        dentry->fileType = record->TypeVal;
                        dentry->fileSize = (__inode_get_by_idx(record->inodeNumber))->bytesFileSize;

                        g_dirs[handle].pointer += 1;

                        return OP_SUCCESS;
                    }
                }
            }

            g_dirs[handle].pointer = 0;
        }
    }

    return OP_ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Fecha o diretório identificado pelo parâmetro "handle".

Entra:	handle -> identificador do diretório que se deseja fechar (encerrar a operação).

Saída:	Se a operação foi realizada com sucesso, a função retorna "0" (zero).
	Em caso de erro, será retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int closedir2 (DIR2 handle)
{
    if( !g_initialized )
    {
        if( __init() != 0 )
        {
            return OP_ERROR;
        }
    }

    return __handler_free(handle, TYPEVAL_DIRETORIO);
}
