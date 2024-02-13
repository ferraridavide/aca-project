# Define variables for directories
CRAY_DIR = ../c-ray
DRIVER_DIR = $(CRAY_DIR)src/driver/encoders
INCLUDE_DIR = $(CRAY_DIR)/include
LIB_DIR = $(CRAY_DIR)/lib
SRC_DIRS = $(CRAY_DIR)/src/lib $(CRAY_DIR)/src/driver $(CRAY_DIR)/src/common $(CRAY_DIR)/generated/ ./src

# Define compiler and flags
CC = mpicc
CFLAGS = -g -I$(DRIVER_DIR) -I$(INCLUDE_DIR) -L$(LIB_DIR) -lc-ray -lpthread -lm -ldl 

# Find all .c files in the specified directories
SRCS = $(shell find $(SRC_DIRS) -name '*.c' | grep -v 'main.c')

# Define the program output
OUTPUT = ../../data/cray-demo

all: $(OUTPUT)

$(OUTPUT): main-mpi.c $(SRCS)
	@$(CC) $^ -o $@ $(CFLAGS)

clean:
	@rm -f $(OUTPUT)