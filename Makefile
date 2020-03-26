CC = gcc
CFLAGS = -Wall -g

BIN = sea
SRC = sea.c

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN)

install: $(BIN)
	sudo cp $(BIN) /usr/bin/$(BIN)

.PHONY: clean

clean:
	rm -f $(BIN)
