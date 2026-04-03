#ifndef SESSION_H
#define SESSION_H

#include "protocol.h"
#include "game_logic.h"

typedef struct {
    int fd;                         /* client's socket */
    int connected;                  /* 1 if socket connected */
    int joined;                     /* 1 if MSG_JOIN received */
    uint8_t player_id;              /* 0 or 1 */
    char name[MAX_NAME_LEN + 1];    /* player display name */
} ClientInfo;

typedef struct {
    int listenfd;                   /* listening socket */
    ClientInfo clients[MAX_PLAYERS];
    int num_connected;
    int match_active;               /* 1 if a best-of-3 match is running */
    GameState game;                 /* owned by game_logic */
} ServerSession;

/* initialization */
void init_client_info(ClientInfo *client, uint8_t player_id);
void init_server_session(ServerSession *session);

/* connection helpers */
/* Returns 1 if both player sockets are currently connected, else 0. */
int both_players_connected(const ServerSession *session);
/* Returns 1 if both players have sent MSG_JOIN, else 0. */
int both_players_joined(const ServerSession *session);
int other_player(int player_id);
/* Removes one client from the session and clears their stored info. */
void remove_client(ServerSession *session, int player_id);

/* match helpers */
void reset_match_state(ServerSession *session);
void start_match(ServerSession *session, const char *secret_word);

#endif /* SESSION_H */