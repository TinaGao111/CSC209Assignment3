#include "game_logic.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Returns 1 if guess appears anywhere in word, else 0.*/
static int secret_contains(const char *word, char guess) {
    for (int i = 0; word[i] != '\0'; i++) {
        if (word[i] == guess) {
            return 1;
        }
    }
    return 0;
}

/* Reveals every occurrence of guess inside masked_word */
static void reveal_letters(const char *secret_word, char *masked_word, char guess) {
    for (int i = 0; secret_word[i] != '\0'; i++){
        if (secret_word[i] == guess) {
            masked_word[i] = guess;
        }
    }
}

/* Appends guess to the player's guessed_letters string */
static void append_guessed_letters(PlayerGameState *player, char guess){
    size_t len = strlen(player->guessed_letters);
    if(len < ALPHABET_SIZE){
        player->guessed_letters[len] = guess;
        player->guessed_letters[len + 1] = '\0';
    }
}

/* Reads the word bank from a file. */
int load_words(const char *filename, char words[][MAX_WORD_LEN + 1], int max_words) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        return -1;
    }

    int count = 0;
    while(count < max_words && fgets(words[count], MAX_WORD_LEN + 1, fp) != NULL) {
        size_t len = strlen(words[count]);

        /* Remove trailing new line / carriage return characters */
        while(len > 0 && (words[count][len - 1] == '\n' || words[count][len - 1] == '\r')) {
            words[count][len - 1] = '\0';
            len--;
        }

        /* Skip empty lines. */
        if(len == 0) {
            continue;
        }

        int valid = 1;

        /* Ensure the word contains only letters; convert to lowercase. */
        for(size_t i = 0; i < len; i++) {
            if(!isalpha((unsigned char)words[count][i])) {
                valid = 0;
                break;
            }
            words[count][i] = (char)tolower((unsigned char)words[count][i]);
        }

        if(!valid || len > MAX_WORD_LEN) {
            continue;
        }

        count++;
    }
    fclose(fp);
    return count;
}

/* Randomly chooses a word as the secret_word from the word bank. */
int choose_word(char words[][MAX_WORD_LEN + 1], int word_count, char *out_word) {
    if (word_count <= 0 || out_word == NULL) {
        return -1;
    }

    static int seeded = 0;
    if(!seeded){
        srand((unsigned int)time(NULL));
        seeded = 1;
    }

    int index = rand() % word_count;
    strcpy(out_word, words[index]);
    return 0;
}
 
/* Initializes the player's game state for a fresh round. */
void init_player_state(PlayerGameState *player, const char *secret_word) {
    int len = (int)strlen(secret_word);

    for(int i = 0; i < len; i++) {
        player->masked_word[i] = '_';
    }
    player->masked_word[len] = '\0';

    player->guessed_letters[0] = '\0';

    for (int i = 0; i < ALPHABET_SIZE; i++) {
        player->guessed[i] = 0;
    }

    player->lives_left = MAX_LIVES;
    player->finished = 0;
}

/* Initializes a round using one shared secret_word. */
void init_round(GameState * game, const char *secret_word, int round_num, int total_rounds) {
    if (game == NULL || secret_word == NULL) {
        return;
    }

    strncpy(game->secret_word, secret_word, MAX_WORD_LEN);
    game->secret_word[MAX_WORD_LEN] = '\0';

    game->word_len = (int)strlen(game->secret_word);
    game->round_num = round_num;

    game->total_rounds = total_rounds;

    /* Fresh match: reset scores when starting round 1. */
    if (round_num == 1) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            game->scores[i] = 0;
        }
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        init_player_state(&game->players[i], secret_word);
    }

    game->state = ROUND_IN_PROGRESS;
}

/* Normalizes a guess to lowercase and checks if it is a valid character. */
char normalize_guess(char c) {
    if (!isalpha((unsigned char)c)) {
        return '\0';
    }
    return (char)tolower((unsigned char)c);
}

/* Checks whether the masked word still has '_' */
int player_has_won(const PlayerGameState *player) {
    for (int i = 0; player->masked_word[i] != '\0'; i++) {
        if (player->masked_word[i] == '_') {
            return 0;
        }
    }
    return 1;
}

/* Checks whether the player has lost all their lives.*/
int player_has_lost(const PlayerGameState * player) {
    return player->lives_left <= 0;
}

/* Checks both players and decides the round outcome. */
RoundState update_round_state(GameState *game) {
    int p0_won = player_has_won(&game->players[0]);
    int p1_won = player_has_won(&game->players[1]);
    int p0_lost = player_has_lost(&game->players[0]);
    int p1_lost = player_has_lost(&game->players[1]);

    /* If round already ended earlier, do not score it again */
    if (game->state != ROUND_IN_PROGRESS) {
        return game->state;
    }
    if (p0_won) {
        game->state = ROUND_PLAYER0_WON;
        game->scores[0]++;
    } else if (p1_won) {
        game->state = ROUND_PLAYER1_WON;
        game->scores[1]++;
    } else if (p0_lost && p1_lost) {
        game->state = ROUND_DRAW;
    } else {
        game->state = ROUND_IN_PROGRESS;
    }

    return game->state;
}

/* Processes a guess made by a player. */
GuessResult process_guess(GameState *game, int player_id, char guess) {
    if (game == NULL || player_id < 0 || player_id >= MAX_PLAYERS) {
        return GUESS_INVALID;
    }

    if(game->state != ROUND_IN_PROGRESS) {
        return GUESS_INVALID;
    }

    guess = normalize_guess(guess);
    if (guess == '\0') {
        return GUESS_INVALID;
    }

    PlayerGameState *player = &game->players[player_id];
    int index = guess - 'a';

    if (player->guessed[index]) {
        return GUESS_REPEAT;
    }

    player->guessed[index] = 1;
    append_guessed_letters(player, guess);

    if (secret_contains(game->secret_word, guess)) {
        reveal_letters(game->secret_word, player->masked_word, guess);

        if (player_has_won(player)) {
            player->finished = 1;
        }

        update_round_state(game);
        return GUESS_CORRECT;
    } else {
        player->lives_left--;

        if (player_has_lost(player)) {
            player->finished = 1;
        }

        update_round_state(game);
        return GUESS_WRONG;
    }
}

int match_winner(const GameState *game) {
    if (game == NULL) {
        return -1;
    }

    /* Number of round wins needed to clinch the match */
    int wins_needed = game->total_rounds / 2 + 1;

    if (game->scores[0] >= wins_needed) {
        return 0;  
    }
    if (game->scores[1] >= wins_needed) {
        return 1;
    }
    
    if (game->round_num < game->total_rounds) {
        return -1;
    }

    if (game->scores[0] > game->scores[1]) {
        return 0;
    } else if (game->scores[1] > game->scores[0]){
        return 1;
    } else {
        return 255;
    }
}