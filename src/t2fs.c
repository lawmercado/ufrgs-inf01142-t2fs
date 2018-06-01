#include "../include/t2fs.h"
#include "../include/bitmap2.h"
#include "../include/apidisk.h"
#include "../include/converter.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define OP_SUCCESS 0
#define OP_ERROR -1

#define MAX_NUM_HANDLERS 10

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

char *g_cwd;
struct t2fs_record *g_gwd_record;

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

/*-----------------------------------------------------------------------------
Função: Calcula o setor base dos inodes

Saída:
    Setor base dos inodes.
-----------------------------------------------------------------------------*/
unsigned int __get_inodes_base_sector()
{
    return (g_sb->superblockSize + g_sb->freeBlocksBitmapSize + g_sb->freeInodeBitmapSize) * g_sb->blockSize;
}

unsigned int __get_inode_sector(int inodeNumber)
{
    unsigned int base_sector = __get_inodes_base_sector();

    return base_sector + (inodeNumber * sizeof(struct t2fs_inode)) / SECTOR_SIZE;
}

unsigned int __get_inode_index(int inodeNumber)
{
    return (inodeNumber * sizeof(struct t2fs_inode)) / SECTOR_SIZE;
}

/*-----------------------------------------------------------------------------
Função: Calcula o setor base dos blocos de dados

Saída:
    Setor base dos blocos de dados.
-----------------------------------------------------------------------------*/
unsigned int __get_data_blocks_base_sector()
{
    return __get_inodes_base_sector() + (g_sb->inodeAreaSize) * g_sb->blockSize;
}

unsigned int __get_data_block_sector(int blockNumber)
{
    return __get_data_blocks_base_sector() + blockNumber * g_sb->blockSize;
}

/*-----------------------------------------------------------------------------
Função: Encontra o ponteiro assoc. ao inodeNumber informado

Entra:
    inodeNumber -> número do inode a ser encontrado

Saída:
    Se a operação foi realizada com sucesso, retorna o inode
    Se ocorreu algum erro, retorna NULL.
-----------------------------------------------------------------------------*/
struct t2fs_inode* __get_inode(int inodeNumber)
{
    unsigned char buffer[SECTOR_SIZE];
    unsigned int idxInode = __get_inode_index(inodeNumber);

    if( read_sector(__get_inode_sector(inodeNumber), buffer) == OP_SUCCESS )
    {
        return buffer_to_inode(buffer, idxInode);
    }

    return NULL;
}

/*-----------------------------------------------------------------------------
Função: Encontra o registro associado ao nome

Entra:
    name -> nome da entrada
    blockNumber -> número do bloco

Saída:
    Se a operação foi realizada com sucesso, retorna o record
    Se ocorreu algum erro, retorna NULL.
-----------------------------------------------------------------------------*/
struct t2fs_record* __get_record_by_name(char *name, int blockNumber)
{
    unsigned char buffer[SECTOR_SIZE];
    struct t2fs_record *record = NULL;
    int i;

    if( read_sector(__get_data_block_sector(blockNumber), buffer) == OP_SUCCESS )
    {
        for( i = 0; i < SECTOR_SIZE; i += sizeof(struct t2fs_record) )
        {
            record = buffer_to_record(buffer, i);

            if( strcmp(record->name, name) == 0 )
            {
                return record;
            }
        }
    }

    return NULL;
}

/*-----------------------------------------------------------------------------
Função: Encontra o registro associado com dado tipo

Entra:
    type -> tipo da entrada (TYPEVAL_REGULAR, TYPEVAL_INVALIDO ou TYPEVAL_DIRETORIO)
    blockNumber -> número do bloco

Saída:
    Se a operação foi realizada com sucesso, retorna o idx do record
    Se ocorreu algum erro, retorna OP_ERROR.
-----------------------------------------------------------------------------*/
unsigned int __get_idx_record_by_type(int type, int blockNumber)
{
    unsigned char buffer[SECTOR_SIZE];
    struct t2fs_record *record = NULL;
    int i;

    if( read_sector(__get_data_block_sector(blockNumber), buffer) == OP_SUCCESS )
    {
        for( i = 0; i < SECTOR_SIZE; i += sizeof(struct t2fs_record) )
        {
            record = buffer_to_record(buffer, i);

            if( record->TypeVal == type )
            {
                return i;
            }
        }
    }

    return OP_ERROR;
}

int __write_inode(struct t2fs_inode *inode, int inodeNumber)
{
    unsigned char buffer[SECTOR_SIZE], *buffer_inode = NULL;
    unsigned int sector = __get_inode_sector(inodeNumber);
    unsigned int idxInode = __get_inode_index(inodeNumber);
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
            printf("ESCREVEU INODE\n");
            return OP_SUCCESS;
        }
    }

    return OP_ERROR;
}

