#include "session.h"

#include <string.h>

void init_client_info(ClientInfo *client, uint8_t player_id) {
    client->fd = -1;
    client->connected = 0;
    client->joined = 0;
    client->player_id = player_id;
    client->name[0] = '\0';
}

void init_server_session(ServerSession *session) {
    int i;

    session->listenfd = -1;
    session->num_connected = 0;
    session->match_active = 0;

    for (i = 0; i < MAX_PLAYERS; i++) {
        init_client_info(&session->clients[i], (uint8_t)i);
    }

    memset(&session->game, 0, sizeof(session->game));
}

int both_players_connected(const ServerSession *session) {
    return session->clients[0].connected && session->clients[1].connected;
}

int both_players_joined(const ServerSession *session) {
    return session->clients[0].joined && session->clients[1].joined;
}

int other_player(int player_id) {
    if (player_id == 0) {
        return 1;
    } else {
        return 0;
    }
}

void remove_client(ServerSession *session, int player_id) {
    ClientInfo *client = &session->clients[player_id];

    if (client->connected) {
        session->num_connected--;
        if (session->num_connected < 0) {
            session->num_connected = 0;
        }
    }

    /* reset client info */
    init_client_info(client, (uint8_t)player_id);

    /* stop current match if a player disconnects */
    session->match_active = 0;
    memset(&session->game, 0, sizeof(session->game));
}

void reset_match_state(ServerSession *session) {
    session->match_active = 0;
    memset(&session->game, 0, sizeof(session->game));
}

void start_match(ServerSession *session, const char *secret_word) {
    session->match_active = 1;

    /*
     * Start round 1 of a best-of-3 match.
     * game_logic owns secret_word, masked words, scores, lives, etc.
     */
    init_round(&session->game, secret_word, 1, 3);
}