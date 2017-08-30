CC          := gcc
CFLAGS      := -Wall -Wno-format-security -Wno-missing-braces -pthread -O2 -D NUM_OF_THREADS=88 -D _DEBUG -D NO_PRINT
PROJ_DIR    := $(shell dirname $(shell pwd))
NAME        := $(shell basename $(PROJ_DIR))

INC_FILES   := $(shell find $(PROJ_DIR)/code/inc -name *.h)
SRC_FILES   := $(shell find $(PROJ_DIR)/code/src -name *.c)

INC_DIRS    := $(addprefix -I, $(sort $(dir $(INC_FILES))))

.PHONY: $(NAME) clean

$(NAME): $(SRC_FILES)
	$(CC) $(CFLAGS) $(INC_DIRS) $^ -o $@

clean:
	rm $(NAME)

