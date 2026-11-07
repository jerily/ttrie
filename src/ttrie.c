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

static const unsigned char snapshot_header[8] = {'T', 'T', 'R', 'I', 'E', 0, 1, 0};

static int count_nodes(const node *current, size_t *length) {
    for (; current; current = current->next) {
        if (*length > SIZE_MAX - 3) { return -1; }
        *length += 3;
        if (count_nodes(current->child, length)) { return -1; }
    }
    return 0;
}

static unsigned char *write_nodes(const node *current, unsigned char depth, unsigned char *out) {
    for (; current; current = current->next) {
        *out++ = depth;
        *out++ = current->byte;
        *out++ = current->terminal;
        out = write_nodes(current->child, (unsigned char)(depth + 1), out);
    }
    return out;
}

unsigned char *ttrie_serialize(const ttrie *trie, size_t *length) {
    if (!length) { return NULL; }
    *length = 0;
    size_t size = 12;
    if (!trie || count_nodes(trie->root.child, &size) || (size - 12) / 3 > UINT32_MAX) { return NULL; }
    unsigned char *bytes = malloc(size);
    if (!bytes) { return NULL; }
    memcpy(bytes, snapshot_header, sizeof(snapshot_header));
    uint32_t count = (uint32_t)((size - 12) / 3);
    for (int i = 11; i >= 8; --i) { bytes[i] = (unsigned char)count; count >>= 8; }
    write_nodes(trie->root.child, 1, bytes + 12);
    *length = size;
    return bytes;
}

ttrie *ttrie_load(const void *data, size_t length) {
    const unsigned char *bytes = data;
    if (!bytes || length < 12 ||
        memcmp(bytes, snapshot_header, sizeof(snapshot_header)) ||
        (length - 12) % 3) { return NULL; }
    uint32_t count = ((uint32_t)bytes[8] << 24) | ((uint32_t)bytes[9] << 16) |
                     ((uint32_t)bytes[10] << 8) | bytes[11];
    if ((length - 12) / 3 != count) { return NULL; }
    ttrie *trie = ttrie_create();
    if (!trie) { return NULL; }
    node *parents[TTRIE_MAX_LENGTH + 1] = {&trie->root};
    node *last[TTRIE_MAX_LENGTH + 1] = {0};
    unsigned int previous_depth = 0;
    for (size_t offset = 12; offset < length; offset += 3) {
        unsigned int depth = bytes[offset];
        unsigned char byte = bytes[offset + 1], terminal = bytes[offset + 2];
        if (!depth || depth > previous_depth + 1 || !byte || terminal > 1 ||
            (last[depth - 1] && last[depth - 1]->byte >= byte)) {
            ttrie_destroy(trie);
            return NULL;
        }
        node *created = calloc(1, sizeof(node));
        if (!created) { ttrie_destroy(trie); return NULL; }
        created->byte = byte;
        created->terminal = terminal;
        if (last[depth - 1]) { last[depth - 1]->next = created; }
        else { parents[depth - 1]->child = created; }
        last[depth - 1] = created;
        parents[depth] = created;
        last[depth] = NULL;
        previous_depth = depth;
    }
    return trie;
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
