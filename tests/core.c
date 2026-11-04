#include "ttrie.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { char keys[32][256]; int distances[32]; int count; } results;
static int collect(const char *key, int distance, void *data) {
    results *result = data;
    assert(result->count < 32);
    strcpy(result->keys[result->count], key);
    result->distances[result->count++] = distance;
    return 0;
}
static int stop(const char *key, int distance, void *data) {
    (void)key; (void)distance; (void)data;
    return 42;
}
int main(void) {
    ttrie *trie = ttrie_create();
    assert(trie);
    const char *keys[] = {"xyzxyz", "tk", "tclx", "tcl", "tcl", "tcl-tk", "abc", "a+c", "a-c", "a_c", "a.c"};
    for (size_t i = 0; i < sizeof(keys) / sizeof(*keys); ++i) { assert(ttrie_insert(trie, keys[i]) == 0); }
    assert(ttrie_contains(trie, "tcl"));
    assert(!ttrie_contains(trie, "tc"));
    assert(!ttrie_contains(trie, "TCL"));
    results found = {0};
    assert(ttrie_search(trie, "tcl", 2, collect, &found) == 0);
    assert(found.count == 3);
    assert(!strcmp(found.keys[0], "tcl") && found.distances[0] == 0);
    assert(!strcmp(found.keys[1], "tclx") && found.distances[1] == 1);
    assert(!strcmp(found.keys[2], "tk") && found.distances[2] == 2);
    found.count = 0;
    assert(ttrie_search(trie, "abc", 1, collect, &found) == 0);
    assert(found.count == 5);
    assert(!strcmp(found.keys[1], "a+c") && !strcmp(found.keys[2], "a-c"));
    assert(!strcmp(found.keys[3], "a.c") && !strcmp(found.keys[4], "a_c"));
    assert(ttrie_search(trie, "tcl", 2, stop, NULL) == 42);
    assert(ttrie_search(trie, "tclz", 2, stop, NULL) == 42);
    assert(ttrie_search(trie, "", 2, collect, &found) == -1);
    assert(ttrie_search(trie, "tcl", 3, collect, &found) == -1);
    assert(ttrie_search(NULL, "tcl", 2, collect, &found) == -1);
    assert(ttrie_insert(trie, "") == -1);
    assert(ttrie_insert(trie, NULL) == -1);
    char boundary[257];
    memset(boundary, 'a', 256); boundary[256] = 0;
    assert(ttrie_insert(trie, boundary) == -1);
    boundary[255] = 0;
    assert(ttrie_insert(trie, boundary) == 0);
    boundary[254] = 'b';
    found.count = 0;
    assert(ttrie_search(trie, boundary, 2, collect, &found) == 0);
    assert(found.count == 1 && found.distances[0] == 1);
    ttrie_destroy(trie);
    ttrie_destroy(NULL);
    puts("Core search, ordering, limits and cancellation passed");
    return 0;
}
