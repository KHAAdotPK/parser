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
            Build Hash Table
            ----------------
            Iterates through the corpus and builds a hash table of unique tokens.
            Each unique token is assigned a unique token_number and stored in the hash table.
            The hash table is implemented as an array of pointers to WordRecord.
            The index_table stores the hash key for each token_number.
        */
        TABLES* build_hash_table(void)
        {
            size_t bucket_count = KEYS_COMMON_STARTING_SIZE;
            size_t bucket_used = 0;

            WordRecord** hash_table = nullptr;
            size_t* index_table = nullptr; 
            size_t line_number = 0;
            size_t token_number = 0;

            try
            {
                hash_table = new WordRecord*[bucket_count](); // Create array of pointers to WordRecord and return address of first element of the array
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
                        //token_number++; // Increment the token number, it does not count the redundency of the tokens/words on the corpus
                    }
                    else // If the bucket is not empty, it means the token/word is already present
                         // No check of the hash collision is done here, becuasse we are setting a threshold for the load factor.
                         // Note: The load factor only reduces collision probability, it does not eliminate it. 
                         // At any load factor, two different words can still hash to the same bucket. This is a data-loss bug, not a probability question.
                         //
                         // But even then if collision did not happen when the token/word was first inserted, it will not happen later either.
                         // Given that now the memory size is increased as well, the probability of collision is further reduced.
                    {
                        word_record = hash_table[key]; // Get the word record
                        // It is not possible that head ever can be null here
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

                    /*
                        Check if the hash table needs to be rehashed
                        Note: Integer division would truncate the result, so we cast to double
                    */
                    if (static_cast<double>(bucket_used) / static_cast<double>(bucket_count) > KEYS_LOAD_FACTOR_THRESHOLD)
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
                            new_index_table = new size_t[bucket_count]();
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
                                new_hash_table[new_key] = hash_table[i];

                                new_index_table[hash_table[i]->word_id] = new_key;
                            }

                            //new_index_table[i] = index_table[i];
                        }

                        delete[] hash_table;
                        delete[] index_table;

                        hash_table = new_hash_table;
                        index_table = new_index_table;
                    }

                    token_number++; // Increment the token number, these are tokens in this line, it does not count the redundency of the tokens/words on the corpus
                }

                token_number = 0; // Reset the token number for the next line
                line_number++;
            }
            
            /*
                Reset the line number, token number, number of buckets used, and bucket count
                This is done so that the hash table can be used again

                These should be made into local variables instead of member variables
            */
            line_number = 0; // Reset the line number
            token_number = 0; // Reset the token number
            //bucket_used = 0; // Reset the number of buckets used. This is mportant fo the TABLE composite
            //bucket_count = size_t(KEYS_COMMON_STARTING_SIZE); // Reset the bucket count

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