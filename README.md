# Parser

A C++ library for reading a corpus and building a vocabulary index for NLP-style preprocessing tasks.

The codebase is currently in a transition phase: the newer implementation uses a flatter, more cache-friendly layout, while older linked-list based code still remains in the repository for reference and gradual replacement.

---

## Current status

The main implementation in [lib/Parser/lib/src/Parser.hh](lib/Parser/lib/src/Parser.hh) now follows a newer path built around:

- `WordRecord_new` for vocabulary entries
- `WORDS` for per-line token-key storage
- `build_hash_table_very_new()` for vocabulary construction
- `build_lines_table()` for building the flat line-token table
- `free_tables_very_new()` for cleanup

This newer design is intended to reduce heap fragmentation and avoid the overhead of per-occurrence linked-list nodes. It is the path currently being exercised by the parser code in this repository.

## Important note about legacy code

This repository still contains older parser code that is being phased out. That includes:

- older linked-list based structures such as `WordRecord`, `OccurrenceNode`, and `TABLES`
- older implementation paths in the `old-implementation` directories
- documentation and comments that still describe the earlier architecture

That legacy material is still present for reference, comparison, and compatibility during the migration. It should not be treated as the primary implementation going forward.

---

## Intended use

If you are building a package that needs corpus tokenisation and vocabulary indexing, `Parser` (along with its dependency [Hash](https://github.com/KHAAdotPK/Hash)) should be placed in the `lib/` directory of your package.

### Optional text cleaning

Before tokenisation, `Parser`'s `Iterator` can pass each line through a `Cleaner` object to strip punctuation and noise characters. This is enabled by defining `ITERATOR_USER_DEFINED_CLEANER_CODE` before including `Parser/header.hh`.

Two cleaning packages are available depending on the language of the corpus:

| Package | Language | Repository |
|---|---|---|
| [Imprint](https://github.com/KHAAdotPK/Imprint.git) | English | Unicode-aware punctuation stripping for English text |
| [Naqsh](https://github.com/KHAAdotPK/Naqsh.git) | Urdu | Unicode-aware punctuation and noise normalisation for Urdu text |

These packages are intended to plug into `Parser` via the `ITERATOR_USER_DEFINED_CLEANER_CODE` macro hook.

---

## Dependencies

`Parser` has one direct dependency:

- **[Hash](https://github.com/KHAAdotPK/Hash)** — provides `Keys::generate_key()` and `Keys::next_prime()`, which drive the hashing and rehash strategy used by the parser.

Both `Parser` and `Hash` should be present under the `lib/` directory of the package that depends on them.

---

## Architecture

The library is built around three cooperating pieces:

### `Iterator` — streaming tokeniser

A C++ input iterator that wraps an `std::ifstream` and yields one line at a time as a `std::vector<std::string>` of token fields. It splits on `CSV_PARSER_TOKEN_DELIMITER` and is compatible with range-based iteration.

Optional behaviour can be enabled through macros:

- `ITERATOR_USER_DEFINED_CLEANER_CODE` — normalises each line through a `Cleaner` object before tokenisation
- `ITERATOR_GUARD_AGAINST_EMPTY_STRING` — skips empty token fields caused by adjacent delimiters

### `Parser` — corpus reader and index builder

The parser owns the file stream and exposes `begin()`/`end()` so it can be iterated directly. The newer implementation builds a vocabulary table and a flat line-table through the following methods:

```cpp
WordRecord_new** vocab = parser.build_hash_table_very_new();
WORDS** lines = parser.build_lines_table(vocab);
```

The file is rewound after the build step so the parser can be reused if needed.

### Newer data model

The newer path uses a compact representation:

```text
hash_table[i] -> WordRecord_new
lines_array[i] -> WORDS
```

Where:

- each `WordRecord_new` stores the token string, a stable word id, and the frequency count
- each `WORDS` object owns a contiguous array of hash keys for one line

This is the preferred direction for the current implementation.

### Legacy data model

The older design used a linked-list based vocabulary structure built around `TABLES`, `WordRecord`, and `OccurrenceNode`. That path remains in the repository as historical code and is expected to be replaced by the newer flat-array implementation.

---

## Hash table design

The hash table uses open addressing with linear probing and a prime bucket count, starting from `KEYS_COMMON_STARTING_SIZE`. The hash function is provided by `Keys::generate_key(word, bucket_count)`.

When a collision occurs, the parser probes forward until it finds either:

- an empty slot for a new token, or
- an existing slot containing the same token

The table is rehashed when the load factor exceeds `KEYS_LOAD_FACTOR_THRESHOLD`, using `Keys::next_prime()` to enlarge the bucket count.

---

## Build configuration

| Macro | Effect |
|---|---|
| `CSV_PARSER_TOKEN_DELIMITER` | Field separator character (for example `','`) |
| `KEYS_COMMON_STARTING_SIZE` | Initial bucket count (recommended: a prime, for example `1009`) |
| `KEYS_LOAD_FACTOR_THRESHOLD` | Rehash trigger ratio (for example `0.7`) |
| `ITERATOR_USER_DEFINED_CLEANER_CODE` | Enable line-cleaning via a `Cleaner` class |
| `ITERATOR_GUARD_AGAINST_EMPTY_STRING` | Skip empty token fields |

---

## Usage

```cpp
#include "lib/Parser/header.hh"

Parser parser("corpus.csv");

WordRecord_new** vocab = parser.build_hash_table_very_new();
WORDS** lines = parser.build_lines_table(vocab);

// vocab[key] points to the vocabulary entry for that hash bucket
// lines[i] contains the flattened token-key sequence for line i
```

The caller is responsible for releasing the memory allocated for the vocabulary and line tables when they are no longer needed.

---

## Roadmap

- [x] Move the main parsing path toward the newer flat-table representation
- [ ] Consolidate the public API around the new naming and ownership model
- [ ] Remove or isolate the remaining legacy `TABLES`-based code paths
- [ ] Clean up old comments, dead code, and transitional documentation
- [ ] Add clearer ownership helpers and safer cleanup patterns

---

## License

This project is governed by a license, the details of which can be located in the accompanying file named `LICENSE`. Please refer to that file for comprehensive information.
