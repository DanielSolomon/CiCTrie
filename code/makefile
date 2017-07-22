CC := gcc
CFLAGS = #-Wall
BINARY = test

INC_DIR := inc/

SRC_FILES := $(shell find src/ -name *.c)

.PHONY: $(BINARY) clean

$(BINARY): $(SRC_FILES)
	$(CC) $(CFLAGS) -I$(INC_DIR) $^ -o $@

clean:
	rm $(BINARY)

