# Parser

A C++ library for reading a corpus file, building a vocabulary index, and materialising a compact representation of token keys per line for NLP-style preprocessing and training pipelines.

The current implementation in this repository is the flat, cache-friendly path. It is centred around the newer parser API in [lib/src/Parser.hh](lib/src/Parser.hh), the streaming iterator in [lib/src/Iterator.hh](lib/src/Iterator.hh), and the current record types in [lib/src/WordRecord.hh](lib/src/WordRecord.hh).

---

## What the current code does

The active parser flow is:

1. Open a corpus file with `Parser`.
2. Build a vocabulary hash table with `build_hash_table_very_new(size_t** index_table)`.
3. Build a flat per-line token-key table with `build_lines_table(const WordRecord_new* const *const hash_table)`.
4. Release the allocated memory with `free_tables_very_new(WordRecord_new**, WORDS**, size_t*)` when finished.

This design avoids the older linked-list-based occurrence tracking and replaces it with a more compact representation that is better suited to modern training loops.

---

## Current data model

The main runtime structures are:

- `WordRecord_new` — one record per unique token, storing:
  - `word_id`
  - `word`
  - `n` (frequency count)
- `WORDS` — one object per line, storing:
  - `n` (number of tokens on that line)
  - `keys` (a contiguous array of hash keys for the line)

The older linked-list structures such as `WordRecord`, `Line`, `Token`, `OccurrenceNode`, and `TABLES` still exist in the repository for reference, but they are not the primary implementation path anymore.

---

## Parser API overview

### Building the vocabulary

```cpp
#include "lib/Parser/header.hh"

Parser parser("corpus.txt");
size_t* index_table = nullptr;
WordRecord_new** vocab = parser.build_hash_table_very_new(&index_table);
```

This builds a hash table of unique tokens and updates the parser state:

- `bucket_count`
- `bucket_used`
- `nol` (number of lines)
- `tnt` (total number of tokens)
- `mxntpl` / `mnntpl` (token-count statistics)

The file stream is rewound at the end of the build step, so the parser can be reused.

### Building the flat line table

```cpp
WORDS** lines = parser.build_lines_table(vocab);
```

Each `WORDS` object owns a contiguous `keys` array for one line. This makes token access much more cache-friendly than the older linked-list representation.

### Cleaning up

```cpp
parser.free_tables_very_new(vocab, lines, index_table);
```

The caller is responsible for releasing the memory allocated by the build functions.

---

## Iterator behaviour

The iterator in [lib/src/Iterator.hh](lib/src/Iterator.hh) reads one line at a time from an `std::ifstream` and yields a `std::vector<std::string>` of token fields.

It supports the following optional hooks:

- `CSV_PARSER_TOKEN_DELIMITER` — custom field separator (default is `,`)
- `ITERATOR_USER_DEFINED_CLEANER_CODE` — run each line through a `Cleaner` implementation before tokenisation
- `ITERATOR_GUARD_AGAINST_EMPTY_STRING` — skip empty token fields caused by adjacent delimiters

The iterator is compatible with range-based iteration and can be used directly via `Parser::begin()` / `Parser::end()`.

---

## Hash-table design

The current implementation uses open addressing with linear probing:

- The table starts at `KEYS_COMMON_STARTING_SIZE`.
- Hashing is performed with `Keys::generate_key()` from the Hash dependency.
- The table is rehashed when the load factor exceeds `KEYS_LOAD_FACTOR_THRESHOLD` using `Keys::next_prime()`.

This keeps the vocabulary index compact while still providing constant-time average lookup for tokens seen during parsing.

---

## Build configuration

| Macro | Effect |
|---|---|
| `CSV_PARSER_TOKEN_DELIMITER` | Field separator character used by the iterator |
| `KEYS_COMMON_STARTING_SIZE` | Initial number of hash buckets |
| `KEYS_LOAD_FACTOR_THRESHOLD` | Rehash trigger threshold |
| `ITERATOR_USER_DEFINED_CLEANER_CODE` | Enable line cleaning via a `Cleaner` object |
| `ITERATOR_GUARD_AGAINST_EMPTY_STRING` | Skip empty token fields |

---

## Repository layout

- [header.hh](header.hh) — public include file and default configuration constants
- [lib/src/Iterator.hh](lib/src/Iterator.hh) — line-based token iterator
- [lib/src/Parser.hh](lib/src/Parser.hh) — main parser implementation and build functions
- [lib/src/WordRecord.hh](lib/src/WordRecord.hh) — current and legacy record structures
- [old-implementation](old-implementation) — older linked-list-based parser code retained for reference

---

## Dependencies

The parser relies on:

- [Hash](https://github.com/KHAAdotPK/Hash) — hashing helpers and prime generation
- the local Corpus package in this workspace for shared parser/corpus support types

If you are integrating this library into another project, keep the relevant `lib/` packages available alongside the parser.

---

## Notes for maintainers

- The current implementation prioritises a compact memory layout and faster access over full occurrence tracking.
- The old `TABLES`-based path remains in the repository as historical code and should be treated as legacy material unless it is explicitly reintroduced.

---

## License

This project is governed by the repository license. Please refer to the accompanying license file for the full terms.
