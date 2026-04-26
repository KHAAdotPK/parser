# Linear Probing — Collision Resolution in Hash Tables

## Table of Contents

1. [What Is a Hash Collision?](#1-what-is-a-hash-collision)
2. [What Is Linear Probing?](#2-what-is-linear-probing)
3. [The Probing Formula](#3-the-probing-formula)
4. [Three Scenarios Linear Probing Must Handle](#4-three-scenarios-linear-probing-must-handle)
5. [Walkthrough — Inserting a New Word (Collision Path)](#5-walkthrough--inserting-a-new-word-collision-path)
6. [Walkthrough — Finding an Existing Word (Collision Path)](#6-walkthrough--finding-an-existing-word-collision-path)
7. [Walkthrough — Rehashing with Linear Probing](#7-walkthrough--rehashing-with-linear-probing)
8. [Why the Wrap-Around (`% bucket_count`) Matters](#8-why-the-wrap-around--bucket_count-matters)
9. [The Probe Termination Condition](#9-the-probe-termination-condition)
10. [Known Limitations of Linear Probing](#10-known-limitations-of-linear-probing)
11. [Summary](#11-summary)

---

## 1. What Is a Hash Collision?

A hash table maps a word (key) to a **bucket index** using a hash function. Because the number of possible words is vastly larger than the number of buckets, two distinct words can compress to the **same index**. This is a collision.

Example using `Keys::generate_key()` with `bucket_count = 10`:

```
"hello"  →  raw hash → ... % 10  →  bucket 7
"start"  →  raw hash → ... % 10  →  bucket 7   ← collision!
```

Both words have a legitimate claim to bucket `7`. The hash table must decide what to do when the target bucket is already occupied.

---

## 2. What Is Linear Probing?

**Linear probing** is one of the simplest open-addressing collision-resolution strategies.

The rule is: if the target bucket is occupied, **step forward one bucket at a time** until you find a bucket that is either:

- **Empty** — insert the new word here, or
- **Occupied by the same word** — the word already exists; update it.

No extra memory structures (like linked lists per bucket) are needed. All entries live directly in the flat array.

```
Buckets:    [0]  [1]  [2]  [3]  [4]  [5]  [6]  [7]  [8]  [9]
Contents:                                       "hello"
                                                  ↑
"start" wants bucket 7, but it's taken.
Probe → bucket 8 (empty) → insert "start" there.
```

---

## 3. The Probing Formula

The implementation uses this formula to advance the probe:

```cpp
size_t probe = (key + 1) % bucket_count;   // start one past the collision
// ...
probe = (probe + 1) % bucket_count;        // advance each iteration
```

The modulo (`% bucket_count`) wraps the probe back to `0` when it reaches the end of the array, turning the flat array into a logical ring.

```
Buckets:  [0]  [1]  [2]  ...  [8]  [9]
                                     ↓ wrap
                               ←←←←←[0]
```

---

## 4. Three Scenarios Linear Probing Must Handle

When a word arrives and its computed bucket `key` is occupied, the probe loop checks each slot it visits and takes one of three actions:

| Slot state | Meaning | Action |
|---|---|---|
| `hash_table[probe] == nullptr` | Empty slot found | Insert new `WordRecord` here |
| `hash_table[probe]->word == token` | Same word found | Append a new `OccurrenceNode` to its linked list |
| Otherwise | Different word, keep searching | Advance probe by 1 |

These three cases map directly to the `if / else if / (implicit continue)` structure inside the probe loop in `build_hash_table()`.

---

## 5. Walkthrough — Inserting a New Word (Collision Path)

The code first checks whether the initial bucket is empty:

```cpp
size_t key = Keys::generate_key(token, bucket_count);

if (hash_table[key] == nullptr)
{
    // Bucket is free — direct insert, no probing needed.
    occurrence = new OccurrenceNode(line_number, token_number, nullptr, nullptr);
    word_record = new WordRecord(bucket_used, token, occurrence);
    hash_table[key] = word_record;
    index_table[word_record->word_id] = key;
    bucket_used++;
}
```

If it is not empty and the word does **not** match (`hash_table[key]->word != token`), linear probing begins:

```cpp
size_t probe = (key + 1) % bucket_count;   // start one past the collision

while (probe != key)   // full-circle guard — stop if we've checked every bucket
{
    if (hash_table[probe] == nullptr)
    {
        // ── Case 1: Empty slot ──────────────────────────────────────────────
        // A different word collided with ours. This slot is free — insert here.
        occurrence = new OccurrenceNode(line_number, token_number, nullptr, nullptr);
        word_record = new WordRecord(bucket_used, token, occurrence);
        hash_table[probe] = word_record;
        index_table[word_record->word_id] = probe;   // record probe, not key
        bucket_used++;
        break;
    }
    else if (hash_table[probe]->word == token)
    {
        // ── Case 2: Word already exists (found after probing) ───────────────
        // (handled below — see Section 6)
        break;
    }

    probe = (probe + 1) % bucket_count;   // advance
}
```

**Concrete example:**

```
bucket_count = 10

Step 1: "hello" → key = 7. Bucket 7 is empty. Insert directly.
         [7] = "hello"

Step 2: "start" → key = 7. Bucket 7 is occupied by "hello". "hello" ≠ "start".
         Begin probing:
           probe = (7 + 1) % 10 = 8 → hash_table[8] == nullptr → INSERT "start" at [8]
         index_table[word_id_of_start] = 8   ← stores 8, not 7!
```

Note the comment in the code:
```cpp
index_table[word_record->word_id] = probe;   // stores the actual slot, not the original key
```
This is important. `index_table` maps a word's ID back to **where it actually lives in the array**, which may be different from where the hash function pointed.

---

## 6. Walkthrough — Finding an Existing Word (Collision Path)

If `"start"` was already inserted at bucket `8`, and the corpus contains `"start"` again later:

```
"start" → key = 7. Bucket 7 is occupied by "hello". "hello" ≠ "start".
Begin probing:
  probe = 8 → hash_table[8]->word == "start" → MATCH FOUND
```

The code then appends a new `OccurrenceNode` to `"start"`'s existing linked list:

```cpp
else if (hash_table[probe]->word == token)
{
    // Found the actual matching word — not a new word, just another occurrence
    word_record = hash_table[probe];

    occurrence = word_record->head;
    while (occurrence->next != nullptr)  // traverse to the tail of the occurrence list
    {
        occurrence = occurrence->next;
    }

    // Append a new occurrence (line number, token position) to the list
    occurrence->next = new OccurrenceNode(line_number, token_number, nullptr, occurrence);
    break;
}
```

This is the same logic used for a word found at its **primary bucket** (no collision), just reached via a probe instead of directly.

---

## 7. Walkthrough — Rehashing with Linear Probing

When the load factor exceeds `KEYS_LOAD_FACTOR_THRESHOLD` (0.7):

```cpp
if ((static_cast<double>(bucket_used) / static_cast<double>(bucket_count)) > KEYS_LOAD_FACTOR_THRESHOLD)
```

...the table is grown and all entries are re-inserted into the new array. Crucially, every entry must be **re-probed** in the new table because the larger `bucket_count` changes every hash index:

```cpp
size_t new_key = Keys::generate_key(hash_table[i]->word, bucket_count); // new bucket_count

if (new_hash_table[new_key] == nullptr)
{
    // Direct placement — no collision in the new table
    new_hash_table[new_key] = hash_table[i];
    new_index_table[hash_table[i]->word_id] = new_key;
}
else
{
    // Collision even in the larger table — probe again
    size_t probe = (new_key + 1) % bucket_count;
    while (probe != new_key)
    {
        if (new_hash_table[probe] == nullptr)
        {
            new_hash_table[probe] = hash_table[i];
            new_index_table[hash_table[i]->word_id] = probe;
            break;
        }
        probe = (probe + 1) % bucket_count;
    }
}
```

**Why does rehashing help?**

With a larger `bucket_count`, the modulo compresses raw hashes into a wider range, so words that collided before are likely to land in different buckets after rehashing. The load factor drops well below `0.7`, and the average probe chain length shortens dramatically.

```
Before rehash:  bucket_count = 1009, bucket_used = 707  → load = 0.70 (threshold hit)
After rehash:   bucket_count = 1013, bucket_used = 707  → load = 0.698 (just under threshold)
```

> **Note:** The implementation calls `Keys::next_prime(bucket_count)` which returns only the next prime immediately above the current size (e.g. `1009 → 1013`). For large corpora this will trigger rehashing again very quickly. A more robust strategy is `Keys::next_prime(bucket_count * 2)` to roughly double capacity and keep the load factor low for longer.

---

## 8. Why the Wrap-Around (`% bucket_count`) Matters

Without wrapping, probing past the last bucket would access memory out of bounds. The modulo makes the array behave as a circular buffer:

```
bucket_count = 10

Probe from bucket 8:
  probe = 8 → occupied
  probe = (8+1) % 10 = 9 → occupied
  probe = (9+1) % 10 = 0 → empty → insert here ✓

Without modulo:
  probe = 10 → out of bounds → undefined behaviour ✗
```

---

## 9. The Probe Termination Condition

The while loop guards against probing forever:

```cpp
while (probe != key)   // stop if probe wraps all the way back to the original bucket
{
    // ...
    probe = (probe + 1) % bucket_count;
}
```

If `probe` returns to `key`, every bucket has been visited and no empty slot was found — the table is completely full. In a well-maintained hash table (load factor ≤ 0.7), this should never happen: rehashing kicks in long before the table saturates.

---

## 10. Known Limitations of Linear Probing

### Primary Clustering

Because probing is strictly sequential, a run of occupied consecutive buckets tends to grow. Any word whose hash lands anywhere in that run extends it further. This is called **primary clustering** and it degrades lookup time from O(1) average toward O(n) worst case.

```
Buckets: [_][_][X][X][X][X][_][_]
                 ↑─────────────┘
         Any key hashing here joins the cluster and extends it.

There is no randomness, no skipping. Every displaced word lands as close to its home bucket as possible, and that closeness is exactly what causes clusters to merge. If bucket 7 is full and "start" goes to bucket 8, then any word that hashes to 8 now has to probe to 9 — even though it had no collision to begin with. Buckets 7, 8, and 9 are now a single cluster even though only bucket 7 had the original collision.

// What it looks like in the code

size_t probe = (key + 1) % bucket_count;
while (probe != key)
{
    if (hash_table[probe] == nullptr) { /* insert here */ break; }
    else if (hash_table[probe]->word == token) { /* update here */ break; }
    probe = (probe + 1) % bucket_count;
}
```

The implementation mitigates this by keeping the load factor below `0.7`, which statistically limits average cluster length.

### Why 0.7 is the threshold, not 0.9 or 0.5

The relationship between load factor and average probe length for linear probing is described by the **Knuth formula** (from Donald Knuth's *The Art of Computer Programming*):

```
average probes (successful lookup)   ≈ ½ × (1 + 1 / (1 - α))
average probes (unsuccessful lookup) ≈ ½ × (1 + 1 / (1 - α)²)
```

where `α` is the load factor. Plugging in some values:

| Load factor (α) | Avg probes (hit) | Avg probes (miss) |
|---|---|---|
| 0.50 | 1.5 | 2.5 |
| 0.70 | 2.2 | 6.2 |
| 0.80 | 3.0 | 13.0 |
| 0.90 | 5.5 | 50.5 |
| 0.95 | 10.5 | 200.5 |

The table shows why 0.7 is the industry standard. From 0.5 to 0.7 the degradation is still manageable (1.5 → 2.2 probes on a hit). From 0.7 to 0.9 it explodes — a miss at 90% load costs 50× more than a miss at 50% load. The implementation in `build_hash_table()` uses `KEYS_LOAD_FACTOR_THRESHOLD = 0.7` exactly because the curve bends sharply here.

---

### Deletions Are Unsafe

Linear probing breaks if you delete a record by simply setting its slot to `nullptr`. A probe chain that passed through that slot during an earlier insert will be severed — a later lookup for the word at the end of the chain will stop prematurely at the now-empty slot and incorrectly conclude the word is absent.

The implementation avoids this problem because it never deletes individual entries during normal operation — records persist for the lifetime of the table.

When deletion genuinely needs to be supported, there are three approaches, each with different trade-offs.

**Tombstone markers** replace a deleted slot with a special sentinel value — not `nullptr`, but a distinct constant like `DELETED`. The probe loop is changed to treat tombstones as occupied (keep scanning) during lookups but as free (stop and insert) during insertions. This preserves chains for lookups while reclaiming the slot for future inserts. The cost is that tombstones accumulate over time and are never freed until a full rehash — a table with many deletions can end up mostly tombstones, with the real load factor much lower than the apparent one. This is exactly what CPython's dictionary implementation uses.

**Backward shift deletion** avoids tombstones entirely. After deleting a slot, the algorithm scans forward and finds any neighbour that is displaced from its home bucket — i.e. any word that was pushed here by probing past the now-empty slot. That neighbour is slid one step backward toward its home, filling the gap. This repeats until a truly empty slot is found. The result is a gap-free array with no sentinels, and lookups remain correct because the invariant is restored immediately. The cost is that each deletion may cascade through several shifts. This is more complex to implement correctly but more memory-efficient than tombstones.

**Robin Hood hashing** is a variant of linear probing where every entry also stores its displacement — the number of probe steps it took to reach its current slot. During insertion, if a new entry has a shorter displacement than the one already in a slot ("richer" than the incumbent), the incumbent is evicted and re-inserted elsewhere. This keeps displacement variance low across the whole table, which makes lookups uniformly fast. Deletion with Robin Hood involves filling the vacated slot from the next entry (similar to backward shifting) while maintaining the displacement invariant. It is the most sophisticated of the three and is used in many high-performance hash maps.

---

### Rehash Does Not Eliminate Collisions

The inline comment in the code is explicit about this:

```cpp
/*
 * No check of the hash collision is done here, because we are setting a threshold for the load factor.
 * Note: The load factor only reduces collision probability, it does not eliminate it.
 * At any load factor, two different words can still hash to the same bucket. This is a data-loss bug, not a probability question.
 */
```

This is why the rehash loop **also** applies linear probing — it does not blindly overwrite `new_hash_table[new_key]`, it checks first and probes if that slot is taken.

---

## 11. Summary

| Concept | Detail |
|---|---|
| **Collision** | Two words map to the same bucket index after `hash % bucket_count` |
| **Linear probe start** | `probe = (key + 1) % bucket_count` |
| **Advance** | `probe = (probe + 1) % bucket_count` |
| **Stop: empty slot** | Insert new `WordRecord` at `probe`; store `probe` (not `key`) in `index_table` |
| **Stop: word match** | Append new `OccurrenceNode` to existing word's list |
| **Stop: full circle** | `probe == key` — table is full (should not happen at load ≤ 0.7) |
| **Rehash trigger** | `bucket_used / bucket_count > 0.7` |
| **Rehash probing** | Same linear probe logic applied to the new, larger array |
| **Wrap-around** | `% bucket_count` turns the flat array into a logical ring |
| **Primary weakness** | Primary clustering degrades performance at high load factors |
