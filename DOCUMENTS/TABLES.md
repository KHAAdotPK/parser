# TABLES — Vocabulary Hash Table Documentation

## Overview

The `TABLES` structure is the core data structure of the tokenizer/vocabulary builder. It is built by `Parser::build_hash_table()` and provides **O(1) average-case lookup** for any token in the corpus. It is the foundation upon which token IDs are assigned, which later feed into the embedding matrix of the Transformer.

The structure solves three fundamental problems simultaneously:

1. **Uniqueness** — every distinct word in the corpus gets exactly one record, no matter how many times it appears.
2. **Occurrence tracking** — every position (line, token index) where a word appears is recorded in a linked list, pre-built at parse time so query time is just pointer chasing.
3. **Corpus layout** — the full sequential structure of the corpus (every line, every token in order) is preserved in a parallel linked list, enabling O(n) traversal without re-reading the file.

---

## Data Structures

### `WordRecord`

Represents a single unique word (token) in the vocabulary.

```
WordRecord
├── word_id                 (unique integer, assigned in order of first encounter)
├── word                    (the token string)
├── n                       (total occurrences in corpus — equals length of occurrence
│                            list. Initialised to 1 on first insertion, incremented on
│                            every repeat. Enables O(1) frequency and probability
│                            queries without traversing the list.)
└── head → OccurrenceNode ⇄ OccurrenceNode ⇄ ...
              (doubly-linked list of every position in the corpus)
```

`word_id` is assigned as `bucket_used` at the time of insertion. It is dense and sequential, making it a direct row index into the embedding matrix.

---

### `OccurrenceNode`

One node in the doubly-linked list of occurrences for a given word.

```
OccurrenceNode
├── line   → size_t          — which line in the corpus this occurrence is on (0-based, never reset)
├── token  → size_t          — position of the token within that line (0-based, reset per line)
├── next   → OccurrenceNode* — next occurrence in corpus order
└── prev   → OccurrenceNode* — previous occurrence (for bidirectional traversal)
```

> **Note:** the fields are named `line` and `token` in the source (not `line_number` / `token_number`).

All occurrences of a word are chained together in corpus order. The list is built incrementally — each new occurrence is appended to the tail.

---

### `Token`

One node in the doubly-linked list of tokens within a single line. Represents a specific occurrence of a word at a specific position in the corpus.

```
Token
├── token_id   → size_t          — word_id of this token (permanent index into embedding matrix)
├── occurrence → OccurrenceNode* — pointer to this token's specific occurrence record
├── next       → Token*          — next token in this line
└── prev       → Token*          — previous token in this line
```

`token_id` is the same value as `WordRecord::word_id` for that word — it is the bridge between the sequential corpus layout and the hash table.

`occurrence` points directly into the occurrence linked list of the corresponding `WordRecord`, to the exact node that records this token's `(line, token)` position.

---

### `Line`

One node in the doubly-linked list of lines in the corpus.

```
Line
├── n      → size_t  — number of tokens in this line
├── tokens → Token*  — head of the token linked list for this line
├── next   → Line*   — next line in corpus order
└── prev   → Line*   — previous line
```

Lines are created in corpus order and linked tail-to-tail as the corpus is iterated. `n` is incremented exactly once per token at the top of the token loop — never inside any branch.

---

### `TABLES` (the composite structure)

```
TABLES
├── hash_to_word_record     → WordRecord** — the hash table array (indexed by hash key)
├── word_id_to_hash         → size_t*      — index table (indexed by word_id → gives hash key)
├── lines                   → Line*        — head of the corpus line linked list
├── bucket_count            → size_t       — current total number of buckets in the table
├── bucket_used             → size_t       — number of unique words inserted (= vocabulary size)
├── ref_count               → size_t       — reference count for shared ownership
├── maximum_tokens_per_line → size_t       — longest line in the corpus (in tokens)
├── minimum_tokens_per_line → size_t       — shortest line in the corpus (in tokens)
└── total_tokens            → size_t       — total token count across entire corpus (with repetition)
```

The two arrays give **bidirectional lookup** into the vocabulary:

```
word string  →  generate_key()              →  hash_to_word_record[key]           →  WordRecord
word_id      →  word_id_to_hash[word_id]   →  key  →  hash_to_word_record[key]   →  WordRecord
```

The `lines` pointer gives **sequential corpus traversal**:

