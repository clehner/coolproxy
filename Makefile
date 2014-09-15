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

check:
	@awk '/\s$$/ {\
			status=1;\
			print "Found lines ending with whitespace:";\
			print FILENAME ":" FNR;\
		}\
		END { exit status }' $(SRC) $(INC)

clean:
	rm -f $(BIN) $(OBJ)

.PHONY: clean check
