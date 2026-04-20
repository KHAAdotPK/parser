/*
    ./lib/src/WordRecord.hh
    Q@hackers.pk
 */

#ifndef WORD_RECORD_HH
#define WORD_RECORD_HH

struct OccurrenceNode;

struct WordRecord
{
    size_t word_id;         // unique integer assigned to this word
    std::string word;       // the word itself
    OccurrenceNode* head;   // points to first occurrence in corpus

    WordRecord(size_t word_id, std::string word, OccurrenceNode* head)
    {
        this->word_id = word_id;
        this->word = word;
        this->head = head;
    }
};

// Each occurrence
struct OccurrenceNode
{
    size_t line;            // line number
    size_t token;           // token number

    OccurrenceNode* next;   // pointer to next occurrence
    OccurrenceNode* prev;   // pointer to previous occurrence

    OccurrenceNode(size_t line, size_t token, OccurrenceNode* next, OccurrenceNode* prev)
    {
        this->line = line;
        this->token = token;
        this->next = next;
        this->prev = prev;
    }
};

struct Tables 
{
    WordRecord** hash_to_word_record; // a.k.a hash_table
    size_t* word_id_to_hash; // a.k.a index_table

    /*
     * The number of buckets in the hash table used to index tokens encountered
     * during iteration. Each unique token from the corpus is hashed into one of
     * these buckets using Keys::generate_key().
     *
     * Starts at KEYS_COMMON_STARTING_SIZE (1009) — a prime chosen to minimise
     * clustering after modulo compression. As the iterator advances through the
     * corpus and the vocabulary grows, the hash table will rehash when the load
     * factor exceeds an acceptable threshold, at which point bucket_count is
     * updated to the next suitable prime and all existing keys are reindexed.
     *
     * This value is therefore not fixed — it reflects the current capacity of
     * the hash table at any point during iteration, not its final size.
     */
    size_t bucket_count;
    /*
     * The number of buckets currently occupied by a WordRecord.
     * Incremented each time a new unique token is encountered and
     * inserted into the hash table for the first time.
     * Never decremented — tokens are never removed during iteration.
     *
     * Used together with bucket_count to compute the current load factor:
     *
     *     load_factor = buckets_used / bucket_count
     *
     * When this ratio exceeds KEYS_LOAD_FACTOR_THRESHOLD, the hash table
     * is rehashed and bucket_count is updated to the next suitable prime.
     */
    size_t bucket_used;
 
    /*
        Reference Count
        ---------------
        The number of references to this instance of tables of hash table. 
        Used to determine when to deallocate the tables.
     */
    size_t ref_count;

    size_t get_bucket_count(void) const
    {
        return bucket_count;
    }

    size_t get_bucket_used(void) const
    {
        return bucket_used;
    }    
};

typedef struct Tables TABLES;

#endif // WORD_RECORD_HH