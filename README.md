# Parser

A lightweight C++ library for streaming CSV corpora and building an in-memory vocabulary index for NLP preprocessing tasks.

---

## Overview

`Parser` reads a delimited text file line by line and constructs a hash table of every unique token encountered, each mapped to a linked list of every position where it appears in the corpus. The result is a `TABLES` structure that downstream NLP code can use for concordance lookup, co-occurrence analysis, frequency counting, and vocabulary enumeration — without loading the entire corpus into memory at once.

---

## Architecture

The library is built around three cooperating components.

### `Iterator` — Streaming CSV Tokeniser

A standard-conforming C++ input iterator (`std::input_iterator_tag`) that wraps a `std::ifstream` and yields one line at a time as a `std::vector<std::string>` of token fields. It splits on `CSV_PARSER_TOKEN_DELIMITER` (configurable at compile time) and is compatible with range-for loops.

Optional compile-time behaviour is gated behind macros:

- `ITERATOR_USER_DEFINED_CLEANER_CODE` — plugs in a `Cleaner` object to normalise each line (strip punctuation, collapse whitespace) before tokenisation.
- `ITERATOR_GUARD_AGAINST_EMPTY_STRING` — skips empty fields produced by adjacent delimiters.

### `Parser` — Corpus Reader and Index Builder

Owns the file stream and exposes `begin()`/`end()` so it can be iterated directly. Its primary method is:

```cpp
TABLES* build_hash_table();
```

This drives the iterator across the entire corpus and populates the vocabulary index. After building, the file is rewound so the parser can be iterated again if needed.

`Parser` is non-copyable (file stream semantics) but move-constructible.

### `WordRecord` / `OccurrenceNode` / `TABLES` — Vocabulary Index

```
TABLES
├── hash_to_word_record[]   (hash table: bucket → WordRecord*)
└── word_id_to_hash[]       (index table: word_id → current bucket key)

WordRecord
├── word_id                 (unique integer, assigned in order of first encounter)
├── word                    (the token string)
└── head → OccurrenceNode ⇄ OccurrenceNode ⇄ ...
              (doubly-linked list of every position in the corpus)

OccurrenceNode
├── line                    (line number of this occurrence)
├── token                   (token position within the line, resets per line)
├── next
└── prev
```

`TABLES` carries a `ref_count` for shared-ownership tracking by the caller. It is allocated on the heap by `build_hash_table()` and ownership transfers to the caller.

---

## Hash Table Design

The hash table uses **open addressing with a prime bucket count**, starting at 1009 (`KEYS_COMMON_STARTING_SIZE`). The hashing function is provided by `Keys::generate_key(word, bucket_count)`.

When the load factor exceeds `KEYS_LOAD_FACTOR_THRESHOLD`, the table is grown to the next prime (via `Keys::next_prime`) and all existing entries are rehashed into the larger table. The `word_id_to_hash` index is updated in the same pass — keyed by `word_id` — so the index remains consistent after every resize.

Each `word_id` is assigned sequentially in order of first encounter (0, 1, 2, ...), independent of hash key or token position. Each `OccurrenceNode` records the intra-line token position (`token`) and line number (`line`) of that specific occurrence, with `token` resetting to 0 at the start of each new line.

> **Known limitation:** collision handling is not yet implemented. Two distinct words that hash to the same bucket will cause the second to silently overwrite the first. For controlled or small vocabularies this is unlikely to occur; for large open-ended corpora, linear probing or bucket chaining should be added.

---

## Build Configuration

| Macro | Effect |
|---|---|
| `CSV_PARSER_TOKEN_DELIMITER` | Field separator character (e.g. `','`) |
| `KEYS_COMMON_STARTING_SIZE` | Initial bucket count (recommended: prime, e.g. `1009`) |
| `KEYS_LOAD_FACTOR_THRESHOLD` | Rehash trigger ratio (e.g. `0.7`) |
| `ITERATOR_USER_DEFINED_CLEANER_CODE` | Enable line-cleaning via a `Cleaner` class |
| `ITERATOR_GUARD_AGAINST_EMPTY_STRING` | Skip empty token fields |

---

## Usage

```cpp
#include "lib/src/WordRecord.hh"
#include "lib/src/Iterator.hh"
#include "lib/src/Parser.hh"

Parser parser("corpus.csv");

if (!parser.is_open())
{
    // handle error
}

TABLES* tables = parser.build_hash_table();

// tables->hash_to_word_record  — look up a word by its hash key
// tables->word_id_to_hash      — look up a word's current bucket by word_id
// tables->bucket_used          — vocabulary size (number of unique tokens)

// Caller is responsible for deallocating tables and its contents
```

---

## Known Issues and Roadmap

- [ ] Collision handling — add linear probing or chaining to eliminate silent data loss on hash collision
- [ ] `TABLES` destructor — no recursive deallocation of `WordRecord` objects and `OccurrenceNode` chains; caller must walk the table to free memory
- [ ] `ref_count` enforcement — manual reference counting on `TABLES` is not encapsulated; consider wrapping in `std::shared_ptr` or adding `acquire`/`release` methods
- [ ] Remove dead commented-out member variables from `Parser` class body

---

## License

This project is governed by a license, the details of which can be located in the accompanying file named 'LICENSE.' Please refer to this file for comprehensive information.

