BIN     = coolproxy
CC     ?= cc
SRC     = $(wildcard *.c)
INC     = $(wildcard *.h)
OBJ     = $(SRC:.c=.o)
DEP     = $(SRC:.c=.d)
CFLAGS  = -std=gnu99
CFLAGS += -Wall -Werror -MMD
LDFLAGS =

all: check $(BIN)

-include $(DEP)

$(BIN): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(SRC) $(INC)
	@awk -f stylecheck.awk $? && touch $@

clean:
	rm -f $(BIN) $(OBJ) $(DEP)

.PHONY: clean check
