BIN     = coolproxy
CC     ?= cc
SRC     = $(wildcard *.c)
INC     = $(wildcard *.h)
OBJ     = $(SRC:.c=.o)
CFLAGS  = -std=gnu99
CFLAGS += -Wall -Werror
LDFLAGS =

all: check $(BIN)

$(BIN): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

.o:
	$(CC) -o $@ $< -c $(CFLAGS)

check: $(SRC) $(INC)
	@awk -f stylecheck.awk $? && touch $@

clean:
	rm -f $(BIN) $(OBJ)

.PHONY: clean check