```
lines → Line → Token → token_id   (= word_id, row index into embedding matrix)
                     → occurrence  (= exact position: line, token within line)
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
  head → [line=0, token=2] → [line=1, token=0] → [line=1, token=5] → nullptr

lines (linked list of Line nodes, one per corpus line)
┌─────────────────────┐     ┌─────────────────────┐
│ Line  n=3           │────▶│ Line  n=2           │────▶ ...
│ tokens ────────┐    │     │ tokens ────────┐    │
└────────────────│────┘     └────────────────│────┘
                 ▼                           ▼
           Token(token_id=0, occurrence=▶OccurrenceNode)
                 │
           Token(token_id=1, occurrence=▶OccurrenceNode)
                 │
           Token(token_id=0, occurrence=▶OccurrenceNode)  ← "the" again, different occurrence
```

---

## `build_hash_table()` — Step by Step

### Phase 1: Initialisation

```cpp
size_t bucket_count = KEYS_COMMON_STARTING_SIZE;
size_t bucket_used  = 0;

hash_table  = new WordRecord*[bucket_count]();   // zero-initialised → all nullptr
index_table = new size_t[bucket_count]();        // zero-initialised

LINE *lines_head = nullptr, *lines_tail = nullptr;
```

Both arrays are zero-initialised with `()`. This is critical — `hash_table[key] == nullptr` is the sentinel that means "this bucket is empty / this word is new". Without `()`, the check becomes undefined behaviour on uninitialised garbage.

`bucket_used` tracks the vocabulary size and doubles as the `word_id` counter. It is never reset.

`lines_head` and `lines_tail` track the corpus line linked list. `lines_head` is returned in `TABLES::lines`.

---

### Phase 2: Corpus Iteration

The function iterates over every line, and within each line over every token.

#### Per-line setup

At the start of each line a new `LINE` node is allocated and appended to the lines linked list:

```
if lines_head == nullptr:
    allocate LINE, set lines_head = lines_tail = new node
else:
    allocate LINE, link to lines_tail->next, advance lines_tail

lines_tail->n      = 0
lines_tail->tokens = nullptr
```

#### Per-token processing

For each token, a new `TOKEN` node is allocated and appended to the current line's token list. `lines_tail->n` is then incremented **once, unconditionally**, before any hash table branch is entered:

```
allocate TOKEN, link into lines_tail->tokens list
lines_tail->n++

key = generate_key(token, bucket_count)
→ case A: hash_table[key] == nullptr       (empty bucket)
→ case B: hash_table[key]->word == token   (same word, no collision)
→ case C: hash_table[key]->word != token   (collision — different word)
```

After every branch, `tokens->token_id` and `tokens->occurrence` are set to bind the `TOKEN` node to its `WordRecord` and its specific `OccurrenceNode`.

#### Case A — Empty Bucket (New Word)

The token has never been seen before.

```
1. Create OccurrenceNode(line_number, token_number, next=nullptr, prev=nullptr)
2. Create WordRecord(word_id=bucket_used, word=token, head=occurrence)
3. hash_table[key]                  = word_record
4. index_table[word_record->word_id] = key
5. tokens->token_id  = word_record->word_id
6. tokens->occurrence = occurrence
7. bucket_used++
```

The `word_id` is stamped at insertion time and never changes.

#### Case B — Same Word (No Collision)

The token already exists in the table and maps directly to its bucket.

```
1. word_record = hash_table[key]
2. Traverse occurrence linked list from head to tail
3. Append new OccurrenceNode(line_number, token_number, next=nullptr, prev=tail)
4. tokens->token_id  = word_record->word_id
5. tokens->occurrence = new occurrence node
```

No new `WordRecord` is created. `bucket_used` is NOT incremented.

#### Case C — Collision (Different Word, Same Bucket)

Two distinct words hashed to the same bucket. **Linear probing** resolves this.

```
probe = (key + 1) % bucket_count

loop:
    if hash_table[probe] == nullptr:
        → New word displaced from its natural bucket
        → Create OccurrenceNode + WordRecord
        → hash_table[probe]                   = word_record
        → index_table[word_record->word_id]   = probe   ← MUST store probe, not key
        → tokens->token_id  = word_record->word_id
        → tokens->occurrence = occurrence
        → bucket_used++
        → break

    elif hash_table[probe]->word == token:
        → Found the actual record (word was previously displaced here)
        → Traverse to tail of its occurrence list
        → Append new OccurrenceNode
        → tokens->token_id  = word_record->word_id
        → tokens->occurrence = new occurrence node
        → break

    else:
        probe = (probe + 1) % bucket_count
        → Keep probing
```

**Critical detail:** `index_table` records `probe` — the actual location — not `key`. Without this, `word_id → hash → WordRecord` lookups would silently return the wrong record for any collided word.

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
2. bucket_count     = Keys::next_prime(bucket_count)   ← grow to next prime
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