int __write_record(struct t2fs_record *record, int blockNumber)
{
    unsigned char buffer[SECTOR_SIZE], *buffer_record = NULL;
    unsigned int sector = __get_data_block_sector(blockNumber);
    unsigned int idxRecord = __get_idx_record_by_type(TYPEVAL_INVALIDO, blockNumber);
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
            printf("ESCREVEU RECORD\n");
            return OP_SUCCESS;
        }
    }

    return OP_ERROR;
}

/*-----------------------------------------------------------------------------
Função: Cria uma instância da struct record

Entra:
    name -> nome do record
    type -> tipo do record

Saída:
    Se a operação foi realizada com sucesso, retorna OP_SUCCESS
    Se ocorreu algum erro, retorna OP_ERROR.
-----------------------------------------------------------------------------*/
int __create_record(char *name, int type)
{
    struct t2fs_record *record = NULL;
    struct t2fs_inode *inode = NULL;

    record = (struct t2fs_record*)malloc(sizeof(struct t2fs_record));

    if( strlen(name) <= (RECORD_NAME_SIZE - 1) )
    {
        int b_inode = searchBitmap2(BITMAP_INODE, 0);
        int b_dados = searchBitmap2(BITMAP_DADOS, 0);

        if( b_inode > 0 && b_dados > 0 )
        {
            strncpy(record->name, name, RECORD_NAME_SIZE - 1);
            record->TypeVal = type;
            record->inodeNumber = b_inode;

            inode = (struct t2fs_inode*)malloc(sizeof(struct t2fs_inode));

            inode->blocksFileSize = 1;
            inode->bytesFileSize = g_sb->blockSize * SECTOR_SIZE;
            inode->dataPtr[0] = b_dados;
            inode->dataPtr[1] = INVALID_PTR;
            inode->singleIndPtr = INVALID_PTR;
            inode->doubleIndPtr = INVALID_PTR;

            __print_record("C. record", record);
            __print_inode("C. inode", inode);

            if( __write_inode(inode, b_inode) == OP_SUCCESS && __write_record(record, b_dados) == OP_SUCCESS )
            {
                setBitmap2(BITMAP_INODE, b_inode, 1);
                setBitmap2(BITMAP_DADOS, b_dados, 1);

                printf("SETOU\n");

                return OP_SUCCESS;
            }
        }
    }

    return OP_ERROR;
}

/*-----------------------------------------------------------------------------
Função: Encontra (ou não) um handler de arquivo disponível

Saída:
    Se há handler disponível, retorna o índice
    Se ocorreu algum erro, retorn OP_ERROR.
-----------------------------------------------------------------------------*/
FILE2 __get_free_file_handler()
{
    int i;

    for(i = 0; i < MAX_NUM_HANDLERS; i++)
    {
        if( g_files[i].free )
        {
            return i;
        }
    }

    return OP_ERROR;
}

