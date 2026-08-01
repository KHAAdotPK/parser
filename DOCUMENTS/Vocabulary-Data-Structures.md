# Vocabulary data structures in the current parser flow

This note traces the journey in [usage/main.cu](../../usage/main.cu) and focuses on the data structures that are created and consumed while building the vocabulary and later training inputs.

## 1. Starting point: the corpus parser

The program starts by creating a `Parser` instance over the input corpus:

```cpp
Parser parser(argv[1]);
```

The parser reads the corpus line by line and collects vocabulary statistics such as:

- `bucket_count`
- `bucket_used`
- `nol` (number of lines)
- `tnt` (total number of tokens)
- `mxntpl` / `mnntpl` (line-length extremes)

These values are stored on the `Parser` object itself and are later used to understand the size and shape of the built vocabulary.

---

## 2. The first vocabulary structure: `WordRecord_new`

Each unique token becomes a `WordRecord_new` object.

```cpp
struct WordRecord_new
{
    size_t word_id;
    std::string word;
    size_t n;
};
```

This is the atomic unit of vocabulary. It stores:

- `word_id`: a compact integer assigned to the unique word
- `word`: the token string itself
- `n`: how many times that token appeared in the corpus

In other words, `WordRecord_new` is the vocabulary entry for one word.

---

## 3. The primary vocabulary container: `WordRecord_new** hash_table`

The first major structure produced by the parser is a hash table of vocabulary entries:

```cpp
const WordRecord_new* const *const hash_table =
    (const WordRecord_new* const *const) parser.build_hash_table_very_new(&index_table);
```

This is a pointer-to-pointer array:

```cpp
WordRecord_new** hash_table
```

### What it represents

- The array is indexed by the hash bucket / slot.
- Each non-null entry points to a `WordRecord_new` object.
- This structure lets the program find the vocabulary record for a token by its hash slot.

### Role in the pipeline

The hash table is the main lookup structure for the vocabulary itself. It answers questions like:

- “What vocabulary entry lives at this hash slot?”
- “Which token is stored here?”
- “How many times did this token appear?”

---

## 4. The second vocabulary mapping: `size_t* index_table`

The parser also builds an `index_table`:

```cpp
size_t* index_table = nullptr;
WordRecord_new** vocab = parser.build_hash_table_very_new(&index_table);
```

This array is the word-id-to-slot mapping.

### What it represents

- It is indexed by `word_id`.
- Each entry stores the actual bucket/slot where the corresponding vocabulary record is found.
- In practice, this gives the program a direct way to go from a word’s assigned ID to the location of its record inside the hash table.

### Why it matters

The vocabulary has two related identifiers:

- `word_id`: the compact numeric ID assigned to the word
- hash slot / key: the position used in the hash table for lookup

`index_table` bridges those two views.

So the relationship is conceptually:

```text
word_id  ->  hash slot / bucket location  ->  WordRecord_new
```

This is the key mapping the README now describes explicitly.

---

## 5. The line-level structure: `WORDS** lines_array`

After the vocabulary hash table is built, the parser creates a flat representation of the corpus lines:

```cpp
WORDS** lines_array = parser.build_lines_table(hash_table);
```

Each item in `lines_array` is a `WORDS` object:

```cpp
struct words
{
    size_t n;
    size_t* keys;
};
```

### What it represents

- `n`: how many tokens this line contains
- `keys`: a contiguous array of vocabulary keys for the tokens on that line

### Why this exists

This is a compact, cache-friendly representation of the corpus as a sequence of lines. Instead of storing full strings again, it stores the numeric vocabulary keys for each token on each line.

So the flow becomes:

```text
corpus text -> tokens -> vocabulary records -> line-level key arrays
```

---

## 6. The training-pair structure: `ContextPairs`

Once lines are represented by their vocabulary keys, the program builds context pairs for training:

```cpp
struct ContextPairs** contexts = pairs.build_pairs(parser, lines_array, (WordRecord_new**)hash_table);
```

The pair structures are defined as:

```cpp
struct ContextPair
{
    size_t* left_context_keys;
    size_t* right_context_keys;
    size_t  target_key;
};

struct ContextPairs
{
    size_t       n;
    ContextPair** pairs;
};
```

### What they represent

- `target_key`: the word id of the center/target word
- `left_context_keys`: the word ids of nearby words on the left
- `right_context_keys`: the word ids of nearby words on the right

The field names use “key”, but they actually store vocabulary word ids rather than hash table slots. A clearer naming convention would be `target_word_id`, `left_context_word_ids`, and `right_context_word_ids`.

These structures are not the vocabulary itself, but they depend directly on it.

They are built from the line-level key arrays and point back into the vocabulary via the hash table.

---

## 7. The full journey in one view

The current flow in [usage/main.cu](../../usage/main.cu) can be understood as:

1. Create a `Parser` over the corpus.
2. Build a vocabulary hash table of `WordRecord_new` objects.
3. Build `index_table` so each `word_id` maps to the correct slot in the hash table.
4. Build `WORDS** lines_array` where each line stores the vocabulary keys for its tokens.
5. Build `ContextPairs` from those line-level keys for training-style pair generation.
6. Free the allocated vocabulary and line structures at the end.

A compact summary is:

```text
Corpus
  -> Parser reads tokens
  -> WordRecord_new objects form the vocabulary
  -> hash_table stores vocabulary by slot
  -> index_table maps word_id -> slot
  -> WORDS** lines_array stores per-line token keys
  -> ContextPairs uses those keys for training examples
```

---

## 8. Important conceptual distinction

The code uses several related but different structures:

- `WordRecord_new`: one record for one unique word
- `hash_table`: lookup structure over the vocabulary by hash slot
- `index_table`: mapping from `word_id` to the actual slot used in the hash table
- `WORDS`: line-level container of vocabulary keys
- `ContextPairs`: training examples built from those keys

These are not interchangeable. Each serves a different stage of the pipeline.

---

## 9. Ownership and cleanup

At the end of the run, the code calls:

```cpp
parser.free_tables_very_new((WordRecord_new**)hash_table, lines_array, index_table);
```

That means the vocabulary entries, the line arrays, and the index table are all owned by the caller and must be released once the downstream structures are no longer needed.
