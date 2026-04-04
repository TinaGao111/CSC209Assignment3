#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>

#include "protocol.h"
#include "network.h"

int server_fd;
int my_id;
char my_name[MAX_NAME_LEN + 1];
char opp_name[MAX_NAME_LEN + 1];

char my_word[MAX_WORD_LEN + 1];
char opp_word[MAX_WORD_LEN + 1];
int my_lives;
int opp_lives;
char my_guesses[ALPHABET_SIZE + 1];
int round_num, total_rounds;
int game_active;
int match_over;


// prints the game board for both players
void print_board() {
    int i;
    printf("\n=== HANGMAN RACE  Round %d/%d ===\n\n", round_num, total_rounds);

    printf("  You (%s)\n", my_name);
    printf("    Word:    ");
    for (i = 0; my_word[i]; i++)
        printf("%c ", my_word[i]);
    printf("\n    Lives:   ");
    for (i = 0; i < my_lives; i++) printf("* ");
    for (i = my_lives; i < MAX_LIVES; i++) printf("x ");
    printf("(%d/%d)\n", my_lives, MAX_LIVES);
    printf("    Guessed: [%s]\n\n", my_guesses);

    printf("  Opponent (%s)\n", opp_name);
    printf("    Word:    ");
    for (i = 0; opp_word[i]; i++)
        printf("%c ", opp_word[i]);
    printf("\n    Lives:   ");
    for (i = 0; i < opp_lives; i++) printf("* ");
    for (i = opp_lives; i < MAX_LIVES; i++) printf("x ");
    printf("(%d/%d)\n", opp_lives, MAX_LIVES);

    printf("\n================================\n");
    printf("Guess a letter > ");
    fflush(stdout);
}


// reads one message from the server and handles it
// returns 0 if ok, -1 if we need to quit
int handle_server_msg() {
    uint8_t opcode;
    char buf[256];
    int ret;

    ret = recv_message(server_fd, &opcode, buf, sizeof(buf));
    if (ret == 0) {
        // server disconnected
        if (!match_over)
            printf("\nServer closed the connection.\n");
        return -1;
    } else if (ret < 0) {
        fprintf(stderr, "\nError reading from server\n");
        return -1;
    }

    switch (opcode) {
        case MSG_WELCOME: {
            msg_welcome_t *w = (msg_welcome_t *)buf;
            my_id = w->player_id;
            printf("%s\n", w->message);
            fflush(stdout);
            break;
        }
        case MSG_GAME_START: {
            msg_game_start_t *gs = (msg_game_start_t *)buf;
            round_num = gs->round_num;
            total_rounds = gs->total_rounds;
            my_lives = MAX_LIVES;
            opp_lives = MAX_LIVES;
            strncpy(my_word, gs->masked_word, MAX_WORD_LEN);
            my_word[MAX_WORD_LEN] = '\0';
            strncpy(opp_name, gs->opponent_name, MAX_NAME_LEN);
            opp_name[MAX_NAME_LEN] = '\0';
            // fill opponent word with underscores
            memset(opp_word, '_', gs->word_len);
            opp_word[gs->word_len] = '\0';
            memset(my_guesses, 0, sizeof(my_guesses));
            game_active = 1;

            printf("\n--- Round %d of %d (word is %d letters) ---\n",
                   round_num, total_rounds, gs->word_len);
            print_board();
            break;
        }
        case MSG_GUESS_RESULT: {
            msg_guess_result_t *gr = (msg_guess_result_t *)buf;
            strncpy(my_word, gr->masked_word, MAX_WORD_LEN);
            my_word[MAX_WORD_LEN] = '\0';
            my_lives = gr->lives_left;
            // uint8_t wraps -1 to 255, so cap it
            if (my_lives > MAX_LIVES) my_lives = 0;
            strncpy(my_guesses, gr->guessed_letters, ALPHABET_SIZE);
            my_guesses[ALPHABET_SIZE] = '\0';

            printf("\n  You guessed '%c' -- %s!\n",
                   gr->letter, gr->correct ? "HIT" : "MISS");
            print_board();
            break;
        }
        case MSG_OPPONENT: {
            msg_opponent_t *o = (msg_opponent_t *)buf;
            strncpy(opp_word, o->opp_masked_word, MAX_WORD_LEN);
            opp_word[MAX_WORD_LEN] = '\0';
            opp_lives = o->opp_lives_left;
            if (opp_lives > MAX_LIVES) opp_lives = 0;

            printf("\n  %s guessed '%c' -- %s!\n",
                   opp_name, o->letter, o->correct ? "HIT" : "MISS");
            print_board();
            break;
        }
        case MSG_ROUND_OVER: {
            msg_round_over_t *ro = (msg_round_over_t *)buf;
            game_active = 0;

            printf("\n-----------------------------\n");
            printf("  Round %d over!\n", round_num);
            printf("  The word was: %s\n", ro->word);
            if (ro->winner_id == 255)
                printf("  Result: Draw!\n");
            else if (ro->winner_id == my_id)
                printf("  YOU WIN this round!\n");
            else
                printf("  %s wins this round.\n", opp_name);
            printf("  Score: %s %d - %d %s\n",
                   my_name, ro->scores[my_id],
                   ro->scores[1 - my_id], opp_name);
            printf("-----------------------------\n\n");
            fflush(stdout);
            break;
        }
        case MSG_MATCH_OVER: {
            msg_match_over_t *mo = (msg_match_over_t *)buf;
            game_active = 0;
            match_over = 1;

            printf("\n*****************************\n");
            printf("  MATCH OVER\n");
            printf("  %s\n", mo->message);
            printf("  Final: %s %d - %d %s\n",
                   my_name, mo->final_scores[my_id],
                   mo->final_scores[1 - my_id], opp_name);
            if (mo->winner_id == (uint8_t)my_id)
                printf("  Congratulations, you won!\n");
            else if (mo->winner_id == 255)
                printf("  It's a tie!\n");
            else
                printf("  Better luck next time!\n");
            printf("*****************************\n");
            printf("\nThanks for playing!\n");
            fflush(stdout);
            break;
        }
        case MSG_ERROR: {
            msg_error_t *e = (msg_error_t *)buf;
            printf("\n[Server error] %s\n", e->message);
            if (game_active)
                printf("Guess a letter > ");
            fflush(stdout);
            break;
        }
        default:
            fprintf(stderr, "Unknown opcode 0x%02x\n", opcode);
            break;
    }
    return 0;
}


