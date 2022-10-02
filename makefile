SRC_DIR := src
OBJ_DIR := build
BIN_DIR := bin
BIN_NAME := customAlloc
EXEC := $(BIN_DIR)/$(BIN_NAME)
SRC_FILES := $(wildcard $(SRC_DIR)/*.c)
OBJ_FILES := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_FILES))
LFLAGS := 
CFLAGS := -Wall -Werror -g
CC:= gcc

make: $(OBJ_DIR) $(BIN_DIR) $(EXEC)
	./$(EXEC)

$(EXEC): $(OBJ_FILES)
	$(CC) -o $@ $^ $(LFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

rmobj:
	rm -rf $(OBJ_DIR)

memleak: $(OBJ_DIR) $(EXEC)
	valgrind --track-origins=yes --tool=memcheck --leak-check=yes $(EXEC)

opencore: $(EXEC)
	gdb $(EXEC) /tmp/$(shell ls /tmp | grep core | tail -1)

rmcore:
	rm -f $(shell ls /tmp | grep core | sed 's/^/\/tmp\//')

listcore:
	ls /tmp | grep core

recompile:
	make rmobj
	make
