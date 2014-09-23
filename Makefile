BIN     = coolproxy
DIST    = coolproxy.tar.gz
CC     ?= cc
CHECK   = stylecheck.awk
SRC     = $(wildcard *.c)
INC     = $(wildcard *.h)
OBJ     = $(SRC:.c=.o)
DEP     = $(SRC:.c=.d)
CFLAGS  = -std=gnu99 -g
CFLAGS += -Wall -Werror -MMD
LDFLAGS =
TEST    = test-curl.sh

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
	@awk -f $(CHECK) $? && touch $@

wc:
	@wc -l $(SRC) $(INC) | sort -n

test: $(TEST) $(BIN)
	./$(TEST)

$(DIST): $(SRC) $(INC) $(CHECK) $(TEST) Makefile README
	tar czf $@ $^

dist: $(DIST)

clean:
	rm -f $(BIN) $(OBJ) $(DEP) $(DIST) check

.PHONY: clean wc test
