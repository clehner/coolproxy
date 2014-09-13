BIN     = coolproxy
CC     ?= cc
SRC     = $(wildcard *.c)
OBJ     = $(SRC:.c=.o)
CFLAGS  = -std=gnu99
CFLAGS += -Wall -Werror
LDFLAGS =

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

.o:
	$(CC) -o $@ $< -c $(CFLAGS)

clean:
	rm -f $(BIN) $(OBJ)

.PHONY: clean
