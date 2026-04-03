#include "protocol.h"
#include "network.h"
#include "session.h"
#include "game_logic.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WORD_BANK_FILE "words.txt"
#define MAX_WORD_BANK 512

typedef union {
    msg_join_t join;
    msg_guess_t guess;
    msg_rematch_t rematch;
    msg_welcome_t welcome;
    msg_game_start_t game_start;
    msg_guess_result_t guess_result;
    msg_opponent_t opponent;
    msg_round_over_t round_over;
    msg_match_over_t match_over;
    msg_error_t error;
} MessageBuf;


/* forward declarations */

static int setup_listener(int port);
static int find_free_slot(const ServerSession *session);
static void close_client(ServerSession *session, int player_id);

static int send_error_msg(int fd, const char *text);
static int send_welcome_msg(const ServerSession *session, int player_id);
static int send_game_start_msg(const ServerSession *session, int player_id);
static int send_guess_result_msg(const ServerSession *session, int player_id, char letter,
                                 GuessResult result);
static int send_round_over_msg(const ServerSession *session);
static int send_match_over_msg(const ServerSession *session);

static int start_next_round(ServerSession *session,
                            char words[][MAX_WORD_LEN + 1],
                            int word_count);

static int handle_join(ServerSession *session, int player_id, const msg_join_t *msg,
                       char words[][MAX_WORD_LEN + 1], int word_count);

static int handle_guess(ServerSession *session, int player_id, const msg_guess_t *msg,
                        char words[][MAX_WORD_LEN + 1], int word_count);

static int handle_client_message(ServerSession *session, int player_id,
                                 char words[][MAX_WORD_LEN + 1], int word_count);


/* socket setup */
static int setup_listener(int port) {
    int listenfd;
    struct sockaddr_in addr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(DEFAULT_PORT);

    if (bind(listenfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listenfd);
        return -1;
    }

    if (listen(listenfd, MAX_PLAYERS) < 0) {
        perror("listen");
        close(listenfd);
        return -1;
    }

    return listenfd;
}


/* client/session helpers */

static int find_free_slot(const ServerSession *session) {
    int i;

    for (i = 0; i < MAX_PLAYERS; i++) {
        if (!session->clients[i].connected) {
            return i;
        }
    }

    return -1;
}

static void close_client(ServerSession *session, int player_id) {
    if (session->clients[player_id].fd >= 0) {
        close(session->clients[player_id].fd);
    }

    remove_client(session, player_id);
}


/* send helpers */

static int send_error_msg(int fd, const char *text) {
    msg_error_t msg;

    memset(&msg, 0, sizeof(msg));
    strncpy(msg.message, text, sizeof(msg.message) - 1);

    return send_message(fd, MSG_ERROR, &msg, sizeof(msg));
}

static int send_welcome_msg(const ServerSession *session, int player_id) {
    msg_welcome_t msg;

    memset(&msg, 0, sizeof(msg));
    msg.player_id = (uint8_t)player_id;

    if (both_players_joined(session)) {
        strncpy(msg.message, "Both players joined. Starting game.", sizeof(msg.message) - 1);
    } else {
        strncpy(msg.message, "Joined successfully. Waiting for opponent.", sizeof(msg.message) - 1);
    }

    return send_message(session->clients[player_id].fd, MSG_WELCOME, &msg, sizeof(msg));
}

static int send_game_start_msg(const ServerSession *session, int player_id) {
    msg_game_start_t msg;
    const PlayerGameState *player_state = &session->game.players[player_id];
    int other_id = other_player(player_id);

    memset(&msg, 0, sizeof(msg));
    msg.round_num = (uint8_t)session->game.round_num;
    msg.total_rounds = (uint8_t)session->game.total_rounds;
    msg.category = 0;
    msg.word_len = (uint8_t)session->game.word_len;

    strncpy(msg.masked_word, player_state->masked_word, MAX_WORD_LEN);
    msg.masked_word[MAX_WORD_LEN] = '\0';

    strncpy(msg.opponent_name, session->clients[other_id].name, MAX_NAME_LEN);
    msg.opponent_name[MAX_NAME_LEN] = '\0';

    return send_message(session->clients[player_id].fd, MSG_GAME_START, &msg, sizeof(msg));
}

