#ifndef __CONVERTER___
#define __CONVERTER___

/*-----------------------------------------------------------------------------
Função: Dado um buffer, lê um valor inteiro do mesmo (expresso em Little Endian)

Entra:
    buffer -> buffer com os valores
    start -> índice onde começa o valor no buffer
	size -> tamanho do valor a ser lido

Saída:
    O valor presente no buffer.
-----------------------------------------------------------------------------*/
int __get_value_from_buffer(unsigned char *buffer, int start, int size);

/*-----------------------------------------------------------------------------
Função: Converte um valor inteiro para um buffer com os bytes (como Little Endian)

Entra:
    value -> valor a ser convertido
    size -> tamanho do buffer desejado

Saída:
    Buffer expressando o valor.
-----------------------------------------------------------------------------*/
unsigned char* __convert_value_to_buffer(unsigned int value, int size);

/*-----------------------------------------------------------------------------
Função: Cria um 't2fs_superbloco' a partir do buffer

Entra:
    buffer -> buffer com os dados para a estrutura
    start -> começo do dado no buffer

Saída:
    A struct criada a partir dos dados.
-----------------------------------------------------------------------------*/
struct t2fs_superbloco* buffer_to_superblock(unsigned char *buffer, int start);

/*-----------------------------------------------------------------------------
Função: Cria um 't2fs_inode' a partir do buffer

Entra:
    buffer -> buffer com os dados para a estrutura
    start -> começo do dado no buffer

Saída:
    A struct criada a partir dos dados.
-----------------------------------------------------------------------------*/
struct t2fs_inode* buffer_to_inode(unsigned char *buffer, int start);

/*-----------------------------------------------------------------------------
Função: Cria um 't2fs_record' a partir do buffer

Entra:
    buffer -> buffer com os dados para a estrutura
    start -> começo do dado no buffer

Saída:
    A struct criada a partir dos dados.
-----------------------------------------------------------------------------*/
struct t2fs_record* buffer_to_record(unsigned char *buffer, int start);

/*-----------------------------------------------------------------------------
Função: Cria um buffer a partir de um 't2fs_inode'

Entra:
    inode -> inode a ser transformado

Saída:
    O buffer representando a estrutura.
-----------------------------------------------------------------------------*/
unsigned char* inode_to_buffer(struct t2fs_inode *inode);

/*-----------------------------------------------------------------------------
Função: Cria um buffer a partir de um 't2fs_record'

Entra:
    inode -> inode a ser transformado

Saída:
    O buffer representando a estrutura.
-----------------------------------------------------------------------------*/
unsigned char* record_to_buffer(struct t2fs_record *record);

#endif
