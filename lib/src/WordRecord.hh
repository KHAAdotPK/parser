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

    WordRecord(size_t _word_id, std::string _word, OccurrenceNode* _head) : word_id(_word_id), word(_word), head(_head)
    {
        /* this->word_id = word_id;
        this->word = word;
        this->head = head; */
    }

    // Copy constructor - needed for vector<WordRecord>
    WordRecord(const WordRecord&) = default;
    // Assignment operator - needed for vector<WordRecord>
    WordRecord& operator=(const WordRecord&) = default;
};

// Each occurrence
struct OccurrenceNode
{
    size_t line;            // line number
    size_t token;           // token number

    OccurrenceNode* next;   // pointer to next occurrence
    OccurrenceNode* prev;   // pointer to previous occurrence

    OccurrenceNode(size_t _line, size_t _token, OccurrenceNode* _next, OccurrenceNode* _prev) : line(_line), token(_token), next(_next), prev(_prev)
    {
        /*this->line = line;
        this->token = token;
        this->next = next;
        this->prev = prev; */
    }

    // Copy constructor - needed for vector<OccurrenceNode>
    OccurrenceNode(const OccurrenceNode&) = default;
    // Assignment operator - needed for vector<OccurrenceNode>
    OccurrenceNode& operator=(const OccurrenceNode&) = default;
};

/*
 * The set of hash tables for efficiently indexing tokens encountered
 * during iteration over a corpus.
 * 
 * NOTE: In this file, the word "token" can be replaced with "word" in your mind.
 * 
 * This structure centralises all hash-table logic, including:
 * - Hash-to-WordRecord mapping (hash_to_word_record)
 * - Word-ID-to-hash-index mapping (word_id_to_hash)
 * - Hash-table metadata (bucket_count, bucket_used)
 * - Corpus statistics (maximum_tokens_per_line, minimum_tokens_per_line, total_tokens)
 */
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
    size_t bucket_used; // Vocabulary size. The size of corpus without redundancy.
 
    /*
        Reference Count
        ---------------
        The number of references to this instance of tables of hash table. 
        Used to determine when to deallocate the tables.
     */
    size_t ref_count;

    /*
        Maximum Tokens Per Line
        -----------------------
        This is the size of largest line in number of tokens
    */
    size_t maximum_tokens_per_line; 
    
    /*
        Minimum Tokens Per Line
        -----------------------
        This is the size of smallest line in number of tokens
    */  
    size_t minimum_tokens_per_line;

    /*
     * Total Number of Tokens (with redundancy)
     * ---------------------------------------
     * This represents the total number of tokens in the corpus.
     */
    size_t total_tokens;

    size_t get_bucket_count(void) const
    {
        return bucket_count;
    }

    size_t get_bucket_used(void) const
    {
        return bucket_used;
    }
    
    size_t get_maximum_tokens_per_line(void) const
    {
        return maximum_tokens_per_line;
    }
    
    size_t get_minimum_tokens_per_line(void) const
    {
        return minimum_tokens_per_line;
    }   
};

typedef struct Tables TABLES;

#endif // WORD_RECORD_HH