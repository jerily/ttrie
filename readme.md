# ttrie

String trie with exact lookup and fuzzy search for C and Tcl.

## Rules

- Return all matches within Levenshtein distance 0–2, sorted by distance then byte order.
- Preserve case and punctuation. Distance counts byte insertions, deletions and substitutions; transpositions cost two edits.
- Accept nonempty keys and queries up to 255 bytes. Duplicate inserts are idempotent.
- Keep application policy and registry access in callers. The C library has no Tcl dependency.

## Tcl

```tcl
package require ttrie 0.1.1
set trie [::ttrie::create names {tcl tclx tk}]
$trie search tcl
# {name tcl distance 0} {name tclx distance 1} {name tk distance 2}
$trie search tcl 0       ;# exact only
$trie insert tjson
$trie contains tjson    ;# 1
$trie destroy
```

`create command_name ?keys?` returns the fully qualified command name and rejects
existing commands. `search query ?max_distance?` defaults to 2 and returns a list
of dictionaries; no matches returns an empty list. Commands own their trie and
release it on deletion or interpreter teardown. Tcl keys use UTF-8 byte distance,
not Unicode character distance, and cannot contain NUL.

## Build

Requires a C11 compiler, CMake 3.18+, and Tcl 8.6 or 9.x development files for the
optional extension. Build against the Tcl major version used at runtime.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build --prefix /your/prefix
```

Shared builds define `USE_TCL_STUBS`, link only the Tcl stub archive, and call
`Tcl_InitStubs` for Tcl 8.6 or 9.0 according to the headers' major version. They
do not require or link the full Tcl runtime library. Static wrappers use the
embedding application's Tcl runtime, as in tjson.

Use `-DTCL_INCLUDE_PATH=...`, `-DTCL_STUB_LIBRARY=...` and `-DTCL_TCLSH=...` to
select a Tcl installation, or use `TCL_INCLUDE_DIR`/`TCL_LIBRARY_DIR` as search
hints. `TCL_LIBRARY` is needed only for static embedding tests.
`-DTTRIE_BUILD_TCL=OFF` builds only
the static C library; `-DBUILD_SHARED_LIBS=OFF` also makes the Tcl wrapper static
for embedding. Register `Ttrie_Init` with `Tcl_StaticLibrary` (Tcl 9) or
`Tcl_StaticPackage` (Tcl 8.6), under `Ttrie`, before loading a static package.
Link both `libttrie.a` and `libttrie_core.a` when embedding the wrapper.

## C API

```cmake
find_package(ttrie 0.1 CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE ttrie::core)
```

Include `ttrie.h`. Create/destroy a trie with `ttrie_create`/`ttrie_destroy`, add
keys with `ttrie_insert`, and look up keys with `ttrie_contains`. `ttrie_search`
calls a visitor for each matching name and distance; a nonzero visitor return
stops the search and is propagated. Keys passed to visitors are borrowed only
for that call. Insert/search return `-1` for invalid input; insert also returns
`-1` on allocation failure. Creation returns NULL on allocation failure.

Children remain in byte order. Fuzzy lookup carries a dynamic-programming row
along each trie branch and prunes prefixes outside the distance bound. Up to two
pruned traversals produce ordered results without allocating a match array.
Concurrent readers may share an immutable C trie; mutations and destruction
require caller synchronization. Tcl commands belong to their interpreter.

## Security

### Surface
C string inputs and Tcl commands; no filesystem, network or script evaluation.

### Protections
Key length and distance limits bound recursion and per-frame memory. Callers
must not mutate or destroy a trie from its search visitor. Tcl initialization
validates all initial keys before installing the command.

### Pending
Callers set index-size and request-rate limits for their own workloads.
