#include "../include/t2fs.h"
#include "../include/bitmap2.h"
#include "../include/apidisk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OP_SUCCESS 0
#define OP_ERROR -1

#define MAX_NUM_HANDLERS 10

#define FILENAME_SIZE 58

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
FILE_HANDLER g_files[MAX_NUM_HANDLERS];

/*-----------------------------------------------------------------------------
Handlers dos diretórios.
-----------------------------------------------------------------------------*/
DIR_HANDLER g_dirs[MAX_NUM_HANDLERS];

char *g_cwd;

/*-----------------------------------------------------------------------------
Função: Dado um buffer, lê um valor inteiro do mesmo (expresso em Little Endian)

Entra:
    buffer -> buffer com os valores
    start -> índice onde começa o valor no buffer
	size -> tamanho do valor a ser lido

Saída:
    Se a operação foi realizada com sucesso, retorna OP_SUCCESS
    Se ocorreu algum erro, retorna OP_ERROR.
-----------------------------------------------------------------------------*/
int __get_value_from_buffer(unsigned char *buffer, int start, int size)
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
Função: Encontra o ponteiro assoc. ao inodeNumber informado

Entra:
    inodeNumber -> número do inode a ser encontrado

Saída:
    Se a operação foi realizada com sucesso, retorna o inode
    Se ocorreu algum erro, retorna NULL
-----------------------------------------------------------------------------*/
struct t2fs_inode* __get_inode(int inodeNumber)
{
    unsigned char buffer[SECTOR_SIZE];
    unsigned int sector_rootInode = ((g_sb->superblockSize + g_sb->freeBlocksBitmapSize + g_sb->freeInodeBitmapSize) * g_sb->blockSize);
    struct t2fs_inode *inode = NULL;

    if( read_sector(sector_rootInode + inodeNumber, buffer) == 0 )
    {
        inode = (struct t2fs_inode*)malloc(sizeof(struct t2fs_inode));

        inode->blocksFileSize = __get_value_from_buffer(buffer, 0, 4);
        inode->bytesFileSize = __get_value_from_buffer(buffer, 4, 4);
        inode->dataPtr[0] = __get_value_from_buffer(buffer, 8, 4);
        inode->dataPtr[1] = __get_value_from_buffer(buffer, 12, 4);
        inode->singleIndPtr = __get_value_from_buffer(buffer, 16, 4);
        inode->doubleIndPtr = __get_value_from_buffer(buffer, 20, 4);
    }

    return inode;
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

    g_sb = (struct t2fs_superbloco*)malloc(sizeof(struct t2fs_superbloco));

    printf("Superblock reading: \n");

    printf("ID: ");
    for( i = 0; i < 4; i++ )
    {
        g_sb->id[i] = (char)buffer[i];
        printf("%c", g_sb->id[i]);
    }

    printf("\n");

    g_sb->version = __get_value_from_buffer(buffer, 4, 2);
    g_sb->superblockSize = __get_value_from_buffer(buffer, 6, 2);
    g_sb->freeBlocksBitmapSize = __get_value_from_buffer(buffer, 8, 2);
    g_sb->freeInodeBitmapSize = __get_value_from_buffer(buffer, 10, 2);
    g_sb->inodeAreaSize = __get_value_from_buffer(buffer, 12, 2);
    g_sb->blockSize = __get_value_from_buffer(buffer, 14, 2);
    g_sb->diskSize = __get_value_from_buffer(buffer, 16, 4);

    printf("Version: 0x%.4x (0x7e2 = 2018; 1 = first semester)\n", g_sb->version);
    printf ("SuperBlockSize: %d logical sectors\n", g_sb->superblockSize);
    printf ("FreeBlocksBitmapSize: %d blocks\n", g_sb->freeBlocksBitmapSize);
    printf ("FreeInodeBitmapSize: %d blocks\n", g_sb->freeInodeBitmapSize);
    printf ("InodeAreaSize: %d blocks\n", g_sb->inodeAreaSize);
    printf ("BlockSize: %d sectors\n", g_sb->blockSize);
    printf ("DiskSize: %d blocks\n", g_sb->diskSize);

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
        printf("Root dir. file size: %d blocks\n", g_ri->blocksFileSize);
        printf("Root dir. file size: %d bytes\n", g_ri->blocksFileSize);
        printf("Root dir. dataPtr[0]: %d\n", g_ri->dataPtr[0]);
        printf("Root dir. dataPtr[1]: %d\n", g_ri->dataPtr[1]);
        printf("Root dir. single ind. ptr.: %d\n", g_ri->singleIndPtr);
        printf("Root dir. double ind. ptr.: %d\n", g_ri->doubleIndPtr);

        return OP_SUCCESS;
    }

    return OP_ERROR;
}

/*-----------------------------------------------------------------------------
Função: Inicializa as varíaveis necessárias para correta execução

Saída:
    Se a operação foi realizada com sucesso, retorna "0" (OP_SUCCESS)
    Se ocorreu algum erro, retorna "-1" (OP_ERROR).
-----------------------------------------------------------------------------*/
int __initialize()
{
    int i;

    if( __read_superblock() == 0 && __read_rootInode() == 0 )
    {
        for(i = 0; i < MAX_NUM_HANDLERS; i++)
        {
            g_files[i].record = NULL;
            g_dirs[i].record = NULL;
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
    sprintf(name, "Luís Augusto Weber Mercado - 265041\nMatheus Tavares Frigo - 262521\nNicholas de Aquino Lau - 268618\n");

    return OP_SUCCESS;
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

    struct t2fs_inode *inode = (struct t2fs_inode*)malloc(sizeof(struct t2fs_inode));
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
    record->inodeNumber = i_bitmap;

    // TODO: write file entrys

    /*if( setBitmap2(d_bitmap, 1) == 0 && setBitmap2(i_bitmap, 1) == 0 )
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

int __get_free_file_handler()
{
    int i;

    for(i = 0; i < MAX_NUM_HANDLERS; i++)
    {
        if( g_files[i].record != NULL )
        {
            return i;
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

    printf("%s\n", g_cwd);

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

int __get_free_dir_handler()
{
    int i;

    for(i = 0; i < MAX_NUM_HANDLERS; i++)
    {
        if( g_dirs[i].record != NULL )
        {
            return i;
        }
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
        if( __initialize() != 0 )
        {
            return OP_ERROR;
        }
    }

    DIR2 freeHandler = __get_free_dir_handler();

    if( freeHandler != OP_ERROR )
    {

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
