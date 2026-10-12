# Makefile for ls-v1.1.0

CC      = gcc
CFLAGS  = -Wall -g

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

SRC     = $(SRC_DIR)/ls-v1.1.0.c
OBJ     = $(OBJ_DIR)/ls-v1.1.0.o
TARGET  = $(BIN_DIR)/ls

all: $(TARGET)

# Link the object file into the final executable
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# Compile the source file into an object file
$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ_DIR)/*.o $(TARGET)

.PHONY: all clean
