CC = gcc
PORT = 4242
CFLAGS = -Wall -Wextra -std=c99 -g -DPORT=$(PORT) -I. -Iserver -Igame_logic

all: hangman_server hangman_client

hangman_server: server/server.o game_logic/game_logic.o server/network.o server/session.o
	$(CC) $(CFLAGS) -o $@ $^

hangman_client: client/client.o server/network.o
	$(CC) $(CFLAGS) -o $@ $^

server/server.o: server/server.c protocol.h game_logic/game_logic.h server/network.h server/session.h
	$(CC) $(CFLAGS) -c server/server.c -o $@

server/session.o: server/session.c server/session.h protocol.h game_logic/game_logic.h
	$(CC) $(CFLAGS) -c server/session.c -o $@

server/network.o: server/network.c server/network.h protocol.h
	$(CC) $(CFLAGS) -c server/network.c -o $@

client/client.o: client/client.c protocol.h server/network.h
	$(CC) $(CFLAGS) -c client/client.c -o $@

game_logic/game_logic.o: game_logic/game_logic.c game_logic/game_logic.h protocol.h
	$(CC) $(CFLAGS) -c game_logic/game_logic.c -o $@

clean:
	rm -f hangman_server hangman_client server/*.o client/*.o game_logic/*.o

.PHONY: all clean
