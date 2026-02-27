#************************************************************************************************
# Authors: 	      Dan DeGenaro, Tian Li (NET IDs: drd92, tl995)
# Date: 		  Feb 27, 2026
# Purpose: 	      This is a simeple Makefile that builds an executable from multiple source files
#          	      	from sub directories. Source code has include directory where header file is stored
#	   				Makefile creates build_obj directory to store all the object files if the directory
#	   				does not already exist. It also creates an executable directory to store
#	   				the executable file.
# gcc Version:    gcc (GCC) 8.5.0 20210514 (Red Hat 8.5.0-28)
# Institution: 	  Georgetown University
# Disclaimer: 	  Adapted from a Makefile by Sam Sainju.         
#************************************************************************************************

#ANSI Colors for compile/link messages:
BLACK := \033[0,30m
RED   := \033[0;31m
GREEN := \033[0;32m
YELLOW:= \033[0;33m
BLUE  := \033[0;34m
PURPLE:= \033[0,35m
CYAN  := \033[0,36m
GREY  := \033[0,37m
RESET := \033[0m
#************************************************************************************************

# Directories
SRC_DIR := src
OBJ_DIR := build_obj
INC_DIR := include
EXE_DIR := application

# Compiler and flags
CC      := gcc
CFLAGS  := -Wall -Wextra -I$(INC_DIR)
# -Iinclude flag lets the compiler find header file in include/folder. So, even though
# INC_DIR is defined in this Makefile, it is not referenced anywhere and it works because
# the include directory is passed directly to the gcc compiler
#-Wall provides all potential warnings gcc encounters
#-Wextra provides additional warnings...

# Target executable name
TARGET	:= $(EXE_DIR)/hw2

# Find all .c files in SRC_DIR
SRCS    := $(wildcard $(SRC_DIR)/*.c)
        # other options are
        # SRCS := $(shell find $(SRC_DIR) -NAME '*.C')
        # OBJS := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

        # The wildcard function is used to expand filenames that match a pattern like in shell (*.c) 
        # find all files in the src directory that ends with .c and store their names in the SRC variable
        # manually: SRC := src/main.c src/menu.c etc
        # SRCS	:=$(SRC_DIR)/*.c this line will not work. wildcard function is required to match pattern

# Create matching .o paths in OBJ_DIR
OBJS    := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
# patsubst converts paths like src/foo.c --> build_obj/foo.o

# Default rule
all: $(TARGET)

# Link step
$(TARGET): $(OBJS) | $(EXE_DIR)
	@echo "$(RED)Linking..... the source files to object files....$(RESET)"
	$(CC) $(OBJS) -o $@

# Compile each .c file into .o
# pattern rules ($OBJ_DIR)/%.o:$(SRC_DIR)/%.c tells make how to build object file from a source file
# $@ expands to the target filename (eg: build/main.o
# $< expands to the first prerequisite, which is the source file (eg: src/main.c)
# instead of writing $(CC) $(CFLAGS) -c $(SRC_DIR)/main.c - o $(OBJ_DIR)/main.o
# for every file, one can write a pattern rule once, and use  $< to refer to the current source file
# being compiled. you can run individually as make build/main.o, this will expand to
# gcc -Wall -Wextra -Iinclude -c src/main.c -o build/main.o

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@echo "$(GREEN)Compiling $<........$(RESET)"
	$(CC) $(CFLAGS) -c $< -o $@

# Create build directory if it doesn't exist

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)
$(EXE_DIR):
	mkdir -p $(EXE_DIR)
run:
	$(TARGET)

# Clean rule
clean:
	rm -rf $(SRC_DIR)/*~
	rm -rf $(OBJ_DIR)
	rm -rf *~
	rm -rf $(INC_DIR)/*~
	rm -rf $(EXE_DIR)
	rm -rf test.tsv
	rm -rf text.txt

rebuild: clean all