static int send_guess_result_msg(const ServerSession *session, int player_id, char letter,
                                 GuessResult result) {
    msg_guess_result_t msg;
    const PlayerGameState *player_state = &session->game.players[player_id];

    memset(&msg, 0, sizeof(msg));
    msg.letter = letter;
    msg.correct = (result == GUESS_CORRECT) ? 1 : 0;
    msg.lives_left = (uint8_t)player_state->lives_left;

    strncpy(msg.masked_word, player_state->masked_word, MAX_WORD_LEN);
    msg.masked_word[MAX_WORD_LEN] = '\0';

    strncpy(msg.guessed_letters, player_state->guessed_letters, ALPHABET_SIZE);
    msg.guessed_letters[ALPHABET_SIZE] = '\0';

    return send_message(session->clients[player_id].fd,
                        MSG_GUESS_RESULT, &msg, sizeof(msg));
}

static int send_round_over_msg(const ServerSession *session) {
    msg_round_over_t msg;
    int i;
    uint8_t winner_id = 255;

    memset(&msg, 0, sizeof(msg));

    if (session->game.state == ROUND_PLAYER0_WON) {
        winner_id = 0;
    } else if (session->game.state == ROUND_PLAYER1_WON) {
        winner_id = 1;
    } else if (session->game.state == ROUND_DRAW) {
        winner_id = 255;
    }

    msg.winner_id = winner_id;

    strncpy(msg.word, session->game.secret_word, MAX_WORD_LEN);
    msg.word[MAX_WORD_LEN] = '\0';

    for (i = 0; i < MAX_PLAYERS; i++) {
        msg.scores[i] = (uint8_t)session->game.scores[i];
    }

    for (i = 0; i < MAX_PLAYERS; i++) {
        if (session->clients[i].connected) {
            if (send_message(session->clients[i].fd, MSG_ROUND_OVER, &msg, sizeof(msg)) != 1) {
                return -1;
            }
        }
    }

    return 1;
}

static int send_match_over_msg(const ServerSession *session) {
    msg_match_over_t msg;
    int i;
    int winner;

    memset(&msg, 0, sizeof(msg));

    winner = match_winner(&session->game);
    if (winner < 0) {
        winner = 255;
    }

    msg.winner_id = (uint8_t)winner;

    for (i = 0; i < MAX_PLAYERS; i++) {
        msg.final_scores[i] = (uint8_t)session->game.scores[i];
    }

    if (winner == 0) {
        snprintf(msg.message, sizeof(msg.message),
                 "%s wins the match!", session->clients[0].name);
    } else if (winner == 1) {
        snprintf(msg.message, sizeof(msg.message),
                 "%s wins the match!", session->clients[1].name);
    } else {
        snprintf(msg.message, sizeof(msg.message), "The match ends in a draw.");
    }

    for (i = 0; i < MAX_PLAYERS; i++) {
        if (session->clients[i].connected) {
            if (send_message(session->clients[i].fd, MSG_MATCH_OVER, &msg, sizeof(msg)) != 1) {
                return -1;
            }
        }
    }

    return 1;
}


/* game flow helpers */

static int start_next_round(ServerSession *session,
                            char words[][MAX_WORD_LEN + 1],
                            int word_count) {
    char secret_word[MAX_WORD_LEN + 1];
    int next_round;

    if (choose_word(words, word_count, secret_word) != 0) {
        return -1;
    }

    if (!session->match_active) {
        start_match(session, secret_word);
    } else {
        next_round = session->game.round_num + 1;
        init_round(&session->game, secret_word, next_round, session->game.total_rounds);
    }

    if (send_game_start_msg(session, 0) != 1) {
        return -1;
    }
    if (send_game_start_msg(session, 1) != 1) {
        return -1;
    }

    return 1;
}


/* message handlers */

static int handle_join(ServerSession *session, int player_id, const msg_join_t *msg,
                       char words[][MAX_WORD_LEN + 1], int word_count) {
    ClientInfo *client = &session->clients[player_id];

    if (client->joined) {
        return send_error_msg(client->fd, "You already joined.");
    }

    strncpy(client->name, msg->name, MAX_NAME_LEN);
    client->name[MAX_NAME_LEN] = '\0';
    client->joined = 1;

    if (send_welcome_msg(session, player_id) != 1) {
        return -1;
    }

    if (both_players_joined(session) && !session->match_active) {
        if (send_welcome_msg(session, other_player(player_id)) != 1) {
            return -1;
        }

        if (start_next_round(session, words, word_count) != 1) {
            return -1;
        }
    }

    return 1;
}

