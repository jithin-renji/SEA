CC = gcc
CFLAGS = -Wall -g

BIN = sea
SRC = sea.c

$(BIN): $(SRC)
	@$(CC) $(CFLAGS) $(SRC) -o $(BIN)
	@echo "Done!"

install: $(BIN)
	@sudo cp $(BIN) /usr/bin/$(BIN)
	@echo "Done!"
