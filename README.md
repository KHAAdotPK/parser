# Parser

A lightweight C++ library for streaming CSV corpora and building an in-memory vocabulary index for NLP preprocessing tasks.

---

## Overview

`Parser` reads a delimited text file line by line and constructs a hash table of every unique token encountered, each mapped to a linked list of every position where it appears in the corpus. The result is a `TABLES` structure that downstream NLP code can use for concordance lookup, co-occurrence analysis, frequency counting, and vocabulary enumeration — without loading the entire corpus into memory at once.

---

## Intended Use

If you are building a package that needs corpus tokenisation and vocabulary indexing, `Parser` (along with its own dependency [Hash](https://github.com/KHAAdotPK/Hash)) should be cloned into the `lib/` directory of your package.

### Optional Text Cleaning

Before tokenisation, `Parser`'s `Iterator` can pass each line through a `Cleaner`
object to strip punctuation and noise characters. This is activated by defining
`ITERATOR_USER_DEFINED_CLEANER_CODE` before including `Parser/header.hh`.

Two cleaning packages are available depending on the language of your corpus:

| Package | Language | Repository |
|---|---|---|
| [Imprint](https://github.com/KHAAdotPK/Imprint.git) | English | Unicode-aware punctuation stripping for English text |
| [Naqsh](https://github.com/KHAAdotPK/Naqsh.git) | Urdu | Unicode-aware punctuation and noise normalisation for Urdu text |

Neither package is meant to be used standalone — they are designed specifically
to plug into `Parser` via the `ITERATOR_USER_DEFINED_CLEANER_CODE` macro hook.
The chosen cleaning package must be cloned into `lib/` and included **before**
`Parser/header.hh` so that `Cleaner` is in scope when `Iterator.hh` compiles.

---

## Dependencies

`Parser` has one dependency:

- **[Hash](https://github.com/KHAAdotPK/Hash)** — provides `Keys::generate_key()` and `Keys::next_prime()`, which drive the hash function and rehash growth strategy used by `build_hash_table()`.

Both `Parser` and `Hash` must be present under the `lib/` directory of whichever package depends on them.

---

## Installation (as a dependency)

Clone both `Parser` and `Hash` into the `lib/` directory of your dependent package. Using [naqsh](https://github.com/KHAAdotPK/naqsh) as an example:

```bash
cd naqsh/lib

git clone https://github.com/KHAAdotPK/Parser.git
git clone https://github.com/KHAAdotPK/Hash.git
```

Then include the entry point header from your package code:

```cpp
#include "lib/Parser/header.hh"
```

The expected layout inside your package's `lib/` directory is:

```
lib/
├── Parser/
│   ├── header.hh
│   └── lib/src/
│       ├── WordRecord.hh
│       ├── Iterator.hh
│       └── Parser.hh
└── Hash/
    └── ...
```

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
├── n                       (total occurrences in corpus — equals length of occurrence
│                            list. Initialised to 1 on first insertion, incremented on
│                            every repeat. Enables O(1) frequency and probability
│                            queries without traversing the list.)
└── head → OccurrenceNode ⇄ OccurrenceNode ⇄ ...
              (doubly-linked list of every position in the corpus)              

OccurrenceNode
├── line                    (line number of this occurrence)
├── token                   (token position within the line, resets per line)
├── next
└── prev
```

`TABLES` carries a `ref_count` for shared-ownership tracking by the caller. It is allocated on the heap by `build_hash_table()` and ownership transfers to the caller.

For a full explanation of how these structures are built, how the bidirectional lookup works, all invariants, and the connection to the downstream Transformer embedding matrix, see **[TABLES.md](./DOCUMENTS/TABLES.md)**.

---

## Hash Table Design

The hash table uses **open addressing with linear probing and a prime bucket count**, starting at `KEYS_COMMON_STARTING_SIZE`. The hashing function is provided by `Keys::generate_key(word, bucket_count)`.

**Collision resolution** is handled by linear probing. When two distinct words hash to the same bucket, the incoming word is walked forward — `probe = (key + 1) % bucket_count` — until either an empty slot is found (new word) or a slot containing the same word is found (existing word, append occurrence). The `word_id_to_hash` index always stores the actual probe position at which a word was placed, not its natural hash key, so bidirectional lookup remains correct for displaced words.

When the load factor exceeds `KEYS_LOAD_FACTOR_THRESHOLD`, the table is grown to the next prime (via `Keys::next_prime`) and all existing entries are rehashed into the larger table using the same linear probing strategy. The `word_id_to_hash` index is updated in the same pass so the index remains consistent after every resize. Only the bucket arrays are reallocated — `WordRecord` and `OccurrenceNode` objects on the heap are never touched during rehash, and all occurrence linked lists survive intact.

Each `word_id` is assigned sequentially in order of first encounter (0, 1, 2, ...), independent of hash key or token position. This makes `word_id` a stable, dense row index directly usable as an index into a Transformer embedding matrix. Each `OccurrenceNode` records the intra-line token position (`token`) and line number (`line`) of that specific occurrence, with `token` resetting to 0 at the start of each new line.

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
#include "lib/parser/header.hh"

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

- [x] ~~Collision handling — add linear probing or chaining to eliminate silent data loss on hash collision~~ — **Implemented.** Full linear probing is in place for both insertion and rehashing. Word-string comparison guards every lookup so displaced words are always found correctly and never silently overwritten.
- [x] ~~Frequency counting — required traversing the OccurrenceNode linked list
      to count occurrences, making frequency and probability queries O(k)~~ — **Implemented.** `WordRecord::n` is incremented during `build_hash_table()` at every occurrence. Frequency and probability are now O(1) field reads.
- [ ] `TABLES` destructor — no recursive deallocation of `WordRecord` objects and `OccurrenceNode` chains; caller must walk the table to free memory
- [ ] `ref_count` enforcement — manual reference counting on `TABLES` is not encapsulated; consider wrapping in `std::shared_ptr` or adding `acquire`/`release` methods
- [ ] Remove dead commented-out member variables from `Parser` class body

---

## License

This project is governed by a license, the details of which can be located in the accompanying file named 'LICENSE.' Please refer to this file for comprehensive information.
