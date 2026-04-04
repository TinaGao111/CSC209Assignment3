CC = gcc
PORT = 4242
CFLAGS = -Wall -Wextra -std=c99 -g -DPORT=$(PORT) -I. -Iserver -Igame

all: hangman_server hangman_client

hangman_server: server/server.o game/game_logic.o network.o server/session.o
	$(CC) $(CFLAGS) -o $@ $^

hangman_client: client/client.o network.o
	$(CC) $(CFLAGS) -o $@ $^

server/server.o: server/server.c protocol.h game/game_logic.h network.h server/session.h
	$(CC) $(CFLAGS) -c server/server.c -o $@

server/session.o: server/session.c server/session.h protocol.h game/game_logic.h
	$(CC) $(CFLAGS) -c server/session.c -o $@

client/client.o: client/client.c protocol.h network.h
	$(CC) $(CFLAGS) -c client/client.c -o $@

game/game_logic.o: game/game_logic.c game/game_logic.h protocol.h
	$(CC) $(CFLAGS) -c game/game_logic.c -o $@

network.o: network.c network.h protocol.h
	$(CC) $(CFLAGS) -c network.c -o $@

clean:
	rm -f hangman_server hangman_client network.o server/*.o client/*.o game/*.o

.PHONY: all clean
