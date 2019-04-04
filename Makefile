CC = gcc
CFLAGS = -Wall -g
LIBS = -lssl -lcrypto

BIN = sea
SRC = sea.c

$(BIN): $(SRC)
	@$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(LIBS)
	@echo "Done!"

install: $(BIN)
	@sudo cp $(BIN) /usr/bin/$(BIN)
	@echo "Done!"