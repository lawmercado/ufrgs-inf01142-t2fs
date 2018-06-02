#ifndef __PARSER___
#define __PARSER___

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

/*-----------------------------------------------------------------------------
Função: Gera o caminho absoluto, removendo os links entre diretórios

Entra:
    path -> caminho a ser processado
    cwdPath -> caminho corrente

Saída:
    Se a operação foi realizada corretamente, o caminho absoluto direto
    Se houver algum erro, NULL.
-----------------------------------------------------------------------------*/
char* parse_path(char *path, char *cwdp);

#endif
