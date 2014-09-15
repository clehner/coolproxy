BIN     = coolproxy
CC     ?= cc
SRC     = $(wildcard *.c)
INC     = $(wildcard *.h)
OBJ     = $(SRC:.c=.o)
DEP     = $(SRC:.c=.d)
CFLAGS  = -std=gnu99
CFLAGS += -Wall -Werror -MMD
LDFLAGS =

ifndef V
	QUIET_CC   = @echo ' CC   ' $<;
	QUIET_LINK = @echo ' LINK ' $@;
endif

all: check $(BIN)

-include $(DEP)

$(BIN): $(OBJ)
	$(QUIET_LINK)$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(QUIET_CC)$(CC) $(CFLAGS) -c -o $@ $<

check: $(SRC) $(INC)
	@awk -f stylecheck.awk $? && touch $@

clean:
	rm -f $(BIN) $(OBJ) $(DEP)

.PHONY: clean