/*-----------------------------------------------------------------------------
Função: Encontra (ou não) um handler de diretório disponível

Saída:
    Se há handler disponível, retorna o índice
    Se ocorreu algum erro, retorn OP_ERROR.
-----------------------------------------------------------------------------*/
DIR2 __get_free_dir_handler()
{
    int i;

    for(i = 0; i < MAX_NUM_HANDLERS; i++)
    {
        if( g_dirs[i].free )
        {
            return i;
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
int __read_superblock()
{
    int i;
    unsigned char buffer[SECTOR_SIZE];
    unsigned int sector_superblock = 0x00000000;

    if( read_sector(sector_superblock, buffer) != 0 )
    {
        return OP_ERROR;
    }

    g_sb = buffer_to_superblock(buffer, 0);

    __print_superbloco("Superbloco", g_sb);

    return OP_SUCCESS;
}

/*-----------------------------------------------------------------------------
Função: Realiza a leitura do inode referente ao diretório raiz

Saída:
    Se a operação foi realizada com sucesso, retorna OP_SUCCESS
    Se ocorreu algum erro, retorna OP_ERROR.
-----------------------------------------------------------------------------*/
int __read_rootInode()
{
    g_ri = __get_inode(0);

    if( g_ri != NULL )
    {
        __print_inode("Root inode", g_ri);

        __create_record("CEBOLA", TYPEVAL_REGULAR);

        if( __get_record_by_name(".", g_ri->dataPtr[0]) == NULL )
        {
            //__create_record(".", TYPEVAL_DIRETORIO);

            //__insert_record(record, block);
        }

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
int __initialize()
{
    int i;

    if( __read_superblock() == OP_SUCCESS && __read_rootInode() == OP_SUCCESS )
    {
        for(i = 0; i < MAX_NUM_HANDLERS; i++)
        {
            g_files[i].free = 1;
            g_dirs[i].free = 1;
        }

        g_cwd = "/";

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
        if( __initialize() != 0 )
        {
            return OP_ERROR;
        }

    }

    int d_bitmap = 0, i_bitmap = 0;

    // Gets a free bitmap entry
    d_bitmap = searchBitmap2(BITMAP_DADOS, 0);

    if( d_bitmap <= 0 )
    {
        printf("Error: can't find a free block!");

        return OP_ERROR;
    }

    // TODO: Parse the filename

    // Gets a free inode bitmap entry
    i_bitmap = searchBitmap2(BITMAP_INODE, 0);

    if( i_bitmap <= 0 )
    {
        printf("Error: can't find a free inode!");

        return OP_ERROR;
    }

    // TODO: test filename existance

    /*struct t2fs_inode *inode = (struct t2fs_inode*)malloc(sizeof(struct t2fs_inode));
    inode->blocksFileSize = 1;
    inode->bytesFileSize = 0;
    inode->dataPtr[0] = d_bitmap;
    inode->dataPtr[1] = INVALID_PTR;
    inode->singleIndPtr = INVALID_PTR;
    inode->doubleIndPtr = INVALID_PTR;

    struct t2fs_record *record = (struct t2fs_record*)malloc(sizeof(struct t2fs_record));

    strncpy(record->name, filename, FILENAME_SIZE - 1);
    printf("%s\n", record->name);

    record->TypeVal = TYPEVAL_REGULAR;
    record->inodeNumber = i_bitmap;*/

    // TODO: write file entrys

    /*if( setBitmap2(d_bitmap, 1) == OP_SUCCESS && setBitmap2(i_bitmap, 1) == OP_SUCCESS )
    {
        return OP_SUCCESS;
    }*/

    return OP_ERROR;
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
        if( __initialize() != 0 )
        {
            return OP_ERROR;
        }
    }

    return OP_ERROR;
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
        if( __initialize() != 0 )
        {
            return OP_ERROR;
        }
    }

    return OP_ERROR;
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
        if( __initialize() != 0 )
        {
            return OP_ERROR;
        }
    }

    return OP_ERROR;
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
        if( __initialize() != 0 )
        {
            return OP_ERROR;
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
        if( __initialize() != 0 )
        {
            return OP_ERROR;
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
        if( __initialize() != 0 )
        {
            return OP_ERROR;
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
        if( __initialize() != 0 )
        {
            return OP_ERROR;
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
        if( __initialize() != 0 )
        {
            return OP_ERROR;
        }
    }

    return OP_ERROR;
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
        if( __initialize() != 0 )
        {
            return OP_ERROR;
        }
    }

    return OP_ERROR;
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
        if( __initialize() != 0 )
        {
            return OP_ERROR;
        }
    }

    g_cwd = pathname;

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
        if( __initialize() != 0 )
        {
            return OP_ERROR;
        }
    }

    return OP_ERROR;
}

struct t2fs_record* __navigate(char *pathname)
{
    char *cpathname, *abspathname;
    int i;

    cpathname = strdup(pathname);

    if( strlen(pathname) > 0 )
    {
        if( strlen(pathname) == 1 && pathname[0] == '/' )
        {
            struct t2fs_record *record = __get_record_by_name(".", g_ri->dataPtr[0]);

            __print_record("Root record", record);

            return record;
        }
        else
        {
            for(i = 0; i < strlen(pathname); i++)
            {
                // Caminho absoluto
                if( i == 0 && pathname[i] == '/' )
                {
                    abspathname = cpathname;
                }
                else // Oi
                {
                    abspathname = strcat(g_cwd, cpathname);
                }
            }
        }
    }

    return NULL;
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
        if( __initialize() != 0 )
        {
            return OP_ERROR;
        }
    }

    if ( strlen(pathname) > 0 )
    {
        DIR2 freeHandler = __get_free_dir_handler();

        if( freeHandler != OP_ERROR )
        {
            g_dirs[freeHandler].record = __navigate(pathname);
            g_dirs[freeHandler].pointer = 0;
            g_dirs[freeHandler].free = 0;
        }
    }

    return OP_ERROR;
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

Saída:	Se a operação foi realizada com sucesso, a função retorna "0" (zero).
	Em caso de erro, será retornado um valor diferente de zero ( e "dentry" não será válido)
-----------------------------------------------------------------------------*/
int readdir2 (DIR2 handle, DIRENT2 *dentry)
{
    if( !g_initialized )
    {
        if( __initialize() != 0 )
        {
            return OP_ERROR;
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
        if( __initialize() != 0 )
        {
            return OP_ERROR;
        }
    }

    return OP_ERROR;
}