**CRITICAL:** Only the **arrays** are freed. `WordRecord`, `OccurrenceNode`, `LINE`, and `TOKEN` objects live on the heap independently and are **never touched during rehash**. Their pointers are simply copied into the new arrays. All occurrence linked lists and the entire lines/tokens linked list survive rehash completely intact.

`LINE` and `TOKEN` nodes hold `token_id` (a `word_id`, not a bucket index) and `OccurrenceNode*` (a heap pointer, not an array offset). Neither value is affected by bucket reorganisation.

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

`token_number` is a **within-line** counter, reset per line. It records the position of a token within its line — not a global token count. This value is stored in `OccurrenceNode::token`.

`line_number` is a global counter, never reset during the build. It is stored in `OccurrenceNode::line`.

---

### Phase 5: Return

A `TABLES` composite is allocated on the heap and all fields are populated:

```cpp
tables->hash_to_word_record     = hash_table;
tables->word_id_to_hash         = index_table;
tables->lines                   = lines_head;   // head of corpus line linked list
tables->bucket_count            = bucket_count;
tables->bucket_used             = bucket_used;  // = vocabulary size
tables->ref_count               = 1;
tables->maximum_tokens_per_line = mxntpl;
tables->minimum_tokens_per_line = mnntpl;
tables->total_tokens            = tnt;
```

Ownership of all heap-allocated structures transfers to `TABLES`. The caller is responsible for the lifetime of this object.

---

## Invariants

These invariants must hold at all times for the structure to be correct:

| Invariant | Reason |
|---|---|
| `bucket_used < bucket_count` | Enforced by load factor. Guarantees at least one empty slot exists for probing |
| `index_table[word_id]` always points to the actual bucket of that word | Maintained by storing `probe` (not `key`) on collision |
| `word_id` values are dense in `[0, bucket_used)` | Enables direct use as embedding matrix row indices |
| `hash_table` is zero-initialised on every allocation | `nullptr` is the only sentinel for empty buckets |
| Occurrence lists are in corpus order | New occurrences are always appended to the tail |
| `WordRecord` and `OccurrenceNode` are never freed during rehash | Only the bucket arrays are replaced; heap objects survive |
| `LINE` and `TOKEN` nodes are never touched during rehash | They hold `word_id` and `OccurrenceNode*`, not bucket indices |
| `LINE::n` equals the number of `TOKEN` nodes in that line | Incremented exactly once per token, unconditionally, before any hash branch |
| `TOKEN::token_id == WordRecord::word_id` for the corresponding word | Set in every branch (A, B, C) before moving to the next token |
| `TOKEN::occurrence` points to this token's exact `OccurrenceNode` | Set in every branch; the node records the `(line, token)` of this specific occurrence |

---

## Complexity

| Operation | Average Case | Worst Case |
|---|---|---|
| Token lookup / insert | O(1) | O(n) — full probe sequence |
| Occurrence append | O(k) where k = occurrences so far | O(k) |
| Rehash | O(m) where m = old bucket count | O(m) |
| Lookup by `word_id` | O(1) | O(1) |
| Full corpus traversal via `lines` | O(T) where T = total tokens | O(T) |

Rehash is amortised O(1) per insertion over the lifetime of the build, assuming the load factor threshold keeps rehash frequency logarithmic in the vocabulary size.

---

## Connection to the Transformer

The `word_id` assigned to each `WordRecord` is the token's index into the **embedding matrix** `E` of shape `[vocab_size × d_model]`:

```
embedding(token) = E[word_id]   ← row lookup, O(1)
```

`bucket_used` at the end of the build equals `vocab_size`.

The `lines` linked list provides the full sequential layout of the corpus:

```
lines → Line₀ → Token(token_id, occurrence)
                Token(token_id, occurrence)
                ...
      → Line₁ → Token(token_id, occurrence)
                ...
```

This enables:
- **Sequence reconstruction** — the exact token order of each line is preserved for positional encoding and attention mask generation, without re-reading the file.
- **Positional encoding** — `token_id` gives the embedding row; position within the line is its index in the `TOKEN` list.

The occurrence linked lists remain available for:
- **Frequency analysis** — count nodes in a word's occurrence list
- **Concordance / co-occurrence** — traverse neighbouring occurrences
- **Positional statistics** — `OccurrenceNode::line` and `OccurrenceNode::token` give exact corpus position

At inference/training time, the hash table gives `word string → word_id` in O(1), the embedding matrix gives `word_id → vector` in O(1), and the lines list gives the full token sequence in O(T). The corpus is thus fully pre-processed before any matrix operation begins.