static int handle_guess(ServerSession *session, int player_id, const msg_guess_t *msg,
                        char words[][MAX_WORD_LEN + 1], int word_count) {
    GuessResult guess_result;
    RoundState round_state;
    char guess_char = msg->letter;

    if (!session->match_active) {
        return send_error_msg(session->clients[player_id].fd, "Match not active.");
    }

    if (!session->clients[player_id].joined) {
        return send_error_msg(session->clients[player_id].fd, "You must join first.");
    }

    guess_result = process_guess(&session->game, player_id, guess_char);

    if (guess_result == GUESS_INVALID) {
        return send_error_msg(session->clients[player_id].fd, "Invalid guess.");
    }

    if (guess_result == GUESS_REPEAT) {
        return send_error_msg(session->clients[player_id].fd, "You already guessed that letter.");
    }

    if (send_guess_result_msg(session, player_id, guess_char, guess_result) != 1) {
        return -1;
    }

    round_state = update_round_state(&session->game);

    if (round_state != ROUND_IN_PROGRESS) {
        if (send_round_over_msg(session) != 1) {
            return -1;
        }

        if (match_winner(&session->game) != -1) {
            if (send_match_over_msg(session) != 1) {
                return -1;
            }
            session->match_active = 0;
            return 1;
        }

        if (session->game.round_num < session->game.total_rounds) {
            if (start_next_round(session, words, word_count) != 1) {
                return -1;
            }
        } else {
            if (send_match_over_msg(session) != 1) {
                return -1;
            }
            session->match_active = 0;
        }
    }

    return 1;
}

static int handle_client_message(ServerSession *session, int player_id,
                                 char words[][MAX_WORD_LEN + 1], int word_count) {
    MessageBuf msg;
    uint8_t opcode;
    int rc;
    int fd = session->clients[player_id].fd;

    rc = recv_message(fd, &opcode, &msg, sizeof(msg));
    if (rc != 1) {
        return rc;
    }

    switch (opcode) {
        case MSG_JOIN:
            return handle_join(session, player_id, &msg.join, words, word_count);

        case MSG_GUESS:
            return handle_guess(session, player_id, &msg.guess, words, word_count);

        case MSG_REMATCH:
            return send_error_msg(fd, "Rematch is not supported in this version.");

        default:
            return send_error_msg(fd, "Unknown or unsupported message type.");
    }
}


/* main */

int main(void) {
    ServerSession session;
    char words[MAX_WORD_BANK][MAX_WORD_LEN + 1];
    int word_count;
    fd_set readfds;
    int maxfd;
    int i;

    init_server_session(&session);

    word_count = load_words(WORD_BANK_FILE, words, MAX_WORD_BANK);
    if (word_count <= 0) {
        fprintf(stderr, "Failed to load words from %s\n", WORD_BANK_FILE);
        return 1;
    }

    session.listenfd = setup_listener(DEFAULT_PORT);
    if (session.listenfd < 0) {
        return 1;
    }

    printf("Server listening on port %d\n", DEFAULT_PORT);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(session.listenfd, &readfds);
        maxfd = session.listenfd;

        for (i = 0; i < MAX_PLAYERS; i++) {
            if (session.clients[i].connected) {
                FD_SET(session.clients[i].fd, &readfds);
                if (session.clients[i].fd > maxfd) {
                    maxfd = session.clients[i].fd;
                }
            }
        }

        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("select");
            break;
        }

        if (FD_ISSET(session.listenfd, &readfds)) {
            int connfd;
            int slot;
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);

            connfd = accept(session.listenfd, (struct sockaddr *)&client_addr, &client_len);
            if (connfd < 0) {
                perror("accept");
            } else {
                slot = find_free_slot(&session);

                if (slot < 0) {
                    send_error_msg(connfd, "Server full.");
                    close(connfd);
                } else {
                    session.clients[slot].fd = connfd;
                    session.clients[slot].connected = 1;
                    session.clients[slot].joined = 0;
                    session.num_connected++;

                    printf("Client connected in slot %d\n", slot);
                }
            }
        }

        for (i = 0; i < MAX_PLAYERS; i++) {
            if (session.clients[i].connected && FD_ISSET(session.clients[i].fd, &readfds)) {
                int rc = handle_client_message(&session, i, words, word_count);

                if (rc == 0) {
                    printf("Client %d disconnected\n", i);
                    close_client(&session, i);
                } else if (rc < 0) {
                    printf("Closing client %d due to error\n", i);
                    close_client(&session, i);
                }
            }
        }
    }

    for (i = 0; i < MAX_PLAYERS; i++) {
        if (session.clients[i].connected) {
            close(session.clients[i].fd);
        }
    }

    close(session.listenfd);
    return 0;
}