#
# Makefile ESQUELETO
#
# DEVE ter uma regra "all" para geração da biblioteca
# regra "clean" para remover todos os objetos gerados.
#
# NECESSARIO adaptar este esqueleto de makefile para suas necessidades.
#
#

CC=gcc
LIB_DIR=./lib
INC_DIR=./include
EXP_DIR=./exemplo
TST_DIR=./teste
SRC_DIR=./src

all: comp gen test

comp:
	$(CC) -c $(SRC_DIR)/t2fs.c -o $(LIB_DIR)/t2fs.o -Wall
	$(CC) -c $(SRC_DIR)/parser.c -o $(LIB_DIR)/parser.o -Wall

gen:
	ar crs $(LIB_DIR)/libt2fs.a $(LIB_DIR)/t2fs.o $(LIB_DIR)/parser.o $(LIB_DIR)/apidisk.o $(LIB_DIR)/bitmap2.o

test:
	$(CC) -o $(EXP_DIR)/teste_dir  $(TST_DIR)/teste_dir.c -L$(LIB_DIR) -lt2fs -Wall
	#$(CC) $(TST_DIR)/teste_file.c -o $(EXP_DIR)/teste_file -Wall

clean:
	rm -rf $(LIB_DIR)/*.a $(LIB_DIR)/t2fs.o $(LIB_DIR)/parser.o $(SRC_DIR)/*.o $(INC_DIR)/*.o $(EXP_DIR)/teste_dir $(EXP_DIR)/teste_file $(TST_DIR)/*.o