int main(int argc, char *argv[]) {
    char *host = "127.0.0.1";
    int port = PORT;
    struct sockaddr_in addr;
    msg_join_t join;
    fd_set readfds;
    int maxfd;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <name> [host] [port]\n", argv[0]);
        return 1;
    }
    strncpy(my_name, argv[1], MAX_NAME_LEN);
    my_name[MAX_NAME_LEN] = '\0';
    if (argc > 2) host = argv[2];
    if (argc > 3) port = atoi(argv[3]);

    // connect to server
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address: %s\n", host);
        close(server_fd);
        return 1;
    }
    if (connect(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(server_fd);
        return 1;
    }
    printf("Connected to %s:%d as \"%s\"\n", host, port, my_name);

    // send join
    memset(&join, 0, sizeof(join));
    strncpy(join.name, my_name, MAX_NAME_LEN);
    if (send_message(server_fd, MSG_JOIN, &join, sizeof(join)) != 1) {
        fprintf(stderr, "Failed to send join\n");
        close(server_fd);
        return 1;
    }

    // main loop: use select to handle both stdin and server
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(server_fd, &readfds);
        maxfd = server_fd > STDIN_FILENO ? server_fd : STDIN_FILENO;

        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        // server sent something
        if (FD_ISSET(server_fd, &readfds)) {
            if (handle_server_msg() < 0)
                break;
        }

        // user typed something
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char input[64];
            char *p;
            char letter;
            msg_guess_t gm;

            if (fgets(input, sizeof(input), stdin) == NULL)
                break;

            p = input;
            while (*p && isspace((unsigned char)*p))
                p++;
            if (*p == '\0')
                continue;

            if (game_active) {
                letter = tolower((unsigned char)*p);
                if (letter < 'a' || letter > 'z') {
                    printf("Please enter a letter (a-z): ");
                    fflush(stdout);
                    continue;
                }
                gm.letter = letter;
                if (send_message(server_fd, MSG_GUESS, &gm, sizeof(gm)) != 1) {
                    printf("\nLost connection to server.\n");
                    break;
                }
            }
        }
    }

    close(server_fd);
    return 0;
}
