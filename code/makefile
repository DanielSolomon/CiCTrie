CC          := gcc
CFLAGS      := -Wall
PROJ_DIR    := $(shell dirname $(shell pwd))
NAME        := $(shell basename $(PROJ_DIR))

INC_FILES   := $(shell find $(PROJ_DIR) -name *.h)
SRC_FILES   := $(shell find $(PROJ_DIR) -name *.c)

INC_DIRS    := $(addprefix -I, $(sort $(dir $(INC_FILES))))

.PHONY: $(NAME) clean

$(NAME): $(SRC_FILES)
	$(CC) $(CFLAGS) $(INC_DIRS) $^ -o $@

clean:
	rm $(NAME)

