#include "ttrie.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct node {
    unsigned char byte;
    bool terminal;
    struct node *child;
    struct node *next;
} node;

struct ttrie { node root; };

static size_t key_length(const char *key) {
    if (!key) { return 0; }
    for (size_t i = 0; i <= TTRIE_MAX_LENGTH; ++i) {
        if (!key[i]) { return i; }
    }
    return 0;
}

ttrie *ttrie_create(void) {
    return calloc(1, sizeof(ttrie));
}

static void destroy_nodes(node *current) {
    while (current) {
        node *next = current->next;
        destroy_nodes(current->child);
        free(current);
        current = next;
    }
}

void ttrie_destroy(ttrie *trie) {
    if (trie) {
        destroy_nodes(trie->root.child);
        free(trie);
    }
}

int ttrie_insert(ttrie *trie, const char *key) {
    if (!trie || !key_length(key)) { return -1; }
    node *parent = &trie->root;
    for (const unsigned char *p = (const unsigned char *)key; *p; ++p) {
        node **link = &parent->child;
        while (*link && (*link)->byte < *p) { link = &(*link)->next; }
        if (!*link || (*link)->byte != *p) {
            node *created = calloc(1, sizeof(node));
            if (!created) { return -1; }
            created->byte = *p;
            created->next = *link;
            *link = created;
        }
        parent = *link;
    }
    parent->terminal = true;
    return 0;
}

int ttrie_contains(const ttrie *trie, const char *key) {
    if (!trie || !key_length(key)) { return 0; }
    const node *parent = &trie->root;
    for (const unsigned char *p = (const unsigned char *)key; *p; ++p) {
        const node *child = parent->child;
        while (child && child->byte < *p) { child = child->next; }
        if (!child || child->byte != *p) { return 0; }
        parent = child;
    }
    return parent->terminal;
}

typedef struct {
    const unsigned char *query;
    size_t length;
    int distance;
    ttrie_visitor visitor;
    void *context;
    char key[TTRIE_MAX_LENGTH + 1];
} search_state;

static uint16_t minimum(uint16_t a, uint16_t b) { return a < b ? a : b; }

static int visit_matches(const node *current, const uint16_t *previous,
                         size_t depth, search_state *state) {
    uint16_t row[TTRIE_MAX_LENGTH + 1];
    row[0] = (uint16_t)depth;
    uint16_t best = row[0];
    for (size_t i = 1; i <= state->length; ++i) {
        row[i] = minimum(minimum(row[i - 1] + 1, previous[i] + 1),
                         previous[i - 1] + (current->byte != state->query[i - 1]));
        best = minimum(best, row[i]);
    }
    state->key[depth - 1] = (char)current->byte;
    if (current->terminal && row[state->length] == state->distance) {
        state->key[depth] = '\0';
        int status = state->visitor(state->key, state->distance, state->context);
        if (status) { return status; }
    }
    // A prefix whose entire row exceeds the threshold cannot produce a match.
    if (best <= state->distance) {
        for (const node *child = current->child; child; child = child->next) {
            int status = visit_matches(child, row, depth + 1, state);
            if (status) { return status; }
        }
    }
    return 0;
}

int ttrie_search(const ttrie *trie, const char *query, int max_distance,
                 ttrie_visitor visitor, void *context) {
    size_t length = key_length(query);
    if (!trie || !length || !visitor || max_distance < 0 || max_distance > TTRIE_MAX_DISTANCE) {
        return -1;
    }
    if (ttrie_contains(trie, query)) {
        int status = visitor(query, 0, context);
        if (status) { return status; }
    }
    uint16_t initial[TTRIE_MAX_LENGTH + 1];
    for (size_t i = 0; i <= length; ++i) { initial[i] = (uint16_t)i; }
    search_state state = {(const unsigned char *)query, length, 0, visitor, context, {0}};
    // At most two pruned traversals keep results ordered without storing matches.
    for (state.distance = 1; state.distance <= max_distance; ++state.distance) {
        for (const node *child = trie->root.child; child; child = child->next) {
            int status = visit_matches(child, initial, 1, &state);
            if (status) { return status; }
        }
    }
    return 0;
}
