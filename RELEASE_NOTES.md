# Release notes

## 0.2.0 — 2026-11-07

- Add portable binary snapshots and direct C/Tcl loading of built indexes.
- Validate snapshot structure before installing commands.

## 0.1.1 — 2026-11-04

- Build shared extensions with Tcl headers and the stub archive alone.
- Match Tcl 8.6/9.0 stub initialization and add a check for direct Tcl symbol references.
- Support Tcl include/library directory hints; keep full-runtime linking in embedding tests only.

## 0.1.0 — 2026-11-04

- Add a static C string trie and a Tcl 8.6/9.x extension.
- Find exact names and every match within edit distance 2, ordered by distance and name.
- Preserve case and punctuation, deduplicate inserts, and prune irrelevant trie branches.
- Provide CMake package exports, shared/static Tcl builds, and C/Tcl tests.
