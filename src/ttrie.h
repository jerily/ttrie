#ifndef TTRIE_H
#define TTRIE_H

#ifdef __cplusplus
extern "C" {
#endif

#define TTRIE_MAX_LENGTH 255
#define TTRIE_MAX_DISTANCE 2

typedef struct ttrie ttrie;
/* The key is borrowed only for the callback. Do not mutate the trie here. */
typedef int (*ttrie_visitor)(const char *key, int distance, void *context);

ttrie *ttrie_create(void);
void ttrie_destroy(ttrie *trie);
/* Nonempty, NUL-terminated byte strings of at most TTRIE_MAX_LENGTH bytes. */
int ttrie_insert(ttrie *trie, const char *key);
int ttrie_contains(const ttrie *trie, const char *key);
/* All matches, ordered by distance then unsigned-byte lexical order.
 * Returns 0 on success, -1 for invalid input, or the visitor's nonzero result.
 * Distances are Levenshtein (insert/delete/substitute), from 0 through 2. */
int ttrie_search(const ttrie *trie, const char *query, int max_distance,
                 ttrie_visitor visitor, void *context);

#ifdef __cplusplus
}
#endif
#endif
