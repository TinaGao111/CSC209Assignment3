#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include "protocol.h"

typedef enum {
    GUESS_INVALID = -1,
    GUESS_REPEAT = 0,
    GUESS_CORRECT = 1,
    GUESS_WRONG = 2
} GuessResult;

typedef enum {
    ROUND_IN_PROGRESS = 0,
    ROUND_PLAYER0_WON,
    ROUND_PLAYER1_WON,
    ROUND_DRAW
} RoundState;

typedef struct {
    char masked_word[MAX_WORD_LEN + 1];
    char guessed_letters[ALPHABET_SIZE + 1];
    int guessed[ALPHABET_SIZE];
    int lives_left;
    int finished;
} PlayerGameState;

typedef struct {
    char secret_word[MAX_WORD_LEN + 1];
    int word_len;
    int round_num;
    int total_rounds;
    int scores[MAX_PLAYERS];
    PlayerGameState players[MAX_PLAYERS];
    RoundState state;
} GameState;

int load_words(const char *filename, char words[][MAX_WORD_LEN + 1], int max_words);
int choose_word(char words[][MAX_WORD_LEN + 1], int word_count, char *out_word);

void init_player_state(PlayerGameState *player, const char *secret_word);
void init_round(GameState *game, const char *secret_word, int round_num, int total_rounds);

GuessResult process_guess(GameState *game, int player_id, char guess);
RoundState update_round_state(GameState *game);

int player_has_won(const PlayerGameState *player);
int player_has_lost(const PlayerGameState *player);
char normalize_guess(char c);

#endif