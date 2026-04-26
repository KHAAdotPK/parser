/*
    lib/src/Parser.hh
    Q@hackers.pk
 */

#ifndef CSV_PARSER_LIB_PARSER_HH
#define CSV_PARSER_LIB_PARSER_HH

class Parser
{
    std::string _file_name;
    std::ifstream _file;
    bool _is_open;

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
    // size_t bucket_count;
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
    // size_t buckets_used;

    /*WordRecord** hash_table;
    size_t* index_table; 
    size_t line_number;
    size_t token_number;*/

    public:

        // Constructors
        Parser() : _file_name(), _file(), _is_open(false)  /*,bucket_count(size_t(KEYS_COMMON_STARTING_SIZE)), buckets_used(0),*/ /*hash_table(nullptr), index_table(nullptr), line_number(0), token_number(0)*/   
        {

        }

        explicit Parser(const std::string& name) : _file_name(name), _file(), _is_open(false)/*, bucket_count(size_t(KEYS_COMMON_STARTING_SIZE)), buckets_used(0),*/ /*hash_table(nullptr), index_table(nullptr), line_number(0), token_number(0)*/
        {
            _file.open(_file_name);

            if (_file.is_open())
            {
                _is_open = true;
            }
            else
            {
                throw std::runtime_error("Parser::Parser(const std::string&) Error: Could not open file " + _file_name);
            }

            try
            {
                //hash_table = new WordRecord*[bucket_count](); // Create array of pointers to WordRecord and return address of first element of the array
                //index_table = new size_t[bucket_count](); // Create array of hashed keys (size_t) and return address of first element of the array
                /*
                 * The () at the end is critical — it zero-initialises every pointer to nullptr.
                 * Without it, all bucket pointers are uninitialised garbage, and your
                 * (hash_table[key] == nullptr) check for unique words becomes undefined behaviour.
                 */
            }
            catch (const std::bad_alloc& e)
            {
                // Corpus can be large. This is a real possibility, not a formality.             
                throw std::runtime_error("Parser::Parser(const std::string&) Error: " + std::string(e.what()));
            }
        }

        // Non‑copyable because of file stream
        Parser(const Parser&) = delete;
        Parser& operator=(const Parser&) = delete;
    
        /*
            Move constructor: Transfers ownership of the file stream from the
            source object to the destination object. The source object is left
            in a valid but unspecified state (specifically, its _is_open flag is
            set to false and its stream is left in a state where it no longer
            owns the file). This is a noexcept operation because it only involves
            moving member variables and does not throw exceptions.
        */
        Parser(Parser&& other) noexcept = default;
 
        // Destructor – file closed automatically
        ~Parser() = default;

         /*
        ╔══════════════════════════════════════════════════════════════════════════════════╗
        ║                           build_hash_table()                                     ║
        ╚══════════════════════════════════════════════════════════════════════════════════╝
 
        PURPOSE
        -------
        Iterates over the entire corpus once and builds two parallel arrays that together
        form the vocabulary table of the tokenizer.  Every unique token in the corpus is
        assigned a permanent, dense, sequential ID (word_id = 0, 1, 2, …) and stored in
        a hash table for O(1) average-case lookup.  Every position at which a token
        appears (line_number, token_number) is recorded in a per-word linked list so that
        occurrence data is fully pre-built by the time this function returns — no second
        pass over the corpus is ever needed.
 
        The returned TABLES* object is heap-allocated and its ownership transfers to the
        caller.  ref_count is initialised to 1.
 
 
        ════════════════════════════════════════════════════════════════════════════════════
        DATA STRUCTURES PRODUCED
        ════════════════════════════════════════════════════════════════════════════════════
 
        1.  hash_to_word_record   (WordRecord*[bucket_count])
            ─────────────────────────────────────────────────
            Flat array of pointers.  Index = hash key derived from the token string.
            Each non-null slot points to a WordRecord on the heap:
 
                WordRecord
                ├── word_id  → size_t          dense sequential ID, row index into embedding matrix
                ├── word     → string          the token string itself
                └── head     → OccurrenceNode* head of the occurrence linked list
 
            Each OccurrenceNode:
                ├── line_number   → size_t          line in the corpus (0-based, never reset)
                ├── token_number  → size_t          position within that line (0-based, reset per line)
                ├── next          → OccurrenceNode* next occurrence in corpus order
                └── prev          → OccurrenceNode* previous occurrence (doubly linked)
 
            Occurrences are appended to the tail so the list is always in corpus order.
 
        2.  word_id_to_hash   (size_t[bucket_count])
            ──────────────────────────────────────────
            Parallel array indexed by word_id.  Stores the bucket index at which that
            word's WordRecord actually lives in hash_to_word_record.  Needed because
            linear probing can displace a word from its natural hash key.
 
            Bidirectional lookup:
                token string  →  generate_key()             →  hash_to_word_record[key]  →  WordRecord
                word_id       →  word_id_to_hash[word_id]   →  hash_to_word_record[key]  →  WordRecord
 
 
        ════════════════════════════════════════════════════════════════════════════════════
        ALGORITHM — TOKEN PROCESSING (per token, three cases)
        ════════════════════════════════════════════════════════════════════════════════════
 
        key = Keys::generate_key(token, bucket_count)
 
        ┌─────────────────────────────────────────────────────────────────────────────┐
        │ CASE A │ hash_table[key] == nullptr          (bucket empty → new word)      │
        ├─────────────────────────────────────────────────────────────────────────────┤
        │  1. Allocate OccurrenceNode(line_number, token_number, next=null, prev=null)│
        │  2. Allocate WordRecord(word_id=bucket_used, word=token, head=occurrence)   │
        │  3. hash_table[key]              = word_record                              │
        │  4. index_table[word_record->word_id] = key                                 │
        │  5. bucket_used++                                                           │
        └─────────────────────────────────────────────────────────────────────────────┘
 
        ┌─────────────────────────────────────────────────────────────────────────────┐
        │ CASE B │ hash_table[key]->word == token      (same word, no collision)      │
        ├─────────────────────────────────────────────────────────────────────────────┤
        │  1. word_record = hash_table[key]                                           │
        │  2. Traverse occurrence list from head to tail                              │
        │  3. Append new OccurrenceNode(line_number, token_number, next=null, prev=tail)│
        │  bucket_used is NOT incremented — word already registered                    │
        └─────────────────────────────────────────────────────────────────────────────┘
 
        ┌─────────────────────────────────────────────────────────────────────────────┐
        │ CASE C │ hash_table[key]->word != token      (hash collision)               │
        ├─────────────────────────────────────────────────────────────────────────────┤
        │  Linear probe:  probe = (key + 1) % bucket_count,  advance until:           │
        │                                                                             │
        │  • hash_table[probe] == nullptr                                             │
        │        New word displaced from natural bucket.                              │
        │        Allocate OccurrenceNode + WordRecord, place at probe.                │
        │        index_table[word_id] = probe   ← MUST store probe, not key           │
        │        bucket_used++                                                        │
        │                                                                             │
        │  • hash_table[probe]->word == token                                         │
        │        Word was previously displaced here.  Found it.                       │
        │        Traverse its occurrence list and append new OccurrenceNode.          │
        │                                                                             │
        │  • otherwise: probe = (probe + 1) % bucket_count  — keep scanning           │
        └─────────────────────────────────────────────────────────────────────────────┘
 
        NOTE ON COLLISION SAFETY
        ────────────────────────
        The load factor threshold (KEYS_LOAD_FACTOR_THRESHOLD) keeps bucket_used well
        below bucket_count, which keeps probe chains short on average.  However, the
        load factor only reduces collision *probability* — it does not eliminate it.
        Two distinct words can always hash to the same bucket regardless of load factor.
        The word-comparison checks in Cases B and C are therefore mandatory correctness
        logic, not optional optimisations.
 
 
        ════════════════════════════════════════════════════════════════════════════════════
        REHASHING
        ════════════════════════════════════════════════════════════════════════════════════
 
        Triggered after every insertion when:
            (double)bucket_used / (double)bucket_count  >  KEYS_LOAD_FACTOR_THRESHOLD
 
        Procedure:
            1.  bucket_count = Keys::next_prime(bucket_count)   — grow to next prime
            2.  Allocate new_hash_table[bucket_count]() and new_index_table[bucket_count]()
            3.  For every occupied slot i in the old table:
                    new_key = generate_key(hash_table[i]->word, bucket_count)
                    if new_hash_table[new_key] == nullptr:
                        place directly, update new_index_table[word_id] = new_key
                    else:
                        linear probe in new table for empty slot,
                        update new_index_table[word_id] = probe
            4.  delete[] hash_table  and  delete[] index_table   (arrays only)
            5.  hash_table  = new_hash_table
                index_table = new_index_table
 
        CRITICAL: Only the bucket arrays are freed.  WordRecord and OccurrenceNode
        objects live on the heap independently.  Their pointers are simply copied into
        the new arrays.  All occurrence linked lists survive rehash completely intact.
 
        Prime-sized tables distribute hash keys more uniformly, reducing the likelihood
        of systematic clustering from any particular hash function.
 
 
        ════════════════════════════════════════════════════════════════════════════════════
        LINE / TOKEN COUNTERS
        ════════════════════════════════════════════════════════════════════════════════════
 
        line_number   — global, incremented once per line, never reset during the build.
                        Stored in OccurrenceNode to identify which line the token is on.
 
        token_number  — local to each line, reset to 0 at the end of every line.
                        Stored in OccurrenceNode to identify the token's position within
                        its line.  It is NOT a global corpus-wide token counter.
 
        mxntpl / mnntpl — track the longest and shortest lines (in tokens) seen so far,
                          stored in TABLES::maximum_tokens_per_line and
                          TABLES::minimum_tokens_per_line respectively.
 
 
        ════════════════════════════════════════════════════════════════════════════════════
        INVARIANTS
        ════════════════════════════════════════════════════════════════════════════════════
 
        •  bucket_used < bucket_count at all times
               Enforced by the load factor check.  Guarantees at least one empty slot
               always exists so that linear probing always terminates.
 
        •  index_table[word_id] always points to the actual bucket of that word
               Maintained by storing probe (not key) when a word is displaced.
 
        •  word_id values are dense in [0, bucket_used)
               Enables direct use as row indices into the embedding matrix E[vocab_size × d_model].
 
        •  hash_table is zero-initialised on every allocation (the () matters)
               nullptr is the only sentinel for "bucket is empty".
 
        •  Occurrence lists are always in corpus order
               New nodes are always appended to the tail.
 
        •  WordRecord and OccurrenceNode are never freed during rehash
               Only arrays are reallocated.  Heap objects are stable for the lifetime
               of the TABLES structure.
 
 
        ════════════════════════════════════════════════════════════════════════════════════
        CONNECTION TO THE TRANSFORMER
        ════════════════════════════════════════════════════════════════════════════════════
 
        At the end of the build:
            bucket_used == vocab_size
 
        The word_id of each WordRecord is the token's permanent index into the embedding
        matrix E of shape [vocab_size × d_model]:
            embedding(token) = E[word_id]     ← O(1) row lookup
 
        The hash table provides:
            token string  →  word_id          O(1) average,  used at encode time
            word_id       →  token string     O(1),          used at decode time
 
        See TABLES.md for full documentation including memory layout diagrams.
 
 
        RETURNS
        -------
        TABLES*  — heap-allocated, ref_count = 1, ownership transfers to caller.
 
        THROWS
        ------
        std::runtime_error  — wraps std::bad_alloc if any heap allocation fails.
                              All partially-allocated objects are cleaned up before
                              the exception propagates.
         */
        TABLES* build_hash_table(void)
        {
            size_t bucket_count = KEYS_COMMON_STARTING_SIZE;
            size_t bucket_used = 0;

            WordRecord** hash_table = nullptr;
            size_t* index_table = nullptr; 
            size_t line_number = 0;
            size_t token_number = 0;

            size_t mxntpl = 0, mnntpl = std::numeric_limits<size_t>::max();; // Maximum and Minimum number of tokens in a largest and smallest line

            try
            {
                hash_table = new WordRecord*[bucket_count](); // Create array of pointers to WordRecord and return address of first element of the array
                // index_table is indexed by word_id (0..bucket_used-1)
                // Safe because load factor guarantees bucket_used < bucket_count always
                index_table = new size_t[bucket_count](); // Create array of hashed keys (size_t) and return address of first element of the array
                /*
                 * The () at the end is critical — it zero-initialises every pointer to nullptr.
                 * Without it, all bucket pointers are uninitialised garbage, and your
                 * (hash_table[key] == nullptr) check for unique words becomes undefined behaviour.
                 */
            }
            catch (const std::bad_alloc& e)
            {
                // Corpus can be large. This is a real possibility, not a formality.             
                throw std::runtime_error("Parser::build_hash_table(void) Error: " + std::string(e.what()));
            }

            for (auto& line : *this)
            {      
                for (auto& token : line)
                {   
                    size_t key = Keys::generate_key(token, bucket_count);

                    OccurrenceNode* occurrence = nullptr;
                    WordRecord* word_record = nullptr;

                    if (hash_table[key] == nullptr) // If the bucket is empty, it means the token/word is new
                    {
                        //std::cout<< "New bucket created for " << token << " at line " << line_number << " and token " << token_number << std::endl; 

                        try
                        {
                            occurrence = new OccurrenceNode(line_number, token_number, nullptr, nullptr);
                            word_record = new WordRecord(/*token_number*/ bucket_used, token, occurrence);
                            hash_table[key] = word_record;
                            index_table[/*token_number*/ word_record->word_id] = key;
                        }
                        catch (const std::bad_alloc& e)
                        {   
                            // Clean up allocated memory before throwing
                            if (occurrence != nullptr)
                            {
                                delete occurrence;
                            }
                            if (word_record != nullptr)
                            {
                                delete word_record;
                            }
                            throw std::runtime_error("Parser::build_hash_table(void) Error: " + std::string(e.what()));
                        }
                        
                        bucket_used++; // Increment the number of buckets used                        
                    }
                    else
                    {
                        // Different word, same bucket = hash collision
                        // COLLISION CHECK — is this actually the same word?
                        // It is a collision, but we need to check if the word is the same as the word in the bucket
                        // A collision resolution strategy here
                        // Simplest: linear probe to find the next free or matching slot
                        if(hash_table[key]->word != token)
                        {
                            //std::cout<< "Collision happened for " << token << " at line " << line_number << " and token " << token_number << " hash key " << key << " " << hash_table[key]->word << std::endl;   

                            size_t probe = (key + 1) % bucket_count;
                            while (probe != key)
                            {
                                if (hash_table[probe] == nullptr)
                                {
                                    // Found empty slot — treat as new word
                                    // (fall through to your "new bucket" logic)
                                    // ... insert word_record here ...
                                    try 
                                    { 
                                        occurrence = new OccurrenceNode(line_number, token_number, nullptr, nullptr);
                                        word_record = new WordRecord(/*token_number*/ bucket_used, token, occurrence);
                                        hash_table[probe] = word_record;
                                        index_table[/*token_number*/ word_record->word_id] = /*key*/probe;
                                    }
                                    catch (const std::bad_alloc& e)
                                    {
                                        // Clean up allocated memory before throwing
                                        if (occurrence != nullptr)
                                        {
                                            delete occurrence;
                                        }
                                        if (word_record != nullptr)
                                        {
                                            delete word_record;
                                        }
                                        throw std::runtime_error("Parser::build_hash_table(void) Error: " + std::string(e.what()));
                                    }                                    
                                    
                                    bucket_used++;
                                    break;
                                }
                                else if (hash_table[probe]->word == token)
                                {
                                    // Found the actual matching word
                                    word_record = hash_table[probe];

                                    occurrence = word_record->head; // Get the head of the linked list
                                    while (occurrence->next != nullptr) // Traverse to the end of the linked list
                                    {                             
                                        occurrence = occurrence->next;
                                    }
                                    try
                                    { 
                                        // Create a new occurrence node and append it to the end of the linked list which has all occurrences of this token/word in whole of the corpus
                                        occurrence->next = new OccurrenceNode(line_number, token_number, nullptr, occurrence);
                                    }
                                    catch (const std::bad_alloc& e)
                                    { 
                                        throw std::runtime_error("Parser::build_hash_table(void) Error: " + std::string(e.what()));
                                    }     

                                    break;
                                }
                                probe = (probe + 1) % bucket_count;
                            }
                        }
                        else
                        {
                            // Found the actual matching word
                            word_record = hash_table[key];

                            occurrence = word_record->head; // Get the head of the linked list
                            while (occurrence->next != nullptr) // Traverse to the end of the linked list
                            {                             
                                occurrence = occurrence->next;
                            }
                            try
                            { 
                                // Create a new occurrence node and append it to the end of the linked list which has all occurrences of this token/word in whole of the corpus
                                occurrence->next = new OccurrenceNode(line_number, token_number, nullptr, occurrence);
                            }
                            catch (const std::bad_alloc& e) 
                            { 
                                throw std::runtime_error("Parser::build_hash_table(void) Error: " + std::string(e.what()));
                            }     
                        }
                    }
                    
                    token_number++;

                    /*
                        Check if the hash table needs to be rehashed
                        Note: Integer division would truncate the result, so we cast to double
                    */
                    if ((static_cast<double>(bucket_used) / static_cast<double>(bucket_count)) > KEYS_LOAD_FACTOR_THRESHOLD)
                    {
                        /*
                            Rehash all existing entries into new table
                            Do NOT reset buckets_used, carry the real count forward
                        */
                        size_t old_bucket_count = bucket_count;
                        bucket_count = Keys::next_prime(bucket_count);

                        WordRecord** new_hash_table = nullptr;
                        size_t* new_index_table = nullptr;

                        try
                        {
                            new_hash_table = new WordRecord*[bucket_count]();
                            // new_index_table is indexed by word_id (0..bucket_used-1)
                            // Safe because load factor guarantees bucket_used < bucket_count always
                            new_index_table = new size_t[bucket_count]();
                            /*
                             * The () at the end is critical — it zero-initialises every pointer to nullptr.
                             * Without it, all bucket pointers are uninitialised garbage, and your
                             * (hash_table[key] == nullptr) check for unique words becomes undefined behaviour.
                             */
                        }
                        catch (const std::bad_alloc& e)
                        {
                            // Clean up allocated memory before throwing
                            if (new_hash_table != nullptr)
                            {
                                delete[] new_hash_table;
                            }
                            if (new_index_table != nullptr)
                            {
                                delete[] new_index_table;
                            }
                            throw std::runtime_error("Parser::build_hash_table(void) Error: " + std::string(e.what()));
                        }

                        /*
                            hash_table and index_table are rehashed here
                            Both has same size determined by bucket_count
                            Both are rehashed here because bucket_count is increased
                         */
                        for (size_t i = 0; i < old_bucket_count; i++)
                        {
                            if (hash_table[i] != nullptr)
                            {
                                /*
                                    No check of the hash collision is done here, becuasse we are setting a threshold for the load factor.
                                    Note: The load factor only reduces collision probability, it does not eliminate it. 
                                    At any load factor, two different words can still hash to the same bucket. This is a data-loss bug, not a probability question.

                                    But even then if collision did not happen when the token/word was first inserted, it will not happen later either.
                                    Given that now the memory size is increased as well, the probability of collision is further reduced.
                                 */
                                /*
                                    Empty String Check
                                    -------------------
                                    The word can be an empty string, which is not a valid word for the hash table
                                    But this check should not be made here.
                                    Instead this check should be made when the word is first inserted into the hash table.                                                                                                            
                                 */ 
                                size_t new_key = Keys::generate_key(hash_table[i]->word, bucket_count);

                                if (new_hash_table[new_key] == nullptr)
                                {
                                    new_hash_table[new_key] = hash_table[i]; 
                                    new_index_table[hash_table[i]->word_id] = new_key;  
                                }
                                else
                                {
                                    
                                    // Collision with the same key
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

                                //new_hash_table[new_key] = hash_table[i];

                                //new_index_table[hash_table[i]->word_id] = new_key;
                            }

                            //new_index_table[i] = index_table[i];
                        }

                        delete[] hash_table;
                        delete[] index_table;

                        hash_table = new_hash_table;
                        index_table = new_index_table;                        
                    }
                }

                if (token_number > mxntpl)
                {
                    mxntpl = token_number;
                }

                if (token_number < mnntpl)
                {
                    mnntpl = token_number;
                }
                
                token_number = 0;
                line_number++;
            }

            //line_number = 0;
            //token_number = 0;

            if (is_open())
            {
                _file.clear();
                _file.seekg(0);
            }

            TABLES* tables = nullptr;

            try
            {
                tables = new TABLES();

                tables->hash_to_word_record = hash_table;
                tables->word_id_to_hash = index_table;

                tables->bucket_count = bucket_count;
                tables->bucket_used = bucket_used;

                tables->ref_count = 1;

                tables->maximum_tokens_per_line = mxntpl;
                tables->minimum_tokens_per_line = mnntpl;
            }
            catch (const std::bad_alloc& e)
            {
                // Corpus can be large. This is a real possibility, not a formality.             
                throw std::runtime_error("Parser::build_hash_table(void) Error: " + std::string(e.what()));
            }
            
            return tables;
        }

        // Iterator access
        Iterator begin()
        { 
            return Iterator(&_file);
        }
        Iterator end()
        { 
            return Iterator();
        }

        // Utility
        bool is_open() const
        { 
            return _is_open;
        }
        const std::string& filename() const
        { 
            return _file_name;
        }
};

#endif // CSV_PARSER_LIB_PARSER_HH