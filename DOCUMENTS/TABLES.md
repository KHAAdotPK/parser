# TABLES — Vocabulary Hash Table Documentation

## Overview

The `TABLES` structure is the core data structure of the tokenizer/vocabulary builder. It is built by `Parser::build_hash_table()` and provides **O(1) average-case lookup** for any token in the corpus. It is the foundation upon which token IDs are assigned, which later feed into the embedding matrix of the Transformer.

The structure solves two fundamental problems simultaneously:

1. **Uniqueness** — every distinct word in the corpus gets exactly one record, no matter how many times it appears.
2. **Occurrence tracking** — every position (line, token index) where a word appears is recorded in a linked list, pre-built at parse time so query time is just pointer chasing.

---

## Data Structures

### `WordRecord`

Represents a single unique word (token) in the vocabulary.

```
WordRecord
├── word_id     → size_t   — unique sequential ID assigned at insertion time (0, 1, 2, ...)
├── word        → string   — the actual token string (e.g. "hello", "the", "transformer")
└── head        → OccurrenceNode*  — pointer to the head of the occurrence linked list
```

`word_id` is assigned as `bucket_used` at the time of insertion. It is dense and sequential, making it a direct row index into the embedding matrix.

---

### `OccurrenceNode`

One node in the doubly-linked list of occurrences for a given word.

```
OccurrenceNode
├── line_number   → size_t          — which line in the corpus this occurrence is on
├── token_number  → size_t          — position of the token within that line
├── next          → OccurrenceNode* — next occurrence in corpus order
└── prev          → OccurrenceNode* — previous occurrence (for bidirectional traversal)
```

All occurrences of a word are chained together in corpus order. The list is built incrementally — each new occurrence is appended to the tail.

---

### `TABLES` (the composite structure)

```
TABLES
├── hash_to_word_record   → WordRecord**  — the hash table array (indexed by hash key)
├── word_id_to_hash       → size_t*       — index table (indexed by word_id → gives hash key)
├── bucket_count          → size_t        — current total number of buckets in the table
├── bucket_used           → size_t        — number of unique words inserted (= vocabulary size)
├── ref_count             → size_t        — reference count for shared ownership
├── maximum_tokens_per_line → size_t      — longest line in the corpus (in tokens)
└── minimum_tokens_per_line → size_t      — shortest line in the corpus (in tokens)
```

The two arrays give **bidirectional lookup**:

```
word string  →  generate_key()  →  hash_to_word_record[key]  →  WordRecord
word_id      →  word_id_to_hash[word_id]  →  key  →  hash_to_word_record[key]  →  WordRecord
```

---

## Memory Layout

```
hash_to_word_record (array of pointers, size = bucket_count)
┌──────────┬──────────┬──────────┬──────────┬──────────┐
│ nullptr  │ WR* "the"│ nullptr  │ WR* "of" │ WR* "a"  │  ...
└──────────┴──────────┴──────────┴──────────┴──────────┘
     0           1          2          3          4

word_id_to_hash (array of size_t, indexed by word_id)
┌──────────┬──────────┬──────────┐
│    1     │    3     │    4     │  ...
└──────────┴──────────┴──────────┘
  word_id=0  word_id=1  word_id=2
  ("the"     ("of"      ("a"
   is at      is at      is at
   bucket 1)  bucket 3)  bucket 4)

WordRecord for "the" (at hash_to_word_record[1])
  word_id = 0
  word    = "the"
  head → [line=0, tok=2] → [line=1, tok=0] → [line=1, tok=5] → nullptr
```

---

## `build_hash_table()` — Step by Step

### Phase 1: Initialisation

```cpp
size_t bucket_count = KEYS_COMMON_STARTING_SIZE;
size_t bucket_used  = 0;

hash_table  = new WordRecord*[bucket_count]();   // zero-initialised → all nullptr
index_table = new size_t[bucket_count]();        // zero-initialised
```

Both arrays are zero-initialised with `()`. This is critical — `hash_table[key] == nullptr` is the sentinel that means "this bucket is empty / this word is new". Without `()`, the check becomes undefined behaviour on uninitialised garbage.

`bucket_used` tracks the vocabulary size and doubles as the `word_id` counter. It is never reset.

---

### Phase 2: Corpus Iteration

The function iterates over every line, and within each line over every token:

```
for line in corpus:
    for token in line:
        key = generate_key(token, bucket_count)
        → case A: hash_table[key] == nullptr       (empty bucket)
        → case B: hash_table[key]->word == token   (same word, no collision)
        → case C: hash_table[key]->word != token   (collision — different word)
```

#### Case A — Empty Bucket (New Word)

The token has never been seen before.

```
1. Create OccurrenceNode(line_number, token_number, next=nullptr, prev=nullptr)
2. Create WordRecord(word_id=bucket_used, word=token, head=occurrence)
3. hash_table[key]  = word_record
4. index_table[word_record->word_id] = key
5. bucket_used++
```

The `word_id` is stamped at insertion time and never changes. It becomes the token's permanent identifier throughout the entire system.

#### Case B — Same Word (No Collision)

The token already exists in the table and maps directly to its bucket.

```
1. word_record = hash_table[key]
2. Traverse occurrence linked list from head to tail
3. Append new OccurrenceNode(line_number, token_number, next=nullptr, prev=tail)
```

No new `WordRecord` is created. The new occurrence is chained to the existing list. `bucket_used` is NOT incremented.

#### Case C — Collision (Different Word, Same Bucket)

Two distinct words hashed to the same bucket. **Linear probing** resolves this.

```
probe = (key + 1) % bucket_count

loop:
    if hash_table[probe] == nullptr:
        → New word found an empty slot
        → Create OccurrenceNode + WordRecord
        → hash_table[probe] = word_record
        → index_table[word_record->word_id] = probe   ← stores PROBE, not key
        → bucket_used++
        → break

    elif hash_table[probe]->word == token:
        → Found the actual record for this word (it was displaced earlier)
        → Traverse to tail of its occurrence list
        → Append new OccurrenceNode
        → break

    else:
        probe = (probe + 1) % bucket_count
        → Keep probing
```

**Critical detail:** when a displaced word is finally inserted at `probe`, `index_table` records `probe` — the actual location — not `key` (the original hash). Without this, `word_id → hash → WordRecord` lookups would be broken for any collided word.

---

### Phase 3: Load Factor Check and Rehashing

After every token insertion, the load factor is checked:

```cpp
if ((double)bucket_used / (double)bucket_count > KEYS_LOAD_FACTOR_THRESHOLD)
    → rehash
```

A low load factor (typically 0.5–0.7) keeps collision probability low and probing chains short.

#### Rehash Procedure

```
1. old_bucket_count = bucket_count
2. bucket_count = Keys::next_prime(bucket_count)   ← grow to next prime
3. Allocate new_hash_table[bucket_count]() and new_index_table[bucket_count]()
4. For each i in 0..old_bucket_count:
       if hash_table[i] != nullptr:
           new_key = generate_key(hash_table[i]->word, bucket_count)
           if new_hash_table[new_key] == nullptr:
               new_hash_table[new_key] = hash_table[i]
               new_index_table[word_id] = new_key
           else:
               linear probe in new table to find empty slot
               new_hash_table[probe] = hash_table[i]
               new_index_table[word_id] = probe
5. delete[] hash_table
6. delete[] index_table
7. hash_table  = new_hash_table
8. index_table = new_index_table
```

Only the **arrays** are freed. The `WordRecord` and `OccurrenceNode` objects on the heap are **not touched** — their pointers are simply copied into the new table. All occurrence linked lists remain intact across rehashes.

Prime-sized tables reduce the chance that a hash function produces systematic clustering, distributing keys more uniformly across buckets.

---

### Phase 4: Per-Line Bookkeeping

At the end of each line:

```cpp
if (token_number > mxntpl) mxntpl = token_number;
if (token_number < mnntpl) mnntpl = token_number;
token_number = 0;   // reset for next line
line_number++;
```

`token_number` is a **within-line** counter, reset per line. It records the position of a token within its line — not a global token count. This is stored in `OccurrenceNode.token_number`.

`line_number` is a global counter, never reset during the build.

---

### Phase 5: Return

A `TABLES` composite is allocated on the heap and all fields are populated:

```cpp
tables->hash_to_word_record     = hash_table;
tables->word_id_to_hash         = index_table;
tables->bucket_count            = bucket_count;
tables->bucket_used             = bucket_used;    // = vocabulary size
tables->ref_count               = 1;
tables->maximum_tokens_per_line = mxntpl;
tables->minimum_tokens_per_line = mnntpl;
```

Ownership of the two arrays transfers to `TABLES`. The caller is responsible for the lifetime of this object.

---

## Invariants

These invariants must hold at all times for the structure to be correct:

| Invariant | Reason |
|---|---|
| `bucket_used < bucket_count` | Enforced by load factor. Guarantees at least one empty slot exists for probing |
| `index_table[word_id]` always points to the actual bucket of that word | Maintained by storing `probe` (not `key`) on collision |
| `word_id` values are dense in `[0, bucket_used)` | Enables direct use as embedding matrix row indices |
| `hash_table` is zero-initialised on every allocation | `nullptr` check is the only sentinel for empty buckets |
| Occurrence lists are in corpus order | New occurrences are always appended to the tail |
| `WordRecord` and `OccurrenceNode` are never freed during rehash | Only the bucket array is replaced; heap objects survive |

---

## Complexity

| Operation | Average Case | Worst Case |
|---|---|---|
| Token lookup / insert | O(1) | O(n) — full probe sequence |
| Occurrence append | O(k) where k = occurrences so far | O(k) |
| Rehash | O(m) where m = old bucket count | O(m) |
| Lookup by `word_id` | O(1) | O(1) |

Rehash is amortised O(1) per insertion over the lifetime of the build, assuming the load factor threshold keeps rehash frequency logarithmic in the vocabulary size.

---

## Connection to the Transformer

The `word_id` assigned to each `WordRecord` is the token's index into the **embedding matrix** `E` of shape `[vocab_size × d_model]`:

```
embedding(token) = E[word_id]   ← row lookup, O(1)
```

`bucket_used` at the end of the build equals `vocab_size`.

The occurrence linked lists are available for:
- **Frequency analysis** — count nodes in the list
- **Concordance / co-occurrence** — traverse neighbouring occurrences
- **Positional statistics** — `line_number` and `token_number` give exact position

At inference/training time, the hash table gives the mapping `word string → word_id` in O(1), and the embedding matrix gives `word_id → vector` in O(1). The corpus is thus fully pre-processed before any matrix operation begins.
