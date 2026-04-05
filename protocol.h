/*
 * protocol.h — Shared message protocol for Hangman Race
 *
 * Wire format:
 *   Every message is a fixed-size struct prefixed by a 1-byte opcode.
 *   The receiver reads 1 byte to determine the message type, then
 *   reads sizeof(payload_struct) bytes for the payload.
 *
 *   +--------+-----------------------------+
 *   | opcode |        payload bytes        |
 *   | 1 byte |   sizeof(struct) bytes      |
 *   +--------+-----------------------------+
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/* Game constants */
#define MAX_WORD_LEN      32   /* max secret word length (excluding '\0') */
#define MAX_NAME_LEN      20   /* max player display name */
#define MAX_LIVES          6   /* wrong guesses before elimination */
#define MAX_PLAYERS        2   /* two-player race */
#define DEFAULT_PORT    4242
#define NUM_CATEGORIES     3   /* easy, medium, hard  */
#define MAX_ROUNDS         5   /* max rounds in a best-of-N series */
#define ALPHABET_SIZE     26

/* Message opcodes   (1 byte each, sent before every payload) */

/* Client → Server */
#define MSG_JOIN        0x01   /* player wants to join the lobby */
#define MSG_GUESS       0x02   /* player submits a letter guess */
#define MSG_REMATCH     0x03   /* player votes on playing again */

/* Server → Client */
#define MSG_WELCOME     0x10   /* lobby: you joined, wait for opponent */
#define MSG_GAME_START  0x11   /* game begins: round info + masked word */
#define MSG_GUESS_RESULT 0x12  /* result of YOUR guess */
#define MSG_OPPONENT    0x13   /* what the opponent just guessed */
#define MSG_ROUND_OVER  0x14   /* round ended: winner announcement */
#define MSG_MATCH_OVER  0x15   /* series ended: final scores */
#define MSG_ERROR       0x1F   /* server-side error / invalid action */

/* Payload structs   (all fixed-width, packed for simplicity) */

/* Client → Server */

/* MSG_JOIN: client sends its player name */
typedef struct {
    char name[MAX_NAME_LEN + 1];   /* null-terminated player name */
} msg_join_t;

/* MSG_GUESS: client sends a single lowercase letter */
typedef struct {
    char letter;                   /* 'a' - 'z' */
} msg_guess_t;

/* MSG_REMATCH: client votes yes or no */
typedef struct {
    uint8_t accept;                /* 1 = yes, 0 = no */
} msg_rematch_t;

/* Server → Client */

/* MSG_WELCOME: confirms join, tells client its player index */
typedef struct {
    uint8_t player_id;             /* 0 or 1 */
    char message[64];              /* e.g. "Waiting for opponent..." */
} msg_welcome_t;

/* MSG_GAME_START: new round begins */
typedef struct {
    uint8_t round_num;             /* current round (1-based) */
    uint8_t total_rounds;          /* total rounds in the series */
    uint8_t category;              /* 0=easy, 1=medium, 2=hard */
    uint8_t word_len;              /* length of the secret word */
    char masked_word[MAX_WORD_LEN + 1];  /* e.g. "_ _ _ _ _" or underscores */
    char opponent_name[MAX_NAME_LEN + 1];
} msg_game_start_t;

/* MSG_GUESS_RESULT: server tells you whether your guess was right */
typedef struct {
    char letter;                   /* the letter you guessed */
    uint8_t correct;               /* 1 = hit, 0 = miss */
    uint8_t lives_left;            /* your remaining lives */
    char masked_word[MAX_WORD_LEN + 1]; /* updated revealed word */
    char guessed_letters[ALPHABET_SIZE + 1]; /* all letters you've tried */
} msg_guess_result_t;

/* MSG_OPPONENT: server tells you what the opponent guessed */
typedef struct {
    char letter;                   /* opponent's guessed letter */
    uint8_t correct;               /* 1 = hit, 0 = miss */
    uint8_t opp_lives_left;        /* opponent's remaining lives */
    char opp_masked_word[MAX_WORD_LEN + 1]; /* opponent's revealed word */
} msg_opponent_t;

/* MSG_ROUND_OVER: a round has ended */
typedef struct {
    uint8_t winner_id;             /* 0, 1, or 255 if draw */
    char word[MAX_WORD_LEN + 1];   /* the secret word revealed */
    uint8_t scores[MAX_PLAYERS];   /* running scores: scores[0], scores[1] */
} msg_round_over_t;

/* MSG_MATCH_OVER: the full series has ended */
typedef struct {
    uint8_t winner_id;             /* overall winner (0, 1, or 255=draw) */
    uint8_t final_scores[MAX_PLAYERS];
    char message[64];              /* e.g. "Player A wins the series 3-1!" */
} msg_match_over_t;

/* MSG_ERROR: server reports an error */
typedef struct {
    char message[64];              /* human-readable error string */
} msg_error_t;

/* 
 * Helper: message size lookup
 * Given an opcode, return the size of its payload struct.
 * Returns 0 for unknown opcodes.
 */
static inline size_t payload_size(uint8_t opcode) {
    switch (opcode) {
        case MSG_JOIN:         return sizeof(msg_join_t);
        case MSG_GUESS:        return sizeof(msg_guess_t);
        case MSG_REMATCH:      return sizeof(msg_rematch_t);
        case MSG_WELCOME:      return sizeof(msg_welcome_t);
        case MSG_GAME_START:   return sizeof(msg_game_start_t);
        case MSG_GUESS_RESULT: return sizeof(msg_guess_result_t);
        case MSG_OPPONENT:     return sizeof(msg_opponent_t);
        case MSG_ROUND_OVER:   return sizeof(msg_round_over_t);
        case MSG_MATCH_OVER:   return sizeof(msg_match_over_t);
        case MSG_ERROR:        return sizeof(msg_error_t);
        default:               return 0;
    }
}

#endif /* PROTOCOL_H */
