/*
    lib/src/Parser.hh

    Declaration of the Parser class used to read corpus input, scan lines and
    tokens, and collect structural statistics that support the downstream
    vocabulary and skip-gram pipeline.

    Maintainer: Sohail.
*/

#ifndef CSV_PARSER_LIB_PARSER_HH
#define CSV_PARSER_LIB_PARSER_HH

/*
    Include parser-level structures.
    Explicitly include headers where a file specifically relies on a type
    from another package. 
    it resolves the circular dependency successfully so the program compiles and works as expected.
 */
#include "./../../../Corpus/lib/src/Serialisation.hh"

class Parser
{
    std::string _ifile_name;
    std::ifstream _ifile;
    bool _is_open;

    size_t bucket_count;
    size_t bucket_used;

    size_t mxntpl; // max number of tokens per line, used to size the token array in each Line struct
    size_t mnntpl; // min number of tokens per line, used to size the token array in each Line struct

    size_t nol; // number of lines in the corpus, used to size the line array in the Corpus struct
    size_t tnt; // total number of tokens in the corpus, used to size the token array in the Corpus struct

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

     public:
        
        void free_tables(const size_t* index_table, WordRecord** hash_table, LINE* line_head, size_t bucket_used) 
        {   
            std::cout<< "BUCKET USED = " << bucket_used << std::endl;

            /*while (line_head != nullptr)
            {
                LINE* next_line = line_head->next;
                while (line_head->tokens != nullptr)
                {
                    TOKEN* next_token = line_head->tokens->next;
                    line_head->tokens->occurrence = nullptr;
                    //delete line_head->tokens;
                    line_head->tokens = next_token;
                }                
                delete line_head;
                line_head = next_line;
            }*/
            
            for (size_t i = 0; i < bucket_used; ++i)
            {                
                size_t key = index_table[i];
                WordRecord* w_rec = hash_table[key];

                if (w_rec != nullptr)
                {
                    OccurrenceNode* current = w_rec->head; 

                    while (current != nullptr)
                    {
                        OccurrenceNode* next = current->next;
                        delete current;
                        current = next;
                    }
                
                    w_rec->head = nullptr;
                    w_rec->n = 0;
                }
            }
            
            while (line_head != nullptr)
            {
                LINE* next_line = line_head->next;
                while (line_head->tokens != nullptr)
                {
                    TOKEN* next_token = line_head->tokens->next;
                    line_head->tokens->occurrence = nullptr;
                    //delete line_head->tokens;
                    line_head->tokens = next_token;
                }                
                delete line_head;
                line_head = next_line;
            }
        }

        static void free_tables_very_new(WordRecord_new** hash_table, WORDS** lines_array, size_t* index_table, INDEX_TABLE_FILE_HEADER iheader, LINES_TABLE_FILE_HEADER lheader)
         {
             // ==================== FREE HASH TABLE ====================
             // Only attempt to free if the pointer is not null.
             if (hash_table != nullptr)
             {
                // Get the number of buckets. This function is assumed to be safe
                // even if hash_table is null (returns 0). If it is not, we are in
                // undefined territory – but we've already checked null above.
                size_t bucket_count =  iheader.bc /*get_bucket_count()*/;

                // Iterate over each bucket and delete the WordRecord_new object.
                for (size_t i = 0; i < bucket_count; ++i)
                {
                    WordRecord_new* record = hash_table[i];
                    if (record != nullptr)
                    {
                        delete record;          // Destructor should clean up its own members
                        hash_table[i] = nullptr; // Avoid dangling pointer (defensive)
                    }
                }

                // Delete the array of pointers itself.
                delete[] hash_table;   // NOT delete – we allocated with new[]
                // The parameter is local; setting it to nullptr does not affect the caller's
                // pointer. We do it only to avoid accidental use inside this function.
                // Caller should still set their own pointer to nullptr after the call.
                hash_table = nullptr;
            }
            // else: hash_table is null, nothing to free.

            // ==================== FREE LINES ARRAY ====================
            if (lines_array != nullptr)
            {
                size_t num_lines = lheader.nol /*get_nol()*/; // Assumed safe even if lines_array is null

                for (size_t i = 0; i < num_lines; ++i)
                {
                    WORDS* line = lines_array[i];
                    if (line != nullptr)
                    {
                        // Free the internal 'keys' array if present.
                        if (line->keys != nullptr)
                        {
                            delete[] line->keys; // assuming keys was allocated with new[]
                            line->keys = nullptr;
                        }

                        delete line;          // Destructor should clean up other members
                        lines_array[i] = nullptr;
                    }
                }

                delete[] lines_array;   // array of pointers, so delete[]
                lines_array = nullptr;
            }

            // ==================== FREE INDEX TABLE ====================
            // index_table may be an array or a single size_t; we assume it was allocated
            // with new[] (common for arrays). If it was a single object, use delete.
            // We choose delete[] here (adjust if needed).
            if (index_table != nullptr)
            {
                delete[] index_table;   // or delete index_table; – caller must guarantee match
                index_table = nullptr;
            }
        }

        /**
         * @brief Frees all dynamically allocated memory associated with the hash table,
         *        the lines array, and the index table.
         *
         * @param hash_table  Pointer to an array of pointers to WordRecord_new objects.
         *                    This array must have been allocated with `new[]`.
         * @param lines_array Pointer to an array of pointers to WORDS objects.
         *                    This array must have been allocated with `new[]`.
         * @param index_table Pointer to an array of size_t (or a single size_t) that
         *                    may have been allocated with `new[]` or `new`.
         *                    The caller is responsible for ensuring the allocation
         *                    method matches the deallocation used here.
         *
         * @pre  hash_table, if not null, points to a valid array of bucket_count()
         *      pointers. lines_array, if not null, points to a valid array of nol()
         *      pointers. index_table, if not null, points to a valid memory block.
         * @post All pointed‑to objects are destroyed and memory is released.
         *       All pointers passed as arguments are set to nullptr (local effect only).
         *
         * @warning The sizes of the arrays are obtained via get_bucket_count() and
         *          get_nol(). These functions must be safe to call even when their
         *          corresponding table is nullptr (e.g., they return 0). 
         */
         void free_tables_very_new(WordRecord_new** hash_table, WORDS** lines_array, size_t* index_table)
         {
             // ==================== FREE HASH TABLE ====================
             // Only attempt to free if the pointer is not null.
             if (hash_table != nullptr)
             {
                // Get the number of buckets. This function is assumed to be safe
                // even if hash_table is null (returns 0). If it is not, we are in
                // undefined territory – but we've already checked null above.
                size_t bucket_count = get_bucket_count();

                // Iterate over each bucket and delete the WordRecord_new object.
                for (size_t i = 0; i < bucket_count; ++i)
                {
                    WordRecord_new* record = hash_table[i];
                    if (record != nullptr)
                    {
                        delete record;          // Destructor should clean up its own members
                        hash_table[i] = nullptr; // Avoid dangling pointer (defensive)
                    }
                }

                // Delete the array of pointers itself.
                delete[] hash_table;   // NOT delete – we allocated with new[]
                // The parameter is local; setting it to nullptr does not affect the caller's
                // pointer. We do it only to avoid accidental use inside this function.
                // Caller should still set their own pointer to nullptr after the call.
                hash_table = nullptr;
            }
            // else: hash_table is null, nothing to free.

            // ==================== FREE LINES ARRAY ====================
            if (lines_array != nullptr)
            {
                size_t num_lines = get_nol(); // Assumed safe even if lines_array is null

                for (size_t i = 0; i < num_lines; ++i)
                {
                    WORDS* line = lines_array[i];
                    if (line != nullptr)
                    {
                        // Free the internal 'keys' array if present.
                        if (line->keys != nullptr)
                        {
                            delete[] line->keys; // assuming keys was allocated with new[]
                            line->keys = nullptr;
                        }

                        delete line;          // Destructor should clean up other members
                        lines_array[i] = nullptr;
                    }
                }

                delete[] lines_array;   // array of pointers, so delete[]
                lines_array = nullptr;
            }

            // ==================== FREE INDEX TABLE ====================
            // index_table may be an array or a single size_t; we assume it was allocated
            // with new[] (common for arrays). If it was a single object, use delete.
            // We choose delete[] here (adjust if needed).
            if (index_table != nullptr)
            {
                delete[] index_table;   // or delete index_table; – caller must guarantee match
                index_table = nullptr;
            }
        }
    
        // Constructors
        Parser() : _ifile_name(), _ifile(), _is_open(false), bucket_count(0), bucket_used(0), mxntpl(0), mnntpl(std::numeric_limits<size_t>::max()), nol(0), tnt(0)  /*,bucket_count(size_t(KEYS_COMMON_STARTING_SIZE)), buckets_used(0),*/ /*hash_table(nullptr), index_table(nullptr), line_number(0), token_number(0)*/   
        {
        }

        explicit Parser(const std::string& iname/*, const std::string& oname*/) : _ifile_name(iname), _ifile(), _is_open(false), bucket_count(0), bucket_used(0), mxntpl(0), mnntpl(std::numeric_limits<size_t>::max()), nol(0), tnt(0) /*, bucket_count(size_t(KEYS_COMMON_STARTING_SIZE)), buckets_used(0),*/ /*hash_table(nullptr), index_table(nullptr), line_number(0), token_number(0)*/
        {
            _ifile.open(_ifile_name);

            if (_ifile.is_open())
            {
                _is_open = true;
            }
            else
            {
                throw std::runtime_error("Parser::Parser(const std::string&) Error: Could not open file " + _ifile_name);
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

        /*
            PLEASE NOTE:- 
            Transfers ownership of the file stream from the
            source object to the destination object. The source object is left
            in a valid but unspecified state (specifically, its _is_open flag is
            set to false and its stream is left in a state where it no longer
            owns the file). This is a noexcept operation because it only involves
            moving member variables and does not throw exceptions.
        */

        // Make Parser non-copyable but movable.
        Parser(const Parser&) = delete; // Copy Constructor, non-copyable due to ifstream 
        Parser& operator=(const Parser&) = delete; // Copy Assignment Operator, non-assignable due to ifstream 
        Parser(Parser&&) = default; // Move Constructor, move-constructible due to ifstream 
        Parser& operator=(Parser&&) = default; // Move Assignment Operator, move-assignable due to ifstream 

        // Copy Assignment Operator, Just for documentation purposes (Don't use it)
        // Just to show that composites with ifstream properties can be made copy-able (using the move semantics)
        /*
        Parser& operator=(const Parser& other) noexcept // Why noexcept? Because all operations here are noexcept. 
        {
            if (this != &other)
            {   
                // Streams are not copyable, but they are movable, make this implementation explocitly do moving of ownership of stream.
                // ...............................................----------------------------------------------------------------------

                // Transfers ownership of the file stream from the source object to the destination object.
                // This is a noexcept operation because it only involves moving member variables and does not throw exceptions.             
                _ifile_name = std::move(other._ifile_name);              
                _ifile = std::move(other._ifile);

                _is_open = other._is_open;
                bucket_count = other.bucket_count;
                bucket_used = other.bucket_used;
                mxntpl = other.mxntpl;
                mnntpl = other.mnntpl;
                nol = other.nol;
                tnt = other.tnt;
            }

            return *this;
        }
         */
        
        // Move Assignment Operator, Just for documentation purposes (Don't use it) default works fine.
        /*
        Parser& operator=(Parser&& other) noexcept // Why noexcept? Because all operations here are noexcept.
        {
            if (this != &other)
            {                                              
                // Streams are movable.
                // This is what std::move does (Move Constructor / Move Assignment Operator):
                // - The contents of 'other' are moved into 'this'.
                // - 'other' is left in a valid but unspecified state (usually empty or zero-initialized).    
                _ifile_name = std::move(other._ifile_name);
                _ifile = std::move(other._ifile);

                _is_open = _ifile.is_open();
                bucket_count = other.bucket_count;
                bucket_used = other.bucket_used;
                mxntpl = other.mxntpl;
                mnntpl = other.mnntpl;
                nol = other.nol;
                tnt = other.tnt;

                other._is_open = false;
            }

            return *this;
        }
         */

        void reset(void)
        {
            if (_is_open)
            {
                _ifile.clear(); // Clear any error flags
                _ifile.seekg(0); // Move to the beginning of the file            
            }
        }

        void close(void)
        {
            if (_is_open)
            {
                _ifile.close();
                _is_open = false;
            }
        }
     
        // Destructor – file closed automatically
        ~Parser() = default;

        /*
        // It is there just for debugging purposes. It should be removed
        WORDS** build_lines_table_very_new(const WordRecord_new* const *const hash_table)
        {
            for (auto& line : *this) // When you dereference the iterator (via `operator*()`), it returns a reference to its internal member `_current`, which is of type `std::vector<std::string>`
                                     // Therefore, the loop variable `line` is of type `std::vector<std::string>&
                                     // When you call `line.size()`, you are calling the standard library method `std::vector::size()` on the vector of tokens for the current line. It outputs the total count of parsed tokens on that line
            {

                for (size_t i = 0; i < line.size(); i++)
                {
                    std::cout<< line[i] << " ";
                }
                std::cout<< std::endl;
            }

            return nullptr; // Placeholder implementation
        }
         */    

        /*
        void descending_quick_sort_index_table(size_t* index_table, size_t bucket_used, WordRecord_new** hash_table)
        {            
            if (bucket_used <= 1)
            {
                return; // Base case: already sorted
            }

            size_t pivot_index = index_table[(TOKEN_ID_ORIGINATE_AT_VALUE + bucket_used) / 2];
            WordRecord_new* pivot_record = hash_table[pivot_index];

            size_t i = TOKEN_ID_ORIGINATE_AT_VALUE;
            size_t j = bucket_used + TOKEN_ID_ORIGINATE_AT_VALUE - 1;

            while (i <= j)
            {
                while (hash_table[index_table[i]]->get_n() > pivot_record->get_n())
                {
                    i++;
                }
                while (hash_table[index_table[j]]->get_n() < pivot_record->get_n())
                {
                    j--;
                }
                if (i <= j)
                {
                    std::swap(index_table[i], index_table[j]);
                    i++;
                    j--;
                }
            }

            std::cout<< "Passed \n";

            if (j > 0)
            {
                descending_quick_sort_index_table(index_table, j + 1, hash_table);
            }

            descending_quick_sort_index_table(index_table + i, bucket_used - i, hash_table);    
        }
         */

        /*
         * @brief Sorts a segment of the index table in descending order of word frequency
         *        and updates each word's ID to reflect its new sorted position.
         *
         * This function operates on a contiguous sub-range of `index_table` that
         * corresponds to the active (populated) entries in a hash table. The range
         * begins at a fixed offset (`TOKEN_ID_ORIGINATE_AT_VALUE`) and spans exactly
         * `bucket_used` elements. It reorders these entries so that the word with the
         * highest frequency (`n` in `WordRecord_new`) appears first, and then assigns
         * a new `word_id` to each record equal to its absolute index in `index_table`
         * after sorting.
         *
         * @param index_table   Pointer to an array of `size_t` elements. Each element
         *                      is a key (index) into `hash_table` pointing to a
         *                      `WordRecord_new` object. The array must be large enough
         *                      to hold at least `TOKEN_ID_ORIGINATE_AT_VALUE + bucket_used`
         *                      entries.
         * @param bucket_used   Number of valid/active entries in the hash table that
         *                      are to be sorted. This determines the size of the
         *                      sub‑range to be processed.
         * @param hash_table    Pointer to an array of `WordRecord_new*` pointers.
         *                      This table is indexed by the values stored in
         *                      `index_table` to retrieve the corresponding word records.
         *
         * @pre `index_table` is non-null and contains valid keys for the range
         *      [TOKEN_ID_ORIGINATE_AT_VALUE, TOKEN_ID_ORIGINATE_AT_VALUE + bucket_used).
         * @pre `hash_table` is non-null and each key in the above range points to a
         *      valid non-null `WordRecord_new` object.
         * @pre `TOKEN_ID_ORIGINATE_AT_VALUE` is a compile‑time constant that defines
         *      the starting offset for the active portion of `index_table`.
         *
         * @post The sub‑range [TOKEN_ID_ORIGINATE_AT_VALUE,
         *       TOKEN_ID_ORIGINATE_AT_VALUE + bucket_used) of `index_table` is sorted
         *       in **descending** order of `WordRecord_new::get_n()` (frequency count).
         * @post For every `i` in that sub‑range, the `word_id` member of the record
         *       pointed to by `hash_table[index_table[i]]` is set to `i`. This makes
         *       `word_id` equal to the record’s final position in the sorted table.
         *
         * @par Complexity
         *      Sorting is performed by `std::sort` with an average‑case O(N log N)
         *      time complexity, where N = `bucket_used`. The post‑processing update
         *      pass runs in O(N). The total time is O(N log N) and the function uses
         *      O(1) additional space (besides the input arrays).
         *
         * @return void
         */
        void descending_sort_index_table(size_t* index_table, size_t bucket_used, WordRecord_new** hash_table)
        {
            // 1. Calculate boundaries based on your offset token ID
            size_t start_idx = TOKEN_ID_ORIGINATE_AT_VALUE;
            size_t end_idx = bucket_used + TOKEN_ID_ORIGINATE_AT_VALUE;

            // 2. Sort the populated portion of the `index_table` in descending order
            //    by the frequency `n` stored in the corresponding `WordRecord_new`.
            //
            // Explanation of the arguments passed to `std::sort`:
            //  - `index_table + start_idx` is a pointer/iterator to the first element
            //    of the sub-range we want to sort. Here `start_idx` equals
            //    `TOKEN_ID_ORIGINATE_AT_VALUE` and is inclusive.
            //  - `index_table + end_idx` is a pointer/iterator one past the last
            //    element to sort. `end_idx` is computed as `bucket_used +
            //    TOKEN_ID_ORIGINATE_AT_VALUE` and is exclusive (standard C++ range
            //    convention). Together these two form the half-open range
            //    [first, last) that `std::sort` will reorder in-place.
            //
            // What is stored inside `index_table`?
            //  - `index_table` is an array of `size_t` values. Each element is a
            //    hash key (an index into `hash_table`) that locates a
            //    `WordRecord_new*` in `hash_table`.
            //
            // What does the comparator receive?
            //  - The comparator lambda receives *elements* from the array — not
            //    the positions. In this case its parameters `a` and `b` are the
            //    `size_t` values stored in `index_table` (i.e. hash keys). They
            //    are not the array indices themselves, but the values pointed to by
            //    `index_table[i]`.
            //
            // Therefore inside the lambda we treat `a` and `b` as keys into
            // `hash_table` and fetch the corresponding `WordRecord_new*` objects
            // via `hash_table[a]` and `hash_table[b]`.
            //
            // Comparator contract and ordering:
            //  - The comparator returns `true` when the first element should
            //    appear before the second. Returning `w_rec1->get_n() >
            //    w_rec2->get_n()` produces a descending sort by frequency.
            //  - The comparator must impose a strict weak ordering. Using `>` on
            //    integer counts satisfies this requirement.
            //
            // Complexity note:
            //  - `std::sort` provides average-case O(N log N) time complexity
            //    and performs the sort in-place. The range is half-open, so
            //    ensure `end_idx` points one past the last element to include.
            std::sort(index_table + start_idx, index_table + end_idx,
                      [hash_table](size_t a, size_t b) {
                          // `a` and `b` are hash keys stored in `index_table`.
                          WordRecord_new* w_rec1 = hash_table[a];
                          WordRecord_new* w_rec2 = hash_table[b];

                          // Descending order by occurrence count (`n`).
                          return w_rec1->get_n() > w_rec2->get_n();
                      });

            // 3. Post-processing Pass: Update word_ids to reflect their exact final position
            // This replaces all your inner-loop incremental updates with a single O(N) pass.
            for (size_t i = start_idx; i < end_idx; i++) 
            {
                WordRecord_new* w_rec = hash_table[index_table[i]];
                if (w_rec != nullptr) 
                {
                    w_rec->word_id = i; 
                }
            }            
        }

        /*
         * @brief Sorts a segment of the index table in descending order of word frequency
         *        using the bubble sort algorithm, updating each word's ID incrementally
         *        during each swap.
         *
         * This function is an alternative to the `descending_sort_index_table` function
         * (which uses `std::sort`). It performs an in‑place bubble sort on the active
         * sub‑range of `index_table` (from `TOKEN_ID_ORIGINATE_AT_VALUE` to
         * `TOKEN_ID_ORIGINATE_AT_VALUE + bucket_used - 1`). After each swap of two
         * adjacent keys, the `word_id` members of the corresponding `WordRecord_new`
         * objects are immediately updated to reflect their new positions. This avoids
         * a separate post‑processing pass, but comes at a significant performance cost.
         *
         * @warning This function has O(N²) worst‑case time complexity, where N =
         *          `bucket_used`. For large datasets, it will be substantially slower
         *          than the `std::sort`‑based version. The latter is strongly preferred
         *          for performance‑critical code. This implementation is provided
         *          mainly for educational or legacy purposes.
         *
         * @param index_table   Pointer to an array of `size_t` elements. Each element
         *                      is a key (index) into `hash_table` pointing to a
         *                      `WordRecord_new` object. The array must be large enough
         *                      to hold at least `TOKEN_ID_ORIGINATE_AT_VALUE + bucket_used`
         *                      entries.
         * @param bucket_used   Number of valid/active entries in the hash table that
         *                      are to be sorted. This determines the size of the
         *                      sub‑range to be processed.
         * @param hash_table    Pointer to an array of `WordRecord_new*` pointers.
         *                      This table is indexed by the values stored in
         *                      `index_table` to retrieve the corresponding word records.
         *
         * @pre `index_table` is non-null and contains valid keys for the range
         *      [TOKEN_ID_ORIGINATE_AT_VALUE, TOKEN_ID_ORIGINATE_AT_VALUE + bucket_used).
         * @pre `hash_table` is non-null and each key in the above range points to a
         *      valid non-null `WordRecord_new` object.
         * @pre `TOKEN_ID_ORIGINATE_AT_VALUE` is a compile‑time constant that defines
         *      the starting offset for the active portion of `index_table`.
         *
         * @post The sub‑range [TOKEN_ID_ORIGINATE_AT_VALUE,
         *       TOKEN_ID_ORIGINATE_AT_VALUE + bucket_used) of `index_table` is sorted
         *       in **descending** order of `WordRecord_new::get_n()` (frequency count).
         * @post For every `i` in that sub‑range, the `word_id` member of the record
         *       pointed to by `hash_table[index_table[i]]` is equal to `i`. This is
         *       maintained continuously throughout the sort, so no separate update
         *       pass is needed.
         *
         * @par Complexity
         *      Bubble sort has O(N²) comparisons and swaps in the worst and average
         *      cases, where N = `bucket_used`. Best‑case (already sorted) is O(N) due
         *      to the early‑exit flag, but the worst‑case makes this implementation
         *      unsuitable for large input sizes. The `std::sort`‑based function is
         *      recommended for production use because it provides O(N log N)
         *      performance.
         *
         * @see descending_sort_index_table – the faster, O(N log N) alternative.
         *
         * @return void
         */
        void descending_bubble_sort_index_table(size_t* index_table,  size_t bucket_used, WordRecord_new** hash_table)
        {
             /*std::cout<< "Hash at 0 = " << index_table[0] << std::endl;
             std::cout<< "Hash at 1 = " << index_table[1] << std::endl;
             
             std::cout<< "Hash at 3 = " << index_table[3] << std::endl;*/

             for (size_t i = TOKEN_ID_ORIGINATE_AT_VALUE; i < bucket_used + TOKEN_ID_ORIGINATE_AT_VALUE - 1; i++) // Outer loop for number of passes
             {

                bool swapped = false;

                for (size_t j = TOKEN_ID_ORIGINATE_AT_VALUE; j < bucket_used + TOKEN_ID_ORIGINATE_AT_VALUE - (i - TOKEN_ID_ORIGINATE_AT_VALUE) - 1; j++) // Inner loop for comparing adjacent elements
                {
                    WordRecord_new* w_rec1 = hash_table[index_table[j]];
                    WordRecord_new* w_rec2 = hash_table[index_table[j + 1]];

                    //std::cout<< "Happened, "; 
                    
                    /*if (w_rec1->get_word() > w_rec2->get_word()) // Compare the words lexicographically
                    {
                        std::swap(index_table[j], index_table[j + 1]); // Swap the indices in the index table
                    }*/

                    if (w_rec2->get_n() > w_rec1->get_n()) // Compare the word IDs
                    {   
                        /*
                            Update the word IDs of the WordRecord_new objects to reflect their new positions in the index table                    
                            This upddate is necessay to maintain consistency with their new positions in the index table
                            The word ID of the first WordRecord_new is updated to j + 1 because it is being swapped with the second WordRecord_new, which is at index j + 1 in the index table. After the swap, the first WordRecord_new will occupy the position of the second WordRecord_new, hence its new word ID should be j + 1.
                         */

                        // The first WordRecord_new's word ID is updated to j + 1 because it is being swapped with the second WordRecord_new, which is at index j + 1 in the index table. After the swap, the first WordRecord_new will occupy the position of the second WordRecord_new, hence its new word ID should be j + 1.
                        w_rec1->word_id = j + 1; // Update the word ID of
                        // The Secoond WordRecord_new to its new position
                        w_rec2->word_id = j; // Update the word ID of

                        // Swap the indices in the index table to reflect the new order
                        std::swap(index_table[j], index_table[j + 1]); 
                        swapped = true; // Mark that a swap occurred
                    }
                }

                if (!swapped) // Check if swap pccured in the inner loop
                {
                    break; // If no swaps occurred, the array is already sorted
                }

                std::cout<< " Pass " << i  - TOKEN_ID_ORIGINATE_AT_VALUE << " completed." << std::endl << std::endl;
             }
        }

        /**
         * @brief Serializes the vocabulary hash table to a binary file.
         *
         * Writes the complete hash table structure to disk in binary format, including
         * header metadata and all non-empty bucket entries. This is used to persist
         * the vocabulary built during corpus parsing for later retrieval and skip-gram
         * pair generation.
         *
         * File layout:
         *   1. INDEX_TABLE_FILE_HEADER  -> containing tioav (total_invalid_outside_ascii_value), 
         *                                   bu (buckets_used), and bc (bucket_count).
         *   2. For each bucket in the hash table (up to header.bc):
         *        a. size_t word_id (0 if empty/nullptr, otherwise the word's unique ID).
         *        b. size_t word_length (only if word_id != 0).
         *        c. char word_data[word_length] (the actual word string, only if word_length > 0).
         *        d. size_t occurrence_count (the frequency count, only if word_id != 0).
         *
         * @param hash_table   Pointer to an array of pointers to WordRecord_new objects.
         *                     Each non-null entry represents a word in the vocabulary.
         * @param header       The INDEX_TABLE_FILE_HEADER containing metadata: total tokens,
         *                     number of buckets used, and total bucket capacity.
         * @param ofile_name   Full path to the binary output file for writing serialized data.
         *
         * @throws std::runtime_error if:
         *         - hash_table is nullptr while metadata reports non-zero buckets
         *         - output file cannot be opened for writing
         *         - binary write operations fail at any stage (header, word_id, length, word data, counts)
         *         - file cannot be flushed or closed
         *
         * @note   This method is const and performs all necessary validation on input parameters
         *         before proceeding with file I/O operations.
         */
        void save_hash_table(const WordRecord_new* const* hash_table, const INDEX_TABLE_FILE_HEADER& header, const std::string ofile_name) const
        {
            if (hash_table == nullptr && (header.bu != 0 || header.bc != 0))
            {
                throw std::runtime_error("Parser::save_hash_table(const WordRecord_new* const*, const INDEX_TABLE_FILE_HEADER&, const std::string) Error: hash table pointer is null while parser reports non-zero buckets count");
            }
            
            std::ofstream ofile(ofile_name, std::ios::out | std::ios::binary);
            if (!ofile.is_open())
            {
                throw std::runtime_error("Parser::save_hash_table(const WordRecord_new* const*, const size_t*, const INDEX_TABLE_FILE_HEADER&, const std::string) Error: failed to open output file for writing");
            }

            ofile.write(reinterpret_cast<const char*>(&header), sizeof(INDEX_TABLE_FILE_HEADER));
            if (!ofile)
            {
                throw std::runtime_error("Parser::save_hash_table(const WordRecord_new* const*, const INDEX_TABLE_FILE_HEADER&, const std::string) Error: failed to write header to output file");
            }

            for (size_t i = 0; i < header.bc; i++)
            {
                if (hash_table[i] != nullptr)                
                {
                    ofile.write(reinterpret_cast<const char*>(&hash_table[i]->word_id), sizeof(size_t));
                    if (!ofile)
                    {
                        throw std::runtime_error("Parser::save_hash_table(const WordRecord_new* const*, const INDEX_TABLE_FILE_HEADER&, const std::string) Error: failed to write word_id to output file");
                    }                    
                    
                    size_t length = hash_table[i]->word.length();

                    ofile.write(reinterpret_cast<const char*>(&length), sizeof(size_t));
                    if (!ofile)
                    {
                        throw std::runtime_error("Parser::save_hash_table(const WordRecord_new* const*, const INDEX_TABLE_FILE_HEADER&, const std::string) Error: failed to write length to output file");
                    }

                    ofile.write(reinterpret_cast<const char*>(hash_table[i]->word.c_str()), hash_table[i]->word.length());
                    if (!ofile)
                    {
                        throw std::runtime_error("Parser::save_hash_table(const WordRecord_new* const*, const INDEX_TABLE_FILE_HEADER&, const std::string) Error: failed to write word_id to output file");
                    }
                                        
                    ofile.write(reinterpret_cast<const char*>(&hash_table[i]->n), sizeof(size_t));
                    if (!ofile)
                    {
                        throw std::runtime_error("Parser::save_hash_table(const WordRecord_new* const*, const INDEX_TABLE_FILE_HEADER&, const std::string) Error: failed to write n to output file");
                    }
                }
                else
                {
                    size_t word_id = 0;
                    ofile.write(reinterpret_cast<const char*>(&word_id), sizeof(size_t));
                    if (!ofile)
                    {
                        throw std::runtime_error("Parser::save_hash_table(const WordRecord_new* const*, const INDEX_TABLE_FILE_HEADER&, const std::string) Error: failed to write word_id to output file, for hash_table[i] == nullptr");
                    }
                }
            }

            ofile.flush();
            if (!ofile)
            {
                throw std::runtime_error("Parser::save_hash_table(const WordRecord_new* const*, const INDEX_TABLE_FILE_HEADER&, const std::string) Error: failed to flush output file");
            }

            ofile.close();
            if (!ofile)
            {
                throw std::runtime_error("Parser::save_hash_table(const WordRecord_new* const*, const INDEX_TABLE_FILE_HEADER&, const std::string) Error: failed to close output file");
            }
        }

        /**
         * @brief Deserializes a vocabulary hash table from a binary file.
         *
         * Reconstructs the hash table structure from a binary file previously written by
         * save_hash_table(). Reads the header metadata, allocates the necessary hash table
         * array, and populates it with WordRecord_new objects for each vocabulary entry.
         * This is used to load a previously serialized vocabulary for skip-gram pair
         * generation and other downstream processing.
         *
         * File layout (expected format):
         *   1. INDEX_TABLE_FILE_HEADER  -> containing tioav (total_invalid_outside_ascii_value), 
         *                                   bu (buckets_used), and bc (bucket_count).
         *   2. For each bucket in the hash table (up to header.bc):
         *        a. size_t word_id (0 if empty/nullptr, otherwise the word's unique ID).
         *        b. size_t word_length (only if word_id != 0).
         *        c. char word_data[word_length] (the actual word string, only if word_length > 0).
         *        d. size_t occurrence_count (the frequency count, only if word_id != 0).
         *
         * @param hash_table   Pointer to a pointer to an array of WordRecord_new objects.
         *                     On successful return, *hash_table will point to a dynamically
         *                     allocated array of size header.bc (or remain nullptr if header.bc == 0).
         * @param ifile_name   Full path to the binary input file containing serialized hash table data.
         *
         * @return INDEX_TABLE_FILE_HEADER containing metadata: tioav, bu (buckets_used),
         *         and bc (bucket_count). Caller is responsible for using these values
         *         for validation and to know how many buckets were allocated.
         *
         * @throws std::runtime_error if:
         *         - input file cannot be opened for reading
         *         - binary read operations fail at any stage (header, word_id, length, word, counts)
         *         - memory allocation fails for the hash table array or WordRecord_new objects
         *
         * @note   Caller must deallocate the allocated hash table and all WordRecord_new
         *         objects when no longer needed. If header.bc == 0, *hash_table remains nullptr.
         */
        static INDEX_TABLE_FILE_HEADER load_hash_table(WordRecord_new*** hash_table, const std::string ifile_name)
        {            
            std::ifstream ifile(ifile_name, std::ios::in | std::ios::binary);
            if (!ifile.is_open())
            {
                throw std::runtime_error("Parser::load_index_table(const std::string&) Error: failed to open file for reading");
            }

            INDEX_TABLE_FILE_HEADER header = {0, 0, 0};

            // Blindly assigning nullptr
            *hash_table = nullptr;

            ifile.read(reinterpret_cast<char*>(&header), sizeof(header));
            if (!ifile)
            {
                throw std::runtime_error("Parser::load_hash_table(const std::string ifile_name) Error: failed to read the header");
            }
                        
            if (header.bc != 0)
            {
                *hash_table = new (std::nothrow) WordRecord_new*[header.bc];
                if (*hash_table == nullptr)
                {
                    throw std::runtime_error("Parser::load_hash_table(const std::string ifile_name) Error: allocation failed for hash table");
                }
            }            
            // If bc == 0, hash_table remains nullptr, and we *don't* throw.

            for (size_t i = 0; i < header.bc; i++)
            {
                size_t word_id = 0;
                ifile.read(reinterpret_cast<char*>(&word_id), sizeof(size_t));
                if (!ifile)
                {
                    throw std::runtime_error("Parser::load_hash_table(const std::string ifile_name) Error: failed to read the word_id");
                }
                
                if (word_id == 0)
                {
                    hash_table[0][i] = nullptr;
                }
                else
                {
                    size_t length = 0;
                    ifile.read(reinterpret_cast<char*>(&length), sizeof(size_t));
                    if (!ifile)
                    {
                        throw std::runtime_error("Parser::load_hash_table(const std::string ifile_name) Error: failed to read the length");
                    }
                    
                    std::string word(length, '\0');
                    ifile.read(reinterpret_cast<char*>(&word[0]), length);
                    if (!ifile)
                    {
                        throw std::runtime_error("Parser::load_hash_table(const std::string ifile_name) Error: failed to read the word");
                    }
                    
                    size_t n = 0;
                    ifile.read(reinterpret_cast<char*>(&n), sizeof(size_t));
                    if (!ifile)
                    {
                        throw std::runtime_error("Parser::load_hash_table(const std::string ifile_name) Error: failed to read the n");
                    }
                    
                    hash_table[0][i] = new (std::nothrow) WordRecord_new(word_id, word, n);
                    if (hash_table[0][i] == nullptr)
                    {
                        throw std::runtime_error("Parser::load_hash_table(const std::string ifile_name) Error: allocation failed for word record");
                    }
                }
            }
                
            return header;
        }

        /**
         * @brief Saves an index table to a binary file, along with a header containing
         *        metadata from the current parser state.
         *
         * The method writes a fixed header (`INDEX_TABLE_FILE_HEADER`) followed by the
         * raw binary data of the index table. The header includes the current bucket
         * count (`bucket_used`) and a fixed origin token ID. The file is flushed and
         * closed; any failure at any step results in an exception.
         *
         * @param parser      A reference to a `Parser` object (currently unused, but
         *                    retained for interface consistency).
         * @param index_table Pointer to the `size_t` array containing the index table
         *                    to be saved. If `index_table` is `nullptr` while the
         *                    parser reports a non-zero bucket count, an exception is
         *                    thrown.
         * @param ofile_name  Path to the output binary file to be created or overwritten.
         *
         * @throw std::runtime_error if any of the following conditions are met:
         *         - `index_table` is `nullptr` but the current bucket count is non-zero.
         *         - The output file cannot be opened for writing.
         *         - Writing the header or the table data fails.
         *         - Flushing the output stream fails.
         *         - Closing the file fails (checked via `is_open()` after `close()`).
         *
         * @note The file format is binary and platform-dependent. It uses the native
         *       memory representation of `size_t` and the header struct.
         * @warning The method assumes the `index_table` pointer is valid and points to
         *          at least `bucket_used` elements when the bucket count is non-zero.
         *          Passing a null pointer with a zero bucket count is allowed (no data
         *          is written beyond the header).
         * @note The `parser` parameter is currently not used; it may be reserved for
         *       future use or for maintaining a consistent API.
         */
        void save_index_table(const Parser& parser, size_t const * index_table, const std::string& ofile_name) const
        {
            if (index_table == nullptr && get_bucket_used() != 0)
            {
                throw std::runtime_error("Parser::save_index_table(const Parser&, size_t const *, const std::string&) Error: index table pointer is null while parser reports non-zero buckets count");
            }   
            
            std::ofstream ofile(ofile_name, std::ios::out | std::ios::binary);
            if (!ofile.is_open())
            {
                throw std::runtime_error("Parser::save_index_table(const Parser&, size_t const *, const std::string&) Error: failed to open file for writing");
            }

            INDEX_TABLE_FILE_HEADER header = {TOKEN_ID_ORIGINATE_AT_VALUE, get_bucket_used(), get_bucket_count()};

            ofile.write(reinterpret_cast<const char*>(&header), sizeof(header));
            if (!ofile)
            {
                throw std::runtime_error("Parser::save_index_table(const Parser&, size_t const *, const std::string&) Error: failed to write header");
            }

            // Save the index table
            //ofile.write(reinterpret_cast<const char*>(index_table), (bucket_used + header.tioav)*sizeof(size_t)); // For documentation
            ofile.write(reinterpret_cast<const char*>(index_table), (bucket_count + TOKEN_ID_ORIGINATE_AT_VALUE)*sizeof(size_t)); // For consistancy
            if (!ofile)
            {
                throw std::runtime_error("Parser::save_index_table(const Parser&, size_t const *, const std::string&) Error: failed to write index table");
            }

            ofile.flush();
            if (!ofile)
            {
                throw std::runtime_error("Parser::save_index_table(const Parser&, size_t const *, const std::string&) Error: failed to flush file");
            }

            ofile.close();
            if (ofile.is_open())
            {
                throw std::runtime_error("Parser::save_lines_table(Parser&, size_t const *, const std::string&) Error: failed to close file");
            }
        }

        /**
         * @brief Loads an index table from a binary file into dynamically allocated memory.
         *
         * This method opens the specified binary file, reads its header to determine
         * the number of elements, allocates a raw array of `size_t` on the heap, and
         * then reads the table data directly into that buffer. The caller is responsible
         * for deallocating the allocated memory using `delete[]`.
         *
         * @param index_table  [out] Pointer to a pointer that will be set to the
         *                     address of the newly allocated array. Must not be null.
         * @param ifile_name   The path to the binary index file to be read.
         *
         * @return INDEX_TABLE_FILE_HEADER The header structure read from the file,
         *         containing metadata about the loaded table.
         *
         * @throw std::runtime_error If the file cannot be opened, the header or data
         *         cannot be read, memory allocation fails, or the file cannot be closed.
         *         In the event of a data-read failure, the allocated memory is freed
         *         automatically before the exception is propagated.
         *
         * @note The file format is binary and platform-dependent; it assumes the same
         *       endianness and `size_t` representation as the running system.
         * @warning The caller takes ownership of the allocated buffer and must free it
         *          using `delete[]` when no longer needed.
         */
        static INDEX_TABLE_FILE_HEADER load_index_table(size_t** index_table, const std::string& ifile_name)
        {
            std::ifstream ifile(ifile_name, std::ios::in | std::ios::binary);
            if (!ifile.is_open())
            {
                throw std::runtime_error("Parser::load_index_table(size_t**, const std::string&) Error: failed to open file for reading");
            }
                        
            INDEX_TABLE_FILE_HEADER header = {0, 0, 0};

            ifile.read(reinterpret_cast<char*>(&header), sizeof(header));
            if (!ifile)
            {
                throw std::runtime_error("Parser::load_index_table(size_t**, const std::string&) Error: failed to read the header");
            }

            //*index_table = new (std::nothrow) size_t[header.bu + header.tioav]();
            *index_table = new (std::nothrow) size_t[header.bc + header.tioav]();
            if (*index_table == nullptr)
            {
                throw std::runtime_error("Parser::load_index_table(size_t**, const std::string&) Error: allocation failed for index table");   
            }

            //ifile.read(reinterpret_cast<char*>(*index_table), sizeof(size_t)*(header.bu + header.tioav));
            ifile.read(reinterpret_cast<char*>(*index_table), sizeof(size_t)*(header.bc + header.tioav));
            if (!ifile)
            {
                delete[] *index_table;
                *index_table = nullptr;
                throw std::runtime_error("Parser::load_index_table(size_t**, const std::string&) Error: failed to read the index table");
            } 

            ifile.close();
            if (ifile.is_open())
            {
                throw std::runtime_error("Parser::load_index_table(size_t**, const std::string&) Error: failed to close file");
            }

            return header;
        }
        
        /**
         * @brief Serialises the parsed line table to a compact binary file.
         *
         * File layout:
         *   1. LINES_TABLE_FILE_HEADER.nol  -> total number of lines in the table.
         *   2. For each line in order:
         *        a. size_t line_length = line->n
         *        b. size_t keys[line_length] = line->keys[0..line_length-1]
         *
         * This allows the loader to rebuild an array of WORDS records without
         * storing any additional metadata. The format is intentionally simple and
         * is expected to be read back by the matching load_lines_table() routine.
         *
         * @param parser Reference to the parser instance whose metadata is used to
         *               validate the table shape before writing.
         * @param lines_array Pointer to the array of WORDS rows to serialise.
         * @param ofile_name Output file path for the binary table.
         *
         * @throws std::runtime_error If the output file cannot be opened, the
         *         pointer table is invalid, a line is null, a key array is null,
         *         or any write operation fails.
         */
        void save_lines_table(const Parser& parser, const WORDS* const* const lines_array, const std::string& ofile_name)
        {
            if (lines_array == nullptr && get_nol() != 0)
            {
                throw std::runtime_error("Parser::save_lines_table(const Parser&, const WORDS* const*, const std::string&) Error: lines table pointer is null while parser reports non-zero line count");
            }

            std::ofstream ofile(ofile_name, std::ios::out | std::ios::binary);
            if (!ofile.is_open())
            {
                throw std::runtime_error("Parser::save_lines_table(const Parser&, const WORDS* const* const, const std::string&) Error: failed to open file for writing");
            }

            LINES_TABLE_FILE_HEADER header = { get_nol() };

            ofile.write(reinterpret_cast<const char*>(&header.nol), sizeof(header.nol));
            if (!ofile)
            {
                throw std::runtime_error("Parser::save_lines_table(const Parser&, const WORDS* const* const, const std::string&) Error: failed to write header");
            }

            for (size_t i = 0; i < header.nol; i++)
            {
                const WORDS* line = lines_array[i];
                if (line == nullptr)
                {
                    throw std::runtime_error(std::string("Parser::save_lines_table(const Parser&, const WORDS* const* const, const std::string&) Error: null WORDS at lines_array[") + std::to_string(i) + "]");
                }

                if (line->keys == nullptr)
                {
                    throw std::runtime_error(std::string("Parser::save_lines_table(const Parser&, const WORDS* const* const, const std::string&) Error: null keys at lines_array[") + std::to_string(i) + "]");
                }

                ofile.write(reinterpret_cast<const char*>(&line->n), sizeof(size_t));
                if (!ofile)
                {
                    throw std::runtime_error(std::string("Parser::save_lines_table(const Parser&, const WORDS* const* const, const std::string&) Error: failed to write line length at index ") + std::to_string(i));
                }

                for (size_t j = 0; j < line->n; j++)
                {
                    ofile.write(reinterpret_cast<const char*>(&line->keys[j]), sizeof(size_t));
                    if (!ofile)
                    {
                        throw std::runtime_error(std::string("Parser::save_lines_table(const Parser&, const WORDS* const* const, const std::string&) Error: failed to write key at lines_array[") + std::to_string(i) + "][" + std::to_string(j) + "]");
                    }
                }

                if (i % CORPUS_SERIALIZATION_CHECKPOINT_INTERVAL == 0)
                {
                    std::cout<< ".";
                }
            }

            ofile.flush();
            if (!ofile)
            {
                throw std::runtime_error("Parser::save_lines_table(const Parser&, const WORDS* const* const, const std::string&) Error: failed to flush file");
            }

            ofile.close();
            if (ofile.is_open())
            {
                throw std::runtime_error("Parser::save_lines_table(Parser&, const WORDS* const* const, const std::string&) Error: failed to close file");
            }
        }

        /**
         * @brief Deserialises a previously saved binary line table into memory.
         *
         * Expected file format:
         *   1. LINES_TABLE_FILE_HEADER.nol  -> number of rows in the table.
         *   2. For each line:
         *        a. size_t line_length
         *        b. size_t keys[line_length]
         *
         * Each line is reconstructed as a fresh WORDS object and appended to a
         * dynamically allocated array of WORDS pointers. The returned value is the
         * number of lines restored, matching the file header.
         *
         * @param lines_array Output pointer that receives the newly allocated array
         *                   of WORDS pointers.
         * @param ifile_name Input binary file path.
         *
         * @return The number of lines loaded from the file.
         *
         * @throws std::runtime_error If the file cannot be opened, the header or
         *         any line payload is truncated, memory allocation fails, or the
         *         stream cannot be closed cleanly.
         */
        static LINES_TABLE_FILE_HEADER load_lines_table(WORDS*** lines_array, const std::string& ifile_name)
        {
            std::ifstream ifile(ifile_name, std::ios::in | std::ios::binary);
            if (!ifile.is_open())
            {
                throw std::runtime_error("Parser::load_lines_table(WORDS***, const std::string&) Error: failed to open file for reading");
            }  
            
            LINES_TABLE_FILE_HEADER header = { 0 };

            ifile.read(reinterpret_cast<char*>(&header.nol), sizeof(header.nol));
            if (!ifile)
            {
                throw std::runtime_error("Parser::load_lines_table(WORDS***, const std::string&) Error: failed to read the header");
            }

            try
            {
                *lines_array = new WORDS*[header.nol];
            }
            catch (const std::bad_alloc& e)
            {
                *lines_array = nullptr;
                throw std::runtime_error("Parser::load_lines_table(WORDS***, const std::string&) Error: " + std::string(e.what()));
            }
            
            for (size_t i = 0; i < header.nol; i++)
            {
                WORDS* line = new WORDS();
                if (line == nullptr)
                {   
                    throw std::runtime_error("Parser::load_lines_table(WORDS***, const std::string&) Error: null WORDS at lines_array[" + std::to_string(i) + "]");
                }

                ifile.read(reinterpret_cast<char*>(&line->n), sizeof(line->n));
                if (!ifile.is_open())
                {
                    delete line;
                    throw std::runtime_error("Parser::load_lines_table(WORDS***, const std::string&) Error: failed to read from file");
                }

                
                line->keys = new (std::nothrow) size_t[line->n];
                if (line->keys == nullptr)
                {
                    delete line;
                    throw std::runtime_error("Parser::load_lines_table(WORDS***, const std::string&) Error: allocation failed for keys");
                }
                                                                
                for ( size_t j = 0; j < line->n; j++)
                {
                    ifile.read(reinterpret_cast<char*>(&line->keys[j]), sizeof(size_t));
                    if (!ifile)
                    {
                        throw std::runtime_error("Parser::load_lines_table(WORDS***, const std::string&) Error: truncated line data, keys are not completely built");
                    }
                }

                *(*lines_array + i) = line;
            }

            ifile.close();
            if (ifile.is_open())
            {
                throw std::runtime_error("Parser::load_lines_table(WORDS***, const std::string&) Error: failed to close file");
            }
            
            return header;
        }   

        /*
         * Dynamically builds a flat, cache-friendly table of lines and their tokens.
         * 
         * DESIGN CHOICE:
         * This method replaces the older linked-list based build_lines() approach.
         * Instead of allocating separate doubly-linked nodes for every line and token (which
         * causes severe heap fragmentation and cache misses), it allocates a contiguous
         * array of size_t keys for each line. 
         * 
         * BENEFITS:
         * - Reduces dynamic memory allocations (from O(N_lines + N_tokens) to O(N_lines)).
         * - Saves massive memory overhead by eliminating linked list pointers (next/prev).
         * - Drastically improves CPU cache locality during training loops because token keys
         *   for each line are contiguous in memory.
         * 
         * @param hash_table The vocabulary hash table used to look up token keys/indices.
         * @return An array of WORDS pointers, where each WORDS struct owns a contiguous array of keys.
         */
        WORDS** build_lines_table(const WordRecord_new* const *const hash_table)
        {
            WORDS** table = nullptr;

            try
            {
                table = new WORDS*[get_nol()]; 
            }
            catch (const std::bad_alloc& e)
            {
                table = nullptr;
                throw std::runtime_error("Parser::build_lines_table(const WordRecord_new* const *const) Error: " + std::string(e.what()));
            }

            size_t i = 0;

            for (auto& line : *this) // When you dereference the iterator (via `operator*()`), it returns a reference to its internal member `_current`, which is of type `std::vector<std::string>`
                                     // Therefore, the loop variable `line` is of type `std::vector<std::string>&
                                     // When you call `line.size()`, you are calling the standard library method `std::vector::size()` on the vector of tokens for the current line. It outputs the total count of parsed tokens on that line
            {

                WORDS* w_ptr = nullptr; 
                
                try
                {
                    w_ptr = new words(line.size(), nullptr);
                    w_ptr->keys = new size_t[line.size()];
                }
                catch (std::bad_alloc& e)
                {
                    throw std::runtime_error("Parser::build_lines_table(const WordRecord_new* const *const) Error: " + std::string(e.what()));
                }
                
                table[i] = w_ptr;

                size_t key = 0;
                size_t j = 0;

                for (auto& token : line)                
                {
                    if (token.empty())
                    {
                        continue;
                    }

                    key = Keys::generate_key(token, bucket_count);

                    WordRecord_new* w_rec = const_cast<WordRecord_new*>(hash_table[key]);
                    
                    if (w_rec == nullptr)
                    {
                        continue;
                    }

                    if (w_rec->get_word() == token)
                    {
                        // Hash found
                        w_ptr->keys[j] = key;

                        j++;

                        continue;
                    }
                                        
                    // Collision
                    size_t probe = (key + 1) % bucket_count; // In keys.hh, after computing the raw djb2 hash, the index is compressed using the modulo operator

                    while (probe != key)
                    {
                        w_rec = const_cast<WordRecord_new*>(hash_table[probe]);
                        
                        if (w_rec == nullptr)
                        {
                            throw std::runtime_error("Parser::build_lines_table(const WordRecord_new* const *const) Error: Could not find the key " + token);
                        }
                        
                        if (w_rec->get_word() == token)
                        {
                            w_ptr->keys[j] = probe;
                            break;
                        }
                        
                        probe = (probe + 1) % bucket_count; // In keys.hh, after computing the raw djb2 hash, the index is compressed using the modulo operator
                    }

                    if (probe == key)
                    {                     
                        throw std::runtime_error("Parser::build_lines_table(const WordRecord_new* const *const) Error: Could not find the key " + token);
                    }
                    
                    j++;
                }

                i++;

                if (i % CORPUS_SERIALIZATION_CHECKPOINT_INTERVAL == 0)
                {
                    std::cout<< ".";
                }
            }

            // Move to the top of the file
            reset();
            
            return table;
        }

        /*
            ╔══════════════════════════════════════════════════════════════════════════════════╗
            ║                             build_hash_table_very_new()                          ║
            ║    → Will be renamed to build_hash_table(), replacing the current method         ║
            ╚══════════════════════════════════════════════════════════════════════════════════╝

            PURPOSE
            -------
            Iterates over the entire corpus once and builds a vocabulary hash table of unique
            tokens.  Each unique token is assigned a permanent word ID and stored in a hash
            table with linear probing for collision resolution.  This is an optimised version
            designed for efficient vocabulary extraction without the overhead of storing per-token
            occurrence records.

            Unlike the original build_hash_table(), this function:
            • Focuses solely on building the vocabulary (hash table of unique words)
            • Uses linear probing with bucket reallocation for collision management
            • Automatically rehashes when load factor exceeds the threshold
            • Returns only the hash table of WordRecord objects, not the full TABLES structure
            • Tracks essential corpus statistics (bucket_count, bucket_used, token counts)
            • Is more cache-efficient for vocabulary construction


            ════════════════════════════════════════════════════════════════════════════════════
            DATA STRUCTURES PRODUCED
            ════════════════════════════════════════════════════════════════════════════════════

            hash_table   (WordRecord_new*[bucket_count])
            ────────────────────────────────────────────
            Dynamically allocated flat array of pointers.  Index = hash key computed via
            Keys::generate_key(token, bucket_count).  Each non-null slot points to a
            WordRecord_new on the heap:

            WordRecord_new
            ├── word_id     → size_t          dense sequential ID, offset by TOKEN_ID_ORIGINATE_AT_VALUE
            ├── word        → string          the token string itself
            ├── n           → size_t          total count of occurrences in corpus
            │                                Initialised to 1 on first insertion (Cases A and D)
            │                                Incremented by 1 on every subsequent occurrence (Cases B and C)
            └── head        → OccurrenceNode* (not used in this implementation; left for compatibility)

            bucket_count
            ────────────
            The current capacity of the hash table.  Starts at KEYS_COMMON_STARTING_SIZE
            (a prime, typically 1009) to minimise clustering.  Increases dynamically via
            Keys::next_prime() when load factor exceeds KEYS_LOAD_FACTOR_THRESHOLD.

            bucket_used
            ───────────
            The number of distinct unique words encountered so far.  Incremented once per
            unique word.  Used to compute word_id (via bucket_used + TOKEN_ID_ORIGINATE_AT_VALUE)
            and to track load factor.


            ════════════════════════════════════════════════════════════════════════════════════
            ALGORITHM — TOKEN PROCESSING (four cases per token)
            ════════════════════════════════════════════════════════════════════════════════════

            key = Keys::generate_key(token, bucket_count)

            ┌─────────────────────────────────────────────────────────────────────────────┐
            │ CASE A │ hash_table[key] == nullptr          (bucket empty → new word)      │
            ├─────────────────────────────────────────────────────────────────────────────┤
            │  1. Allocate WordRecord_new(word_id=bucket_used+TOKEN_ID_ORIGINATE_AT_VALUE, │
            │     word=token, n=1)                                                        │
            │  2. hash_table[key] = word_record                                           │
            │  3. bucket_used++                                                           │
            └─────────────────────────────────────────────────────────────────────────────┘

            ┌─────────────────────────────────────────────────────────────────────────────┐
            │ CASE B │ hash_table[key]->word == token      (direct match, no collision)   │
            ├─────────────────────────────────────────────────────────────────────────────┤
            │  1. word_record = hash_table[key]                                           │
            │  2. word_record->n++                                                        │
            │     bucket_used is NOT incremented — word already registered.               │
            └─────────────────────────────────────────────────────────────────────────────┘

            ┌─────────────────────────────────────────────────────────────────────────────┐
            │ CASE C │ hash_table[key]->word != token      (hash collision, word in table)│
            ├─────────────────────────────────────────────────────────────────────────────┤
            │  Linear probe:  probe = (key + 1) % bucket_count until:                     │
            │  • hash_table[probe] == nullptr         → CASE D (displaced insertion)      │
            │  • hash_table[probe]->word == token     → existing word found, n++          │
            │  bucket_used is NOT incremented — word already registered.                  │
            └─────────────────────────────────────────────────────────────────────────────┘

            ┌─────────────────────────────────────────────────────────────────────────────┐
            │ CASE D │ During probe: hash_table[probe] == nullptr (displaced new word)    │
            ├─────────────────────────────────────────────────────────────────────────────┤
            │  1. Allocate WordRecord_new(word_id=bucket_used+TOKEN_ID_ORIGINATE_AT_VALUE, │
            │     word=token, n=1)                                                        │
            │  2. hash_table[probe] = word_record                                         │
            │  3. bucket_used++                                                           │
            │     Exit linear probe loop.                                                 │
            └─────────────────────────────────────────────────────────────────────────────┘


            ════════════════════════════════════════════════════════════════════════════════════
            DYNAMIC REHASHING
            ════════════════════════════════════════════════════════════════════════════════════

            After every token insertion, the load factor is checked:

                load_factor = bucket_used / bucket_count

            If load_factor > KEYS_LOAD_FACTOR_THRESHOLD:

            1. Compute new_bucket_count = Keys::next_prime(bucket_count)
            2. Allocate new_hash_table[new_bucket_count]()
            3. Rehash all existing entries from old table into new table:
               • For each non-null slot in old table:
                 - Compute new hash key in the context of new_bucket_count
                 - Use linear probing to find an empty slot in new table
                 - Insert WordRecord* (pointer is moved, not copied)
            4. Delete old hash table
            5. Point hash_table to new_hash_table
            6. Update bucket_count
            7. bucket_used remains unchanged — rehashing does not create new words

            This ensures that the load factor never exceeds the threshold, maintaining
            O(1) average-case lookup and O(1) average-case insertion.


            ════════════════════════════════════════════════════════════════════════════════════
            CORPUS STATISTICS
            ════════════════════════════════════════════════════════════════════════════════════

            mxntpl (Maximum number of tokens per line)
            ──────────────────────────────────────────
            Tracks the longest line (by token count) in the corpus. Updated whenever a token
            length exceeds the current maximum. Used for memory pre-allocation and sizing.

            mnntpl (Minimum number of tokens per line)
            ──────────────────────────────────────────
            Tracks the shortest line in the corpus. Initialised to std::numeric_limits<size_t>::max().
            Updated whenever a token length is less than the current minimum.

            nol (Number of lines)
            ────────────────────
            Incremented once per line processed. At the end of the function, this equals the
            total number of lines in the corpus.

            tnt (Total number of tokens)
            ───────────────────────────
            Incremented for every token (including duplicates). Differs from bucket_used,
            which counts only unique words. tnt is the total token count across all lines.


            ════════════════════════════════════════════════════════════════════════════════════
            PARAMETERS
            ════════════════════════════════════════════════════════════════════════════════════

            (none) — function is parameterless.  Operates on the internal file stream
            and member variables (bucket_count, bucket_used, nol, tnt, mxntpl, mnntpl).


            ════════════════════════════════════════════════════════════════════════════════════
            RETURN VALUE
            ════════════════════════════════════════════════════════════════════════════════════

            WordRecord_new** — heap-allocated hash table of vocabulary.  Size = bucket_count.
            Ownership transfers to the caller.  All WordRecord_new* pointers within the array
            point to heap-allocated objects that are owned by the caller.

            The function also updates the following member variables as a side effect:
                • this->bucket_count  → final hash table size
                • this->bucket_used   → number of unique words
                • this->nol           → total lines in corpus
                • this->tnt           → total tokens in corpus
                • this->mxntpl        → maximum tokens per line
                • this->mnntpl        → minimum tokens per line


            ════════════════════════════════════════════════════════════════════════════════════
            INVARIANTS
            ════════════════════════════════════════════════════════════════════════════════════

            • load_factor ≤ KEYS_LOAD_FACTOR_THRESHOLD throughout execution
              (rehashing ensures this is always satisfied).

            • bucket_used ≤ bucket_count (there is always at least one empty bucket,
              guaranteed by the load factor invariant).

            • Every non-null entry hash_table[i] has a valid WordRecord_new* with:
              - word_id = (original bucket_used at insertion) + TOKEN_ID_ORIGINATE_AT_VALUE
              - n ≥ 1 (every word appears at least once)
              - word is a non-empty string

            • Empty tokens (zero-length strings) are skipped and do not contribute to
              bucket_used, tnt, or any statistics.

            • The order of insertion does not affect the final result; only the set of
              unique words and their frequencies matter.

            • After the function returns, the input file stream is at the beginning
              (via reset()).  The caller can iterate over the corpus again.


            ════════════════════════════════════════════════════════════════════════════════════
            PERFORMANCE
            ════════════════════════════════════════════════════════════════════════════════════

            Time Complexity:
            • Average case:  O(T) where T = total tokens in corpus
              (each token takes O(1) average time for hashing, lookup, and insertion)

            • Worst case:    O(T · log(V)) where V = unique words
              (only if rehashing occurs many times with very poor hash distribution,
               but with linear probing and suitable primes, collisions are minimal)

            Space Complexity:
            • O(V) for the hash table and WordRecord objects
              (V = unique words; typically much smaller than T)

            • Rehashing temporarily allocates a new table during the rebuild, so peak
              space can be 2·V for a brief moment.


            ════════════════════════════════════════════════════════════════════════════════════
            THROWS
            ════════════════════════════════════════════════════════════════════════════════════

            • std::runtime_error — wraps std::bad_alloc if any heap allocation fails.
              The error message identifies the operation that failed (hash table allocation,
              WordRecord allocation, or rehashing).  Partial cleanup is performed where
              possible; however, if an exception is raised after some allocations succeed,
              the hash table may be left in an inconsistent state and should not be used.

            • No exceptions are thrown due to invalid input (e.g., empty corpus); the
              function will simply return an empty hash table (bucket_used = 0).


            ════════════════════════════════════════════════════════════════════════════════════
            USAGE EXAMPLE
            ════════════════════════════════════════════════════════════════════════════════════

            Parser parser("corpus.txt");
            WordRecord_new** vocab = parser.build_hash_table_very_new();

            // vocab[key] points to WordRecord_new for the word at hash key 'key'
            // parser.bucket_used now contains the number of unique words
            // parser.tnt now contains the total number of tokens

            // Caller is responsible for freeing:
            for (size_t i = 0; i < parser.bucket_count; ++i) {
                if (vocab[i] != nullptr) {
                    delete vocab[i];
                }
            }
            delete[] vocab;


            ════════════════════════════════════════════════════════════════════════════════════
            DIFFERENCES FROM ORIGINAL build_hash_table()
            ════════════════════════════════════════════════════════════════════════════════════

            • Returns only the hash table (WordRecord_new**), not the full TABLES structure.
            • Does not build LINE/TOKEN linked lists (removes massive allocation overhead).
            • Does not store occurrence nodes; tracks only word frequency (n).
            • Simpler memory footprint — fewer heap allocations per word.
            • Faster construction due to reduced bookkeeping.
            • Uses WordRecord_new instead of WordRecord for potential type compatibility.
         */
        WordRecord_new** build_hash_table_very_new(size_t** index_table) // index_table is indexed by bucket_used, stores the corresponding hash key for each unique word
        {
            bucket_count = KEYS_COMMON_STARTING_SIZE;
            bucket_used = 0;
            nol = 0;
            tnt = 0;

            // hash_table is indexed by hash key, stores pointers to WordRecord object
            WordRecord_new** hash_table = nullptr;
            //size_t* index_table = nullptr; // index_table is indexed by bucket_used, stores the corresponding hash key for each unique word

            try
            {
                hash_table = new WordRecord_new*[bucket_count](); // Create array of pointers to WordRecord and return address of first element of the array                
                /*
                 * The () at the end is critical — it zero-initialises every pointer to nullptr.
                 * Without it, all bucket pointers are uninitialised garbage, and your
                 * (hash_table[key] == nullptr) check for unique words becomes undefined behaviour.
                 */

                *index_table = new size_t[bucket_count + TOKEN_ID_ORIGINATE_AT_VALUE](); // Create array of hashed keys (size_t) and return address of the first element.
                                                                                         // The + TOKEN_ID_ORIGINATE_AT_VALUE reserves the special-token range below the first active vocabulary ID,
                                                                                         // so valid word IDs are stored in the range [TOKEN_ID_ORIGINATE_AT_VALUE, TOKEN_ID_ORIGINATE_AT_VALUE + bucket_used).
                /*
                 * The () at the end is critical — it zero-initialises every entry of this array to zero.
                 */
            }
            catch (const std::bad_alloc& e)
            {    
                throw std::runtime_error("Parser::build_hash_table_very_new(void) Error: " + std::string(e.what()));
            }

            // 1/3. Calculate the max integer limit ONCE outside the loop (or after a rehash)
            size_t max_allowed_buckets = static_cast<size_t>(bucket_count * KEYS_LOAD_FACTOR_THRESHOLD);

            for (auto& line : *this)
            {
                //std::cout<< "Processing line  with " << line.size() << " tokens." << std::endl;

                for (auto& token : line)                
                {
                    /*
                        Empty String Check
                        -------------------
                        The word can be an empty string, which is not a valid word for the hash table                        
                     */
                    if (token == "")
                    {
                        continue;
                    }

                    size_t key = Keys::generate_key(token, bucket_count); 
                    
                    if (hash_table[key] == nullptr) // Case A: New Word 
                    {
                        try
                        {
                            hash_table[key] = new WordRecord_new(bucket_used + TOKEN_ID_ORIGINATE_AT_VALUE, token, 1); // Create new WordRecord for this unique token and insert into hash table at the generated key
                                                                                                                       // Token ID always originate at TOKEN_ID_ORIGINATE_AT_VALUE 

                            /*
                                TOKEN_ID_ORIGINATE_AT_VALUE is the base offset applied to the
                                first vocabulary word ID. In this codebase it is defined as 1,
                                so the first real token receives word_id = 1 rather than 0.

                                This keeps the compact word-id space separate from the special
                                padding slot used by the context-pair pipeline. In the current
                                Pairs implementation, missing left/right context positions are
                                filled with the literal value 0 in the context arrays, and that
                                value is interpreted as padding downstream.

                                The intent is therefore simple: reserve ID 0 for padding/special
                                handling, while all regular vocabulary tokens start at ID 1 or
                                higher. This avoids collisions between real vocabulary ids and
                                the sentinel value used for missing context positions.
                             */                                                                                                                       
                            *(*index_table + bucket_used + TOKEN_ID_ORIGINATE_AT_VALUE) = key; // Store the hash key for this unique word in the index_table, indexed by bucket_used
                        }
                        catch (const std::bad_alloc& e)
                        {
                            throw std::runtime_error("Parser::build_hash_table_very_new(void) Error: " + std::string(e.what()));
                        }
                        
                        bucket_used++; // Increment count of used buckets for load factor calculation
                    }
                    else if (hash_table[key]->get_word() == token) // Case B: Direct Match
                    {
                        hash_table[key]->n++; // Increment frequency count for this word
                    }
                    else // Case C or D: Collision — need to probe for an empty bucket or a direct match
                    {
                        // Linear probing starts at key+1 because the original bucket (key) was already examined
                        size_t probe = (key + 1) % bucket_count; // Linear probing

                        while (probe != key) // Loop until we circle back to the original key
                        {
                            if (hash_table[probe] == nullptr) // Case D: New Displaced Word
                            {
                                try
                                {
                                    hash_table[probe] = new WordRecord_new(bucket_used + TOKEN_ID_ORIGINATE_AT_VALUE, token, 1); // Create new WordRecord for this unique token and insert into hash table at the probed key

                                    /*
                                        TOKEN_ID_ORIGINATE_AT_VALUE is the base offset applied to the
                                        first vocabulary word ID. In this codebase it is defined as 1,
                                        so the first real token receives word_id = 1 rather than 0.

                                        This keeps the compact word-id space separate from the special
                                        padding slot used by the context-pair pipeline. In the current
                                        Pairs implementation, missing left/right context positions are
                                        filled with the literal value 0 in the context arrays, and that
                                        value is interpreted as padding downstream.

                                        The intent is therefore simple: reserve ID 0 for padding/special
                                        handling, while all regular vocabulary tokens start at ID 1 or
                                        higher. This avoids collisions between real vocabulary ids and
                                        the sentinel value used for missing context positions.
                                     */    
                                    *(*index_table + bucket_used + TOKEN_ID_ORIGINATE_AT_VALUE) = probe; // Store the hash key for this unique word in the index_table, indexed by bucket_used
                                }
                                catch (const std::bad_alloc& e)
                                {
                                    throw std::runtime_error("Parser::build_hash_table_very_new(void) Error: " + std::string(e.what()));
                                }

                                bucket_used++; // Increment count of used buckets for load factor calculation
                                break;
                            }
                            else if (hash_table[probe]->get_word() == token) // Case C: Probe Match
                            {
                                hash_table[probe]->n++; // Increment frequency count for this word
                                break;
                            }
                            //else
                            //{
                                probe = (probe + 1) % bucket_count; // Move to the next bucket
                            //}
                        }

                        if (probe == key)
                        {
                            throw std::runtime_error("Parser::build_hash_table_very_new(void) Error: Hash table is full, cannot insert new word.");
                        }
                    }

                    if (token.size() > mxntpl)
                    {
                        mxntpl = token.size();
                    }

                    if (token.size() < mnntpl)
                    {
                        mnntpl = token.size();
                    }

                    /*
                        ... loop over millions of tokens ...

                        Check if the hash table needs to be rehashed
                        Note: Integer division would truncate the result, so we cast to double

                        PLEASE NOTE:- Inefficient Double-Division Load Factor Check Inside the Inner Loop
                                      Performing floating-point division (bucket_used / bucket_count) for every single token processed (which means million times!). 

                    */
                    /*
                        // 1. Executed MILLIONS of times and For every single token in corpus, the CPU has to:
                        // 1,1 Convert bucket_used to a 64-bit double.
                        // 1.2 Convert bucket_count to a 64-bit double.
                        // 1.3 Perform double-precision floating-point division (/).
                        // Compare the result against KEYS_LOAD_FACTOR_THRESHOLD.

                        Floating-point division is one of the slowest basic arithmetic operations a CPU can perform (often taking 10–20+ CPU clock cycles, compared to 1 cycle for integer comparison).
                     */
                    //if ((static_cast<double>(bucket_used) / static_cast<double>(bucket_count)) > KEYS_LOAD_FACTOR_THRESHOLD)
                    // 2/3. Inside the loop: STILL checked every token, but now it's a 1-cycle integer comparison!
                    if (bucket_used > max_allowed_buckets)
                    {
                        /*
                            Rehash all existing entries into new table
                            Do NOT reset buckets_used, carry the real count forward
                        */
                        size_t old_bucket_count = bucket_count;
                        bucket_count = Keys::next_prime(bucket_count);
                    
                        WordRecord_new** new_hash_table = nullptr; 

                        size_t* new_index_table = nullptr; // New index table for the rehashed keys

                        try
                        {
                            new_hash_table = new WordRecord_new*[bucket_count](); // Create new hash table with updated bucket count
                            /*
                             * The () at the end is critical — it zero-initialises every pointer to nullptr.
                             * Without it, all bucket pointers are uninitialised garbage, and your
                             * (hash_table[key] == nullptr) check for unique words becomes undefined behaviour.
                             */

                            new_index_table = new size_t[bucket_count + TOKEN_ID_ORIGINATE_AT_VALUE](); // Create new index table with updated bucket count.
                                                                                                        // The offset reserves the sentinel range below the first active vocabulary ID,
                                                                                                        // so valid word IDs remain in the range [TOKEN_ID_ORIGINATE_AT_VALUE, TOKEN_ID_ORIGINATE_AT_VALUE + bucket_used).
                            /*
                                * The () at the end is critical — it zero-initialises every entry of this array to zero.
                             */
                        }
                        catch (const std::bad_alloc& e)
                        {
                            throw std::runtime_error("Parser::build_hash_table_very_new(void) Error: " + std::string(e.what()));
                        }

                        // Rehash all entries from old table into new table
                        for (size_t i = 0; i < old_bucket_count; ++i)
                        {
                            if (hash_table[i] != nullptr)  // Entry exists
                            {
                                WordRecord_new* entry = hash_table[i];  // Copy the pointer
                                key = Keys::generate_key(entry->get_word(), bucket_count);

                                if (new_hash_table[key] == nullptr) // Case A: New Word 
                                {
                                    new_hash_table[key] = entry;
                                    
                                    *(new_index_table + entry->get_word_id()) = key; // Store the hash key for this unique word in the new index_table, indexed by word_id                                    
                                }                    
                                else // Collision — need to probe for an empty bucket or a direct match
                                {
                                    // Linear probing starts at key+1 because the original bucket (key) was already examined
                                    size_t probe = (key + 1) % bucket_count; // Linear probing

                                    while (probe != key) // Loop until we circle back to the original key
                                    {
                                        if (new_hash_table[probe] == nullptr) // Case D: New Displaced Word
                                        {
                                            new_hash_table[probe] = entry;
                                            /*
                                                entry->get_word_id() returns the unique word ID assigned to this token, which is used as the index into the new index_table. The value stored at that index is the new hash key (probe) where this word now resides in the rehashed table.
                                                entry->get_word_id() returns a value in the range [TOKEN_ID_ORIGINATE_AT_VALUE, TOKEN_ID_ORIGINATE_AT_VALUE + bucket_used - 1], which is guaranteed to be within the bounds of the new index_table since bucket_used ≤ bucket_count. This ensures that we do not write out of bounds when storing the new hash key.
                                             */
                                            *(new_index_table + entry->get_word_id()) = probe; // Store the hash key for this unique word in the new index_table, indexed by word_id
                                            
                                            break;
                                        }

                                        probe = (probe + 1) % bucket_count; // Move to the next bucket
                                    }

                                    if (probe == key)
                                    {
                                        throw std::runtime_error("Parser::build_hash_table_very_new(void) Error: Hash table is full, cannot insert new word.");
                                    }
                                }
                            }
                        }

                        delete[] hash_table; // Free old table
                        hash_table = new_hash_table; // Point to new table

                        delete[] *index_table; // Free old index table
                        *index_table = new_index_table; // Point to new index table

                        // 3/3. Calculate the max integer limit ONCE outside the loop (or after a rehash)
                        max_allowed_buckets = static_cast<size_t>(bucket_count * KEYS_LOAD_FACTOR_THRESHOLD);
                    }

                    tnt++; // Increment total token count for the corpus
                }

                nol++;

                if (nol % CORPUS_SERIALIZATION_CHECKPOINT_INTERVAL == 0)
                {
                    std::cout<< ".";
                }
            }
            
//#ifdef CORPUS_SERIALIZATION_CHECKPOINT_INTERVAL
//            std::cout<< " nol = " << nol;
//#endif
                        
            // Go to the top of the file
            reset();
            
            return hash_table;
        }

        /*
            ╔══════════════════════════════════════════════════════════════════════════════════╗
            ║                    build_hash_table_with_checkpoints()                          ║
            ╚══════════════════════════════════════════════════════════════════════════════════╝

            PURPOSE
            -------
            Builds the vocabulary table (TABLES) from the corpus, with built‑in support for
            incremental checkpointing and resumption.  This function is designed to handle
            very large corpora where a single pass may be interrupted (power loss, signal,
            user interrupt) — by periodically serialising the current state to disk, the
            build can be resumed from the last checkpoint without re‑parsing the entire
            corpus.

            This function is a superset of `build_hash_table()`: it performs the same
            construction of `hash_to_word_record`, `word_id_to_hash`, and the `lines`
            linked list, but adds the ability to:

            1. Resume from a previously saved checkpoint by passing a `TABLES*` object
               that was deserialised from a checkpoint file (or from the final file).

            2. Write intermediate checkpoints at regular line intervals (configured by
               `CORPUS_SERIALIZATION_CHECKPOINT_INTERVAL`).

            3. Write a final checkpoint upon successful completion (to
               `CORPUS_SERIALIZATION_FINAL_FILENAME`).

            The function’s return behaviour depends on the compilation flag
            `CORPUS_SERIALIZATION_CHECKPOINT_INTERVAL`:

            • If > 0 (checkpointing enabled):
              The function writes the final checkpoint and returns `nullptr`.
              The caller is expected to use the serialised file to obtain the
              `TABLES` object (e.g., by calling `Serialisation::read_tables()`).
              This avoids passing a large heap object back through the stack.

            • If == 0 (checkpointing disabled):
              Behaves exactly like `build_hash_table()` and returns the heap‑allocated
              `TABLES*` with `ref_count = 1`; ownership transfers to the caller.

            See the documentation for `build_hash_table()` for the full details of the
            data structures, token processing (Cases A‑D), rehashing, and invariants.
            This function reuses all that logic; the following sections focus on the
            additional behaviour specific to checkpoints and resumption.


            ═══════════════════════════════════════════════════════════════════════════════════
            PARAMETERS
            ═══════════════════════════════════════════════════════════════════════════════════

            TABLES* tables = nullptr
            ────────────────────────
            If `nullptr`, starts a fresh build from scratch.
            If non‑null, must point to a valid `TABLES` object that was previously
            deserialised from a checkpoint.  The function reuses the existing:
                • hash_table    (tables->hash_to_word_record)
                • index_table   (tables->word_id_to_hash)
                • lines         (tables->lines)
                • bucket_count  (tables->bucket_count)
                • bucket_used   (tables->bucket_used)
                • mxntpl / mnntpl / tnt
            and resumes processing from the next line after the last one that was
            already recorded in the checkpoint.

            HEADER* h = nullptr
            ────────────────────
            Optional pointer to a `HEADER` structure that stores the line count
            (`h->line_count`) of the last completed checkpoint.  When resuming, the
            function skips all lines with `line_number < h->line_count`, so that only
            new lines are processed.  If `h` is `nullptr`, the function assumes the
            entire corpus must be processed (i.e., no checkpoint exists).

            The `HEADER` structure is typically the metadata read from the checkpoint
            file itself, or from a separate “resume” file.

            These parameters are intended to be used together: `tables` provides the
            existing vocabulary state, and `h` tells the function where to start
            processing new lines.


            ═══════════════════════════════════════════════════════════════════════════════════
            RESUMPTION LOGIC
            ═══════════════════════════════════════════════════════════════════════════════════

            When `tables != nullptr`:

            1. The function retrieves all internal data structures from the provided
               `TABLES` object (hash table, index table, line list head/tail, bucket
               counts, mxntpl/mnntpl/tnt, etc.).

            2. It traverses the existing line list to find its tail (`lines_tail`),
               so that new `LINE` nodes can be appended correctly.

            3. It uses `starting_line = h->line_count` to skip lines that have already
               been processed in the previous session.  For each line in the corpus:

               if (line_number < starting_line)
               {
                   line_number++;
                   continue;
               }

               This ensures that lines already represented in the checkpoint are not
               re‑parsed.  Word records and occurrence lists for those lines are
               already present in the `TABLES` object; no duplicate entries are created.

            4. For each token in the new lines, the standard four‑case insertion
               logic applies, exactly as in `build_hash_table()`.  However, an
               important subtlety arises for words that existed before the checkpoint
               but whose occurrence list was partially truncated (see below).


            ═══════════════════════════════════════════════════════════════════════════════════
            CHECKPOINTING — WRITE AND RESUME CONSISTENCY
            ═══════════════════════════════════════════════════════════════════════════════════

            When a checkpoint is written (both intermediate and final), the `TABLES`
            structure is serialised exactly as it stands.  The `HEADER` (or the checkpoint
            file’s own metadata) records:

                • `line_count`         — the number of lines that have been fully processed
                • `bucket_used`        — the number of distinct words seen so far
                • (implicit) the state of every `WordRecord` and `OccurrenceNode`.

            On resumption, the `WordRecord` objects have their `head` and `n` fields
            preserved.  For words that already existed, their occurrence lists are
            complete up to the last processed line.  When we process a new occurrence of
            an already‑existing word, we simply append to the existing list — no special
            handling is needed for the *normal* case.

            HOWEVER, there is one edge case: a word that existed *before* the checkpoint
            may have had **all its occurrences** in lines that were already processed.
            In the checkpoint file, its `WordRecord::head` will be non‑null and
            `WordRecord::n` will equal the number of occurrences.  When we resume and
            process a new line that contains that word, we simply append a new
            `OccurrenceNode` and increment `n`.  That works fine.

            But if the checkpoint was written **exactly after the last occurrence of a
            word** and that word never appears again, then on resumption we will
            encounter the word again only in a new line — that is a new occurrence, so
            appending is correct.

            The code guards against an inconsistent state where `head == nullptr && n == 0`
            yet the word is present in the table — this could happen if a checkpoint was
            taken before any occurrence of that word was recorded (impossible, because the
            word is only added when its first occurrence is seen).  The guard is present
            to handle the case where the checkpoint was taken at a point where a word
            exists in the hash table (from a previous session) but we are resuming and
            the word has not yet been seen in the new session (i.e., its occurrence list
            is empty because the checkpoint was taken before any occurrence was written?).

            Actually, the code contains a special handling when resuming:

            if (current->head == nullptr && current->n == 0) 
            {
                // All previous occurrences were already recorded in the file? 
                // This branch treats the word as if it is being seen for the first time
                // in the current session, and creates a new head with n=1.
            }

            This branch is likely intended to handle the resumption case where a word
            was present in the hash table from the checkpoint but its occurrence list
            was **not** saved (perhaps because the checkpoint was taken before the word’s
            first occurrence was written?  That would be inconsistent).  The original
            `build_hash_table()` never has `head == nullptr && n == 0` because n is
            always set to 1 on first insertion.  In a checkpoint, if the word was
            serialised, its `head` and `n` would have been saved.  So this branch may
            be defensive or for a specific serialisation format that drops occurrence
            lists.  Nonetheless, the comment block should note that the function handles
            such a state by re‑initialising the occurrence list for that word.

            In practice, the checkpoint serialisation should save the entire `WordRecord`
            including `head` and `n`, so this branch should rarely be taken.  But the
            documentation should mention that the function is robust to that scenario.

            The checkpoint writing occurs:

            • At the end of every line where `line_number % CHECKPOINT_INTERVAL == 0`
                (an intermediate checkpoint).

            • At the end of the function (a final checkpoint) when checkpointing is
                enabled.

            When checkpointing is enabled, the function does **not** return the `TABLES*`
            object; it writes the final checkpoint and returns `nullptr`.  The caller is
            expected to read the final checkpoint file to obtain the completed `TABLES`
            structure.  This design simplifies memory management when checkpoints are
            active.


            ═══════════════════════════════════════════════════════════════════════════════════
            DIFFERENCES FROM build_hash_table()
            ═══════════════════════════════════════════════════════════════════════════════════

            • Supports resumption via `tables` and `h`.

            • When resuming, lines before `starting_line` are skipped.

            • The `WordRecord::word_id` is offset by `TOKEN_ID_ORIGINATE_AT_VALUE`
                (likely to reserve ID 0 for a special token such as <PAD> or <UNK>).
                This is a difference in the base implementation; in `build_hash_table()`
                the word_id starts at 0 (or bucket_used).  Here, the offset is applied
                when creating a new `WordRecord`.

            • The `OccurrenceNode*` is **not** stored in the `TOKEN` node
                (`tokens->occurrence` is never set).  This is a deviation from the
                original; the reason might be to reduce memory overhead or because the
                occurrence lists are only needed for the word records, not per token.
                The documentation should note that the `TOKEN` structure still stores
                `token_id`, but the occurrence pointer is left null.  (The code comments
                out assignments to `tokens->occurrence`.)

            • Returns `nullptr` when checkpoints are enabled, otherwise returns the
                `TABLES*` as usual.

            • Contains an extra `read_tables()` call inside the checkpoint block
                (commented out) — likely for debugging; not part of the production flow.

            All other aspects — hash table structure, rehashing, load factor, line/token
            counters, and invariants — remain identical to `build_hash_table()`.


            ═══════════════════════════════════════════════════════════════════════════════════
            INVARIANTS (additional to those of build_hash_table())
            ═══════════════════════════════════════════════════════════════════════════════════

            • When resuming, `starting_line <= line_number` at the point where
                processing begins.  All lines with `line_number < starting_line` are
                skipped and their tokens are **not** processed.

            • The existing `TABLES` object must be consistent with the `HEADER`:
                `h->line_count` should equal the number of lines that are already
                represented in `tables->lines`.  The function does not verify this;
                it is the caller’s responsibility to ensure consistency.

            • The `tables` object must have `ref_count >= 1`; the function does not
                modify `ref_count` (it sets it to 1 when building from scratch).

            • When checkpointing is enabled, the final checkpoint is written, and
                the function returns `nullptr`.  The original `TABLES` object’s memory
                is not freed; it is the caller’s responsibility to free it after reading
                the checkpoint (or the checkpoint file can be used to reconstruct it).


            ═══════════════════════════════════════════════════════════════════════════════════
            RETURNS
            ═══════════════════════════════════════════════════════════════════════════════════

            • If `CORPUS_SERIALIZATION_CHECKPOINT_INTERVAL > 0`:
                `nullptr` — the completed `TABLES` structure has been serialised to
                the final checkpoint file.  The caller must read that file to obtain
                the vocabulary and corpus layout.

            • If `CORPUS_SERIALIZATION_CHECKPOINT_INTERVAL == 0`:
                `TABLES*` — heap‑allocated, `ref_count = 1`, ownership transfers to
                the caller.  The structure contains the complete vocabulary table
                and corpus layout.

            ═══════════════════════════════════════════════════════════════════════════════════
            THROWS
            ═══════════════════════════════════════════════════════════════════════════════════

            • `std::runtime_error` — wraps `std::bad_alloc` if any heap allocation fails.
                Partial cleanup is performed for allocations made during token processing;
                however, if an allocation fails after some structures have been allocated,
                no full rollback is performed — the caller should treat the `TABLES` object
                as invalid if an exception escapes.

            • Exceptions from the serialisation library (`Serialisation::save_tables`)
                are caught and written to `std::cerr` (in the current implementation),
                but they do not cause the function to throw.  This behaviour is noted
                in the code but may be changed in future versions.

            SEE ALSO
            ═══════════════════════════════════════════════════════════════════════════════════

            • `build_hash_table()` — the base implementation without checkpointing.

            • `Serialisation` — for reading/writing checkpoint files.
        */
        TABLES* build_hash_table_with_checkpoints(TABLES* tables = nullptr, HEADER* h = nullptr) 
        {
            // Linked list of lines in the corpus. Each line contains an array of tokens in that line.
            LINE *lines_head = nullptr, *lines_tail = nullptr;

            size_t bucket_count = KEYS_COMMON_STARTING_SIZE;
            size_t bucket_used = 0;

            size_t token_number = 0, line_number = 0;
            size_t mxntpl = 0, mnntpl = std::numeric_limits<size_t>::max(); // Maximum and Minimum number of tokens in a largest and smallest line
            size_t tnt = 0; // Total number of tokens

            // hash_table is indexed by hash key, stores pointers to WordRecord object
            WordRecord** hash_table = nullptr;
            // index_table is indexed by word_id (0..bucket_used-1)
            size_t* index_table = nullptr;

#if CORPUS_SERIALIZATION_CHECKPOINT_INTERVAL > 0
            Serialisation serialisation; // Serialisation object for checkpointing
            size_t starting_line = 0;
            size_t starting_bucket = 0;
#endif  
            try
            {
                if (tables == nullptr)
                {
                    hash_table = new WordRecord*[bucket_count](); // Create array of pointers to WordRecord and return address of first element of the array
                }
                // index_table is indexed by word_id (0..bucket_used-1)
                // Safe because load factor guarantees bucket_used < bucket_count always
                if (tables == nullptr)
                {
                    index_table = new size_t[bucket_count](); // Create array of hashed keys (size_t) and return address of first element of the array
                }
                /*
                 * The () at the end is critical — it zero-initialises every pointer to nullptr.
                 * Without it, all bucket pointers are uninitialised garbage, and your
                 * (hash_table[key] == nullptr) check for unique words becomes undefined behaviour.
                 */

                if (tables == nullptr)
                {
                    tables = new TABLES();
                }
                else
                {
                    hash_table = tables->hash_to_word_record;
                    index_table = tables->word_id_to_hash;
                    lines_head = tables->lines;
                    lines_tail = tables->lines;
                    
                    while (lines_tail->next != nullptr)
                    {
                        lines_tail = lines_tail->next;
                    }
                    
                    bucket_count = tables->get_bucket_count();
                    bucket_used = tables->get_bucket_used();
                    
                    //line_number = tables->line_number;
                    //token_number = tables->token_number;

                    mxntpl = tables->get_maximum_tokens_per_line();
                    mnntpl = tables->get_minimum_tokens_per_line();
                    tnt = tables->get_total_tokens();
#if CORPUS_SERIALIZATION_CHECKPOINT_INTERVAL > 0
                    starting_line = h->line_count;
                    starting_bucket = bucket_used;
#endif                        
                }                                
            }
            catch (const std::bad_alloc& e)
            {    
                throw std::runtime_error("Parser::build_hash_table_with_checkpoints(void) Error: " + std::string(e.what()));
            }

            for (auto& line : *this)
            {
#if CORPUS_SERIALIZATION_CHECKPOINT_INTERVAL > 0                
                if (line_number < starting_line)
                {
                    line_number++;
                    continue;
                }             
#endif                
                if (lines_head == nullptr) // First line — need to create head of the linked list of lines
                {
                    try
                    {
                        lines_head = new LINE(); // Create a new line and append it to the linked list of lines
                        lines_head->prev = nullptr;
                        lines_head->next = nullptr;
                        lines_tail = lines_head; // Set the tail pointer to the head (since it's the only line so far)
                    }
                    catch (const std::bad_alloc& e)
                    {
                        throw std::runtime_error("Parser::build_hash_table(void) Error: " + std::string(e.what()));
                    }  
                }
                else
                {
                    try
                    {
                        lines_tail->next = new LINE(); // Create a new line and append it to the linked list of lines
                        lines_tail->next->prev = lines_tail;
                        lines_tail->next->next = nullptr;
                        lines_tail = lines_tail->next; // Move the new line pointer to the tail pointer
                    }
                    catch (const std::bad_alloc& e)
                    {
                        throw std::runtime_error("Parser::build_hash_table(void) Error: " + std::string(e.what()));
                    }
                }

                lines_tail->n = 0; // Set the number of tokens in the line
                lines_tail->tokens = nullptr; // Set the pointer to the first token in the line

                TOKEN* tokens = nullptr; // Traversal cursor for the linked list of tokens in the current line

                for (auto& token : line)
                {
                    if (lines_tail->tokens == nullptr) // First token in the line — need to create head of the token linked list for this line
                    {
                        try
                        {
                            lines_tail->tokens = new TOKEN(); // Create a new token and append it to the linked list of tokens for this line
                            lines_tail->tokens->next = nullptr;
                            lines_tail->tokens->prev = nullptr;

                            tokens = lines_tail->tokens; // Set the token pointer to the head of the token linked list for this line
                        }
                        catch (const std::bad_alloc& e)
                        {
                            throw std::runtime_error("Parser::build_hash_table(void) Error: " + std::string(e.what()));
                        }                        
                    }
                    else
                    {
                        try
                        {
                            tokens->next = new TOKEN(); // Create a new token and append it to the linked list of tokens for this line
                            tokens->next->prev = tokens;
                            tokens->next->next = nullptr;

                            tokens = tokens->next; // Move the token pointer to the new token
                        }
                        catch (const std::bad_alloc& e)
                        {
                            throw std::runtime_error("Parser::build_hash_table(void) Error: " + std::string(e.what()));
                        }                        
                    }

                    lines_tail->n++; // Increment the number of tokens in the line

                    OccurrenceNode* occurrence = nullptr;
                    WordRecord* word_record = nullptr;
                    
                    size_t key = Keys::generate_key(token, bucket_count);

                    /*if (key == 0)
                    {
                        std::cout<< "key is zero" << std::endl;   
                    }*/
                                        
                    // CASE A: bucket empty → new word, direct natural bucket placement                     
                    if (hash_table[key] == nullptr)
                    {                        
                        try
                        {
                            occurrence = new OccurrenceNode(line_number, token_number, nullptr, nullptr);
                            word_record = new WordRecord(bucket_used + TOKEN_ID_ORIGINATE_AT_VALUE, token, 1, occurrence); // Initialize it to 1 on first insertion
                            hash_table[key] = word_record;
                            index_table[word_record->word_id] = key;
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

                        tokens->token_id = word_record->word_id; // Set the token_id of the token to the word_id of the word_record
                        /*tokens->occurrence = occurrence; // Set the occurrence of the token to the occurrence node created for the word_record*/

                        bucket_used++;     
                    }
                    else // Token is already in the hash table, it could be a collision as well as the same word at a different position
                    {
                        WordRecord* current = hash_table[key];

                        // Case B — Same repeated word (direct match), no collision and went to natural bucket and not displaced
                        if (current->get_word() == token)
                        {
                            if (current->head == nullptr && current->n == 0) // All the previous occurances were already recorded in the file
                            { 
                                try
                                {
                                    current->head = new OccurrenceNode(line_number, token_number, nullptr, nullptr); // Create a new occurrence node and append it to the linked list of occurrences for this line
                                    current->n = 1; // Set the number of occurrences of the word to 1
                                }
                                catch (const std::bad_alloc& e)
                                {   
                                    throw std::runtime_error("Parser::build_hash_table_with_checkpoints(void) Error: " + std::string(e.what()));
                                }
                            }
                            else // All occurances are intact since the last recording in the file session (if any such session/s took place)
                            {
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
                                    word_record->n++; // Increment the n of the word_record for this token/word in the corpus   
                                }
                                catch (const std::bad_alloc& e) 
                                { 
                                    throw std::runtime_error("Parser::build_hash_table(void) Error: " + std::string(e.what()));
                                }
                            
                                tokens->token_id = word_record->word_id; // Set the token_id of the token to the word_id of the word_record 
                                /*tokens->occurrence = occurrence->next; // Set the occurrence of the token to the occurrence node created for the word_record*/                       
                            }
                        }
                        else // Collision, start probing for empty bucket
                        {
                            size_t probe = (key + 1) % bucket_count;

                            while (probe != key)
                            {
                                // Case D — empty bucket during probe — new word displaced from natural bucket to a probed bucket
                                if (hash_table[probe] == nullptr)
                                {   
                                    /*std::cout<< "1"  << std::endl;*/

                                    try
                                    {
                                        occurrence = new OccurrenceNode(line_number, token_number, nullptr, nullptr);
                                        word_record = new WordRecord(bucket_used + TOKEN_ID_ORIGINATE_AT_VALUE, token, 1, occurrence); // Initialize it to 1 on first insertion
                                        hash_table[/*key*/probe] = word_record;
                                        index_table[word_record->word_id] = /*key*/probe;
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

                                    tokens->token_id = word_record->word_id; // Set the token_id of the token to the word_id of the word_record
                                    /*tokens->occurrence = occurrence; // Set the occurrence of the token to the occurrence node created for the word_record*/
                                    
                                    bucket_used++;
                                    break;
                                }
                                // Case C  (probe match) — same repeated word, displaced from natural bucket to probed bucket                               
                                else if (hash_table[probe]->get_word() == token)
                                {                                    
                                    WordRecord* word_record = hash_table[probe];

                                    if (word_record->head == nullptr && word_record->n == 0) // All the previous occurances were already recorded in the file
                                    { 
                                        /*std::cout<< "2.1" << std::endl;*/
                                        try
                                        {
                                            word_record->head = new OccurrenceNode(line_number, token_number, nullptr, nullptr); // Create a new occurrence node and append it to the linked list of occurrences for this line
                                            word_record->n = 1; // Set the number of occurrences of the word to 1

                                            tokens->occurrence = word_record->head; 

                                            /*std::cout<< "2.1.1" << std::endl;*/
                                        }
                                        catch (const std::bad_alloc& e)
                                        {   
                                            throw std::runtime_error("Parser::build_hash_table_with_checkpoints(void) Error: " + std::string(e.what()));
                                        }
                                    }
                                    else // All occurances are intact since the last recording in the file session (if any such session/s took place)
                                    {                                 
                                        occurrence = word_record->head; // Get the head of the linked list

                                        while (occurrence->next != nullptr) // Traverse to the end of the linked list
                                        {                             
                                            occurrence = occurrence->next;
                                        }
                                        try
                                        { 
                                            // Create a new occurrence node and append it to the end of the linked list which has all occurrences of this token/word in whole of the corpus
                                            occurrence->next = new OccurrenceNode(line_number, token_number, nullptr, occurrence);
                                            word_record->n++; // Increment the n of the word_record for this token/word in the corpus   

                                            //tokens->occurrence = occurrence->next; 
                                        }
                                        catch (const std::bad_alloc& e) 
                                        { 
                                            throw std::runtime_error("Parser::build_hash_table(void) Error: " + std::string(e.what()));
                                        }
                                    }

                                    tokens->token_id = word_record->word_id; // Set the token_id of the token to the word_id of the word_record 

                                    break;
                                }

                                probe = (probe + 1) % bucket_count;
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
                                    /*
                                        Rehash Collision — Linear Probe with Infinite-Loop Guard
                                        ─────────────────────────────────────────────────────────
                                        During rehash, two words that lived in different buckets in the
                                        old table can collide in the new table because the new bucket count
                                        is different.  We resolve this with the same linear probing strategy
                                        used during normal insertion: walk forward from new_key until an
                                        empty slot is found.

                                        Infinite-Loop Risk
                                        ──────────────────
                                        The termination condition  (probe != new_key)  relies on wrapping
                                        all the way around the table and arriving back at new_key — which
                                        only happens if at least one slot is empty somewhere in the table.
                                        If next_prime() did not grow the table enough and bucket_used is
                                        close to bucket_count, every slot could already be occupied by the
                                        time a later entry is being rehashed, making full wrap-around
                                        impossible and the loop infinite.

                                        The load factor threshold (KEYS_LOAD_FACTOR_THRESHOLD) is the first
                                        line of defence: it triggers rehash early enough that bucket_used
                                        is always well below bucket_count.  But that threshold governs the
                                        OLD table.  After next_prime() the new table is larger, yet by the
                                        time the last entry of the old table is being rehashed, (bucket_used
                                        / new_bucket_count) may still be uncomfortably high if next_prime()
                                        returned a value only marginally larger.

                                        The step counter below is the second line of defence.  If we have
                                        probed more than bucket_count slots without finding an empty one,
                                        the table is effectively full and continued probing is pointless.
                                        We throw rather than spin forever.
                                     */                                 
                                    size_t probe = (new_key + 1) % bucket_count;
                                    size_t steps = 0; // for counting the number of steps taken to find an empty slot
                                                      // This is a safety measure against infinite loops in case the table is full (never grown enough by next_prime()).
                                                      // However, given the load factor, this should never happen as soon table size reaches this threshold, it will rehash and increase its size.   
                                                      // Even saftey counter is getting included.

                                    // Linear probing to find an empty slot, until collision happens.
                                    while (probe != new_key /*&& steps < bucket_count*/) // Until we circle back to the original index/key
                                    {
                                        if (new_hash_table[probe] == nullptr)
                                        {
                                            new_hash_table[probe] = hash_table[i]; 
                                            new_index_table[hash_table[i]->word_id] = probe;  
                                            break;
                                        }
                                        probe = (probe + 1) % bucket_count; // Linear probing with wrap-around at the end of the table
                                                                            // If table is not big enough, then due to wrap-around collision with new_key may never happen.
                                                                            // So, we are at risk of infinite loop here.
            
                                        steps = steps + 1; // Increment the safety counter

                                        /*
                                            Safety guard — should never fire under normal operating conditions.
                                            If it does fire, it means next_prime() returned a value too close
                                            to the old bucket_count, the new table filled up before all old
                                            entries could be rehashed, and the wrap-around termination
                                            condition (probe != new_key) can no longer be reached.
                                            Increase KEYS_LOAD_FACTOR_THRESHOLD (lower the threshold ratio)
                                            or ensure next_prime() grows the table by at least 2x to
                                            guarantee sufficient headroom after every rehash.
                                        */
                                        if (steps >= bucket_count)
                                        {
                                            throw std::runtime_error(
                                                "Parser::build_hash_table() Fatal: rehash linear probe "
                                                "exhausted all " + std::to_string(bucket_count) + " buckets "
                                                "while relocating word \"" + hash_table[i]->word + "\" "
                                                "(word_id=" + std::to_string(hash_table[i]->word_id) + "). "
                                                "The new table is full: bucket_used=" + std::to_string(bucket_used) +
                                                " vs bucket_count=" + std::to_string(bucket_count) + ". "
                                                "next_prime() did not grow the table enough — lower "
                                                "KEYS_LOAD_FACTOR_THRESHOLD or guarantee next_prime() "
                                                "returns at least 2x the previous bucket count."
                                            );
                                        }                                                                            
                                    }                                    
                                } 
                            } 
                        }

                        delete[] hash_table;
                        delete[] index_table;

                        hash_table = new_hash_table;
                        index_table = new_index_table;          
                    }
                    
                    tnt = tnt + 1;
#if CORPUS_SERIALIZATION_CHECKPOINT_INTERVAL > 0
                    /*if (starting_bucket == 0 && bucket_used == 1)
                    {
                        starting_bucket = 0; // starting_bucket and starting_line originate at 0
                    }
                    else if (starting_bucket == 0)                        
                    {
                        starting_bucket = bucket_used - 2; // starting_bucket and starting_line originate at 0
                    }*/
#endif
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

                tables->hash_to_word_record = hash_table;
                tables->word_id_to_hash = index_table;
                tables->lines = lines_head;

                tables->bucket_count = bucket_count;
                tables->bucket_used = bucket_used;

                tables->ref_count = 1;

                tables->maximum_tokens_per_line = mxntpl; // max
                tables->minimum_tokens_per_line = mnntpl; // min
                tables->total_tokens = tnt; // total number tokenss (vocabulary + redundency)

                /*
                    When hitting the checkpoint interval, generate a unique fully qualified 
                    filename (fqn) containing the current line number and total tokens processed.
                    
                    When this condition is true:
                    1. The fqn string is constructed to be used as the target file path.
                    2. This fqn is intended to be passed to a serialization function 
                       (e.g., Serialisation::save_tables) to save the current TABLES 
                       state (the parsed corpus up to this point) to disk. This ensures 
                       that progress is saved incrementally.
                 */
#if CORPUS_SERIALIZATION_CHECKPOINT_INTERVAL > 0                 
                if (line_number % CORPUS_SERIALIZATION_CHECKPOINT_INTERVAL == 0)
                {
                    std::string fqn = CORPUS_SERIALIZATION_CHECKPOINT_FILENAME + std::string(".") + std::to_string(line_number) + std::string(".") + "intermediate_check_point_number" + std::string(CORPUS_SERIALIZATION_CHECKPOINT_EXTENSION);

                    std::cout<< "Starting = " << starting_line << " Ending = " << line_number << std::endl;
                    std::cout<< "Starting bucket = " << starting_bucket << " Ending bucket = " << bucket_used << std::endl;
                    std::cout<< "-----------------------------------------------------------------------------------------------------------------------------------" << std::endl;
                    
                    serialisation.save_tables(tables, starting_bucket, starting_line, line_number, fqn);

                    //free_tables(index_table, hash_table, lines_head, bucket_used);
                    //lines_head = nullptr;
                    //lines_tail = nullptr;

                    starting_line = line_number; // starting_line originates at 0
                    starting_bucket = bucket_used; // Likewise 

                    serialisation.read_tables(fqn);

                    //exit(0);
                }
#endif                           
            }

            serialisation.save_tables(tables, starting_bucket, starting_line, line_number, CORPUS_SERIALIZATION_FINAL_FILENAME);

#if CORPUS_SERIALIZATION_CHECKPOINT_INTERVAL > 0                 
            return nullptr;
#else
            tables->hash_to_word_record = hash_table;
            tables->word_id_to_hash = index_table;
            tables->lines = lines_head;

            tables->bucket_count = bucket_count;
            tables->bucket_used = bucket_used;

            tables->ref_count = 1;

            tables->maximum_tokens_per_line = mxntpl; // max
            tables->minimum_tokens_per_line = mnntpl; // min
            tables->total_tokens = tnt; // total number tokenss (vocabulary + redundency)

            return tables;
#endif
        }

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
            ├── n        → size_t          total number of times this word appears in the corpus
            │                             Initialised to 1 on first insertion (Cases A and D).
            │                             Incremented by 1 on every subsequent occurrence
            │                             (Cases B and C).  Always equals the length of the
            │                             occurrence linked list.  Enables O(1) frequency
            │                             and probability queries without traversing the list.
            └── head     → OccurrenceNode* head of the occurrence linked list

            Each OccurrenceNode:
                 ├── line   → size_t          line in the corpus (0-based, never reset)
                 ├── token  → size_t          position within that line (0-based, reset per line)
                 ├── next   → OccurrenceNode* next occurrence in corpus order
                 └── prev   → OccurrenceNode* previous occurrence (doubly linked)

            Occurrences are appended to the tail so the list is always in corpus order.

            2.  word_id_to_hash   (size_t[bucket_count])
            ──────────────────────────────────────────
            Parallel array indexed by word_id.  Stores the bucket index at which that
            word's WordRecord actually lives in hash_to_word_record.  Needed because
            linear probing can displace a word from its natural hash key.

            Bidirectional lookup:
                token string  →  generate_key()             →  hash_to_word_record[key]  →  WordRecord
                word_id       →  word_id_to_hash[word_id]   →  hash_to_word_record[key]  →  WordRecord

            3.  lines   (LINE*)
            ──────────────────────────────────────────────────
            Head of a singly-traversable doubly-linked list of LINE nodes, one per
            line in the corpus, in corpus order.

            LINE
            ├── n       → size_t   number of tokens in this line
            ├── tokens  → TOKEN*   head of the token linked list for this line
            ├── next    → LINE*    next line in corpus order
            └── prev    → LINE*    previous line

            Each TOKEN node:
                 ├── token_id   → size_t          word_id of this token (index into embedding matrix)
                 ├── occurrence → OccurrenceNode* pointer to this token's specific occurrence record
                 ├── next       → TOKEN*          next token in this line
                 └── prev       → TOKEN*          previous token in this line

            This structure preserves the full sequential layout of the corpus — every
            line, every token in order — independently of the hash table.  It enables
            O(n) corpus traversal without rehashing or re-reading the file.

            CRITICAL: LINE and TOKEN nodes are never touched during rehash.  Their
            token_id and occurrence fields reference heap objects (WordRecord,
            OccurrenceNode) that are stable for the lifetime of TABLES.


            ════════════════════════════════════════════════════════════════════════════════════
            ALGORITHM — TOKEN PROCESSING (per token, four cases)
            ════════════════════════════════════════════════════════════════════════════════════

            key = Keys::generate_key(token, bucket_count)

            ┌─────────────────────────────────────────────────────────────────────────────┐
            │ CASE A │ hash_table[key] == nullptr          (bucket empty → new word)      │
            ├─────────────────────────────────────────────────────────────────────────────┤
            │  1. Allocate OccurrenceNode(line_number, token_number, next=null, prev=null)│
            │  2. Allocate WordRecord(word_id=bucket_used, word=token, n=1, head=occ)     │
            │     n is initialised to 1 — this is the first occurrence of this word.      │
            │  3. hash_table[key]                   = word_record                         │
            │  4. index_table[word_record->word_id] = key                                 │
            │  5. bucket_used++                                                           │
            └─────────────────────────────────────────────────────────────────────────────┘

            ┌─────────────────────────────────────────────────────────────────────────────┐
            │ CASE B │ hash_table[key]->word == token      (same word, direct match)      │
            ├─────────────────────────────────────────────────────────────────────────────┤
            │  1. word_record = hash_table[key]                                           │
            │  2. Traverse occurrence list from head to tail                              │
            │  3. Append new OccurrenceNode(line_number, token_number, next=null,         |
            |     prev=tail)                                                              │
            │  4. word_record->n++                                                        │
            │     Incremented because this is a repeat occurrence of an existing word.    │
            │  bucket_used is NOT incremented — word already registered.                  │
            └─────────────────────────────────────────────────────────────────────────────┘

            ┌─────────────────────────────────────────────────────────────────────────────┐
            │ CASE C │ hash_table[key]->word != token      (hash collision — linear probe)│
            ├─────────────────────────────────────────────────────────────────────────────┤
            │  Linear probe:  probe = (key + 1) % bucket_count,  advance until:           │
            │                                                                             │
            │  • hash_table[probe] == nullptr           (CASE D — new displaced word)     │
            │        New word, no prior record exists.                                    │
            │        Allocate OccurrenceNode + WordRecord(n=1), place at probe.           │
            │        n is initialised to 1 — first occurrence of this word.               │
            │        index_table[word_id] = probe   ← MUST store probe, not key           │
            │        bucket_used++                                                        │
            │                                                                             │
            │  • hash_table[probe]->word == token       (probe match — word found)        │
            │        Word was previously displaced here.  Found it.                       │
            │        Traverse its occurrence list and append new OccurrenceNode.          │
            │        word_record->n++                                                     │
            │        Incremented because this is a repeat occurrence of an existing word. │
            │                                                                             │
            │  • otherwise: probe = (probe + 1) % bucket_count  — keep scanning           │
            └─────────────────────────────────────────────────────────────────────────────┘

            WordRecord::n INVARIANT ACROSS ALL CASES
            ─────────────────────────────────────────
            word_record->n  ==  length of word_record->head linked list

            This holds at all times because n is incremented in exactly the same
            cases where a new OccurrenceNode is appended to the list:

                Case A  — new word:                 n = 1,  list length = 1   ✓
                Case B  — repeat, direct match:     n++,    list appended     ✓
                Case D  — new word, displaced:      n = 1,  list length = 1   ✓
                Case C  — repeat, probe match:      n++,    list appended     ✓

            The consequence is that Corpus::probability() and Corpus::frequency()
            are O(1) — no list traversal is ever needed:

                frequency(word_id)   = word_record->n
                probability(word_id) = (double)word_record->n / (double)tables->total_tokens


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
                WordRecord::n is stored inside the WordRecord object on the heap — it is
                unaffected by rehash and retains its accumulated count across all rehash cycles.
                LINE and TOKEN linked lists are entirely unaffected by rehash — they hold
                word_id and OccurrenceNode* values, neither of which is a bucket index.

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

                -  bucket_used < bucket_count at all times
                   Enforced by the load factor check.  Guarantees at least one empty slot
                   always exists so that linear probing always terminates.

                -  index_table[word_id] always points to the actual bucket of that word
                   Maintained by storing probe (not key) when a word is displaced.

                -  word_id values are dense in [0, bucket_used)
                   Enables direct use as row indices into the embedding matrix E[vocab_size × d_model].

                -  hash_table is zero-initialised on every allocation (the () matters)
                   nullptr is the only sentinel for "bucket is empty".

                -  Occurrence lists are always in corpus order
                   New nodes are always appended to the tail.

                -  WordRecord::n always equals the length of WordRecord::head linked list
                   Initialised to 1 in Cases A and D (new word).
                   Incremented by 1 in Cases B and C (repeat occurrence).
                   Never modified during rehash — the WordRecord object does not move.
                   Consequence: frequency and probability queries are O(1).

                -  WordRecord and OccurrenceNode are never freed during rehash
                   Only arrays are reallocated.  Heap objects are stable for the lifetime
                   of the TABLES structure.

                -  LINE list length == line_number at end of build
                   One LINE node is allocated per corpus line, in order.

                -  TOKEN list length for each LINE == LINE::n
                   Incremented exactly once per token at the top of the token loop.
                   No branch inside the loop touches LINE::n.


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

                WordRecord::n provides:
                    word_id  →  frequency             O(1),  used for probability, subsampling,
                                                      negative sampling weight in Skip-gram

                The lines linked list provides:
                    corpus position  →  TOKEN node  →  token_id + occurrence   O(n) traversal
                    Enables sequence reconstruction needed for positional encoding and
                    attention mask generation without re-reading the file.

                See TABLES.md for full documentation including memory layout diagrams.

                RETURNS
                -------
                TABLES*  — heap-allocated, ref_count = 1, ownership transfers to caller.
                           Contains hash_to_word_record, word_id_to_hash, and lines —
                           the complete vocabulary table and corpus layout.

                THROWS
                ------
                std::runtime_error — wraps std::bad_alloc if any heap allocation fails.
                                     Partial cleanup is performed for allocations made inside the token‑processing loop; however,
                                     the function does not implement a full transactional rollback for previously allocated structures
                                     (e.g., hash buckets, line nodes). If such a failure occurs, the caller should treat the TABLES
                                     object as invalid and not use it.                                      
         */
        TABLES* build_hash_table(void)
        {
            size_t bucket_count = KEYS_COMMON_STARTING_SIZE;
            size_t bucket_used = 0;

            // hash_table is indexed by hash key, stores pointers to WordRecord object
            WordRecord** hash_table = nullptr;
            // index_table is indexed by word_id (0..bucket_used-1)
            size_t* index_table = nullptr;
            // Linked list of lines in the corpus. Each line contains an array of tokens in that line.
            LINE *lines_head = nullptr, *lines_tail = nullptr;

            size_t line_number = 0;
            size_t token_number = 0;

            size_t mxntpl = 0, mnntpl = std::numeric_limits<size_t>::max();; // Maximum and Minimum number of tokens in a largest and smallest line
            size_t tnt = 0; // Total number of tokens

            TABLES* tables = nullptr;

#if CORPUS_SERIALIZATION_CHECKPOINT_INTERVAL > 0
            Serialisation serialisation; // Serialisation object for checkpointing
            size_t starting_line = 0;
            size_t starting_bucket = 0;
#endif            
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

                tables = new TABLES();
            }
            catch (const std::bad_alloc& e)
            {
                // Corpus can be large. This is a real possibility, not a formality.             
                throw std::runtime_error("Parser::build_hash_table(void) Error: " + std::string(e.what()));
            }
        
            for (auto& line : *this)
            { 
                if (lines_head == nullptr) // First line — need to create head of the linked list of lines
                {
                    try
                    {
                        lines_head = new LINE(); // Create a new line and append it to the linked list of lines
                        lines_head->prev = nullptr;
                        lines_head->next = nullptr;
                        lines_tail = lines_head; // Set the tail pointer to the head (since it's the only line so far)
                    }
                    catch (const std::bad_alloc& e)
                    {
                        throw std::runtime_error("Parser::build_hash_table(void) Error: " + std::string(e.what()));
                    }  
                }
                else
                {
                    try
                    {
                        lines_tail->next = new LINE(); // Create a new line and append it to the linked list of lines
                        lines_tail->next->prev = lines_tail;
                        lines_tail->next->next = nullptr;
                        lines_tail = lines_tail->next; // Move the new line pointer to the tail pointer
                    }
                    catch (const std::bad_alloc& e)
                    {
                        throw std::runtime_error("Parser::build_hash_table(void) Error: " + std::string(e.what()));
                    }
                }

                lines_tail->n = 0; // Set the number of tokens in the line
                lines_tail->tokens = nullptr; // Set the pointer to the first token in the line

                TOKEN* tokens = nullptr; // Traversal cursor for the linked list of tokens in the current line
                                
                for (auto& token : line)
                {
                    if (lines_tail->tokens == nullptr) // First token in the line — need to create head of the token linked list for this line
                    {
                        try
                        {
                            lines_tail->tokens = new TOKEN(); // Create a new token and append it to the linked list of tokens for this line
                            lines_tail->tokens->next = nullptr;
                            lines_tail->tokens->prev = nullptr;

                            tokens = lines_tail->tokens; // Set the token pointer to the head of the token linked list for this line
                        }
                        catch (const std::bad_alloc& e)
                        {
                            throw std::runtime_error("Parser::build_hash_table(void) Error: " + std::string(e.what()));
                        }                        
                    }
                    else
                    {
                        try
                        {
                            tokens->next = new TOKEN(); // Create a new token and append it to the linked list of tokens for this line
                            tokens->next->prev = tokens;
                            tokens->next->next = nullptr;

                            tokens = tokens->next; // Move the token pointer to the new token
                        }
                        catch (const std::bad_alloc& e)
                        {
                            throw std::runtime_error("Parser::build_hash_table(void) Error: " + std::string(e.what()));
                        }                        
                    }

                    lines_tail->n++; // Increment the number of tokens in the line

                    size_t key = Keys::generate_key(token, bucket_count);

                    OccurrenceNode* occurrence = nullptr;
                    WordRecord* word_record = nullptr;

                    // CASE A: bucket empty → new word, direct natural bucket placement
                    if (hash_table[key] == nullptr) // If the bucket is empty, it means the token/word is new
                    {
                        //std::cout<< "New bucket created for " << token << " at line " << line_number << " and token " << token_number << std::endl; 

                        try
                        {
                            occurrence = new OccurrenceNode(line_number, token_number, nullptr, nullptr);
                            word_record = new WordRecord(/*token_number*/ bucket_used, token, 1, occurrence); // Initialize it to 1 on first insertion
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

                        tokens->token_id = word_record->word_id; // Set the token_id of the token to the word_id of the word_record
                        tokens->occurrence = occurrence; // Set the occurrence of the token to the occurrence node created for the word_record

                        /*lines_tail->n++;*/ // Increment the number of tokens in the line
                        
                        bucket_used++; // Increment the number of buckets used                                                 
#if CORPUS_SERIALIZATION_CHECKPOINT_INTERVAL > 0
                        if (starting_bucket == 0)
                        {
                            starting_bucket = bucket_used;
                        }  
#endif                        
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
                                // Case D — empty bucket during probe — new word displaced from natural bucket to a probed bucket
                                if (hash_table[probe] == nullptr)
                                {
                                    // Found empty slot — treat as new word
                                    // (fall through to your "new bucket" logic)
                                    // ... insert word_record here ...
                                    try 
                                    { 
                                        occurrence = new OccurrenceNode(line_number, token_number, nullptr, nullptr);
                                        word_record = new WordRecord(/*token_number*/ bucket_used, token, 1, occurrence); // Initialize it to 1 on first insertion
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
                                    
                                    tokens->token_id = word_record->word_id; // Set the token_id of the token to the word_id of the word_record
                                    tokens->occurrence = occurrence; // Set the occurrence of the token to the occurrence node

                                    /*lines_tail->n++;*/ // Increment the number of tokens in the line
                                                                       
                                    bucket_used++;
#if CORPUS_SERIALIZATION_CHECKPOINT_INTERVAL > 0
                                    if (starting_bucket == 0)
                                    {
                                        starting_bucket = bucket_used;
                                    }                                    
#endif                                    
                                    break;
                                }
                                // Case C  (probe match) — same repeated word, displaced from natural bucket to probed bucket                               
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
                                    
                                    tokens->token_id = word_record->word_id; // Set the token_id of the token to the word_id of the word_record
                                    tokens->occurrence = occurrence->next; // Set the occurrence of the token to the occurrence node created for the word_record
                                    
                                    /*lines_tail->n++;*/ // Increment the number of tokens in the line

                                    // Increment the n of occurrence node for this token/word in the line
                                    word_record->n++; // Increment the n of the word_record for this token/word in the corpus

                                    break;
                                }
                                probe = (probe + 1) % bucket_count;
                            }
                        }
                        else
                        {
                            // Case B — Same repeated word (direct match), no collision and went to natural bucket and not displaced                           
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
                            
                            tokens->token_id = word_record->word_id; // Set the token_id of the token to the word_id of the word_record 
                            tokens->occurrence = occurrence->next; // Set the occurrence of the token to the occurrence node created for the word_record

                            /*lines_tail->n++;*/ // Increment the number of tokens in the line

                            // Increment the n of occurrence node for this token/word in the line
                            word_record->n++; // Increment the n of the word_record for this token/word in the corpus
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
                                    /*
                                        Rehash Collision — Linear Probe with Infinite-Loop Guard
                                        ─────────────────────────────────────────────────────────
                                        During rehash, two words that lived in different buckets in the
                                        old table can collide in the new table because the new bucket count
                                        is different.  We resolve this with the same linear probing strategy
                                        used during normal insertion: walk forward from new_key until an
                                        empty slot is found.

                                        Infinite-Loop Risk
                                        ──────────────────
                                        The termination condition  (probe != new_key)  relies on wrapping
                                        all the way around the table and arriving back at new_key — which
                                        only happens if at least one slot is empty somewhere in the table.
                                        If next_prime() did not grow the table enough and bucket_used is
                                        close to bucket_count, every slot could already be occupied by the
                                        time a later entry is being rehashed, making full wrap-around
                                        impossible and the loop infinite.

                                        The load factor threshold (KEYS_LOAD_FACTOR_THRESHOLD) is the first
                                        line of defence: it triggers rehash early enough that bucket_used
                                        is always well below bucket_count.  But that threshold governs the
                                        OLD table.  After next_prime() the new table is larger, yet by the
                                        time the last entry of the old table is being rehashed, (bucket_used
                                        / new_bucket_count) may still be uncomfortably high if next_prime()
                                        returned a value only marginally larger.

                                        The step counter below is the second line of defence.  If we have
                                        probed more than bucket_count slots without finding an empty one,
                                        the table is effectively full and continued probing is pointless.
                                        We throw rather than spin forever.
                                     */                                 
                                    size_t probe = (new_key + 1) % bucket_count;
                                    size_t steps = 0; // for counting the number of steps taken to find an empty slot
                                                      // This is a safety measure against infinite loops in case the table is full (never grown enough by next_prime()).
                                                      // However, given the load factor, this should never happen as soon table size reaches this threshold, it will rehash and increase its size.   
                                                      // Even saftey counter is getting included.

                                    // Linear probing to find an empty slot, until collision happens.
                                    while (probe != new_key /*&& steps < bucket_count*/) // Until we circle back to the original index/key
                                    {
                                        if (new_hash_table[probe] == nullptr)
                                        {
                                            new_hash_table[probe] = hash_table[i]; 
                                            new_index_table[hash_table[i]->word_id] = probe;  
                                            break;
                                        }
                                        probe = (probe + 1) % bucket_count; // Linear probing with wrap-around at the end of the table
                                                                            // If table is not big enough, then due to wrap-around collision with new_key may never happen.
                                                                            // So, we are at risk of infinite loop here.
            
                                        steps = steps + 1; // Increment the safety counter

                                        /*
                                            Safety guard — should never fire under normal operating conditions.
                                            If it does fire, it means next_prime() returned a value too close
                                            to the old bucket_count, the new table filled up before all old
                                            entries could be rehashed, and the wrap-around termination
                                            condition (probe != new_key) can no longer be reached.
                                            Increase KEYS_LOAD_FACTOR_THRESHOLD (lower the threshold ratio)
                                            or ensure next_prime() grows the table by at least 2x to
                                            guarantee sufficient headroom after every rehash.
                                        */
                                        if (steps >= bucket_count)
                                        {
                                            throw std::runtime_error(
                                                "Parser::build_hash_table() Fatal: rehash linear probe "
                                                "exhausted all " + std::to_string(bucket_count) + " buckets "
                                                "while relocating word \"" + hash_table[i]->word + "\" "
                                                "(word_id=" + std::to_string(hash_table[i]->word_id) + "). "
                                                "The new table is full: bucket_used=" + std::to_string(bucket_used) +
                                                " vs bucket_count=" + std::to_string(bucket_count) + ". "
                                                "next_prime() did not grow the table enough — lower "
                                                "KEYS_LOAD_FACTOR_THRESHOLD or guarantee next_prime() "
                                                "returns at least 2x the previous bucket count."
                                            );
                                        }                                                                            
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

                    tnt = tnt + 1; // Total number of tokens in the corpus.
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

                tables->hash_to_word_record = hash_table;
                tables->word_id_to_hash = index_table;
                tables->lines = lines_head;

                tables->bucket_count = bucket_count;
                tables->bucket_used = bucket_used;

                tables->ref_count = 1;

                tables->maximum_tokens_per_line = mxntpl;
                tables->minimum_tokens_per_line = mnntpl;
                tables->total_tokens = tnt;

/*
    Automatic checkpoint — fires when line_number is a
    non-zero multiple of PARSER_SERIALIZATION_CHECKPOINT_INTERVAL.
    Writes the current TABLES state to a versioned binary file
    so the build can be safely interrupted without losing work.
 */
#if CORPUS_SERIALIZATION_CHECKPOINT_INTERVAL > 0
                // Initialize the starting line on the first pass
                if (starting_line == 0)
                {
                    starting_line = line_number;        
                }
                
                /*
                    When hitting the checkpoint interval, generate a unique fully qualified 
                    filename (fqn) containing the current line number and total tokens processed.
                    
                    When this condition is true:
                    1. The fqn string is constructed to be used as the target file path.
                    2. This fqn is intended to be passed to a serialization function 
                       (e.g., Serialisation::save_tables) to save the current TABLES 
                       state (the parsed corpus up to this point) to disk. This ensures 
                       that progress is saved incrementally.
                 */
                if (line_number % CORPUS_SERIALIZATION_CHECKPOINT_INTERVAL == 0)
                {
                    std::string fqn = CORPUS_SERIALIZATION_CHECKPOINT_FILENAME + std::string(".") + std::to_string(line_number) + std::string(".") + std::to_string(tnt) + std::string(".") + "intermediate_check_point_number" + std::string(CORPUS_SERIALIZATION_CHECKPOINT_EXTENSION);

                    try
                    {
                        serialisation.save_tables(tables, starting_bucket, starting_line, line_number, fqn);
                        std::cout<< "Written Checkpoint: " << fqn << std::endl; 
                    }
                    catch (const std::exception& e)
                    {
                        std::cerr << "Parser::build_hash_table(void) Error: " << e.what() << std::endl; 
                    }

                    starting_line = 0;
                    starting_bucket = 0;
                }
#endif
            }

            //line_number = 0;
            //token_number = 0;

            if (is_open())
            {
                _ifile.clear();
                _ifile.seekg(0);
            }

            //TABLES* tables = nullptr;

            try
            {
/*                
                tables->hash_to_word_record = hash_table;
                tables->word_id_to_hash = index_table;
                tables->lines = lines_head;

                tables->bucket_count = bucket_count;
                tables->bucket_used = bucket_used;

                tables->ref_count = 1;

                tables->maximum_tokens_per_line = mxntpl;
                tables->minimum_tokens_per_line = mnntpl;
                tables->total_tokens = tnt;
 */                
            }
            catch (const std::bad_alloc& e)
            {
                // Corpus can be large. This is a real possibility, not a formality.             
                throw std::runtime_error("Parser::build_hash_table(void) Error: " + std::string(e.what()));
            }
/*
    On successful completion, before returning a pointer to the new TABLES struct,
    the build function writes a versioned snapshot of the tables to disk. This way,
    the build is crash-safe: if the program is terminated for any reason (power loss,
    signal, user interrupt), the TABLES state can be restored from the last checkpoint,
    avoiding the need to re-parse the entire corpus. The checkpoint file is placed in
    PARSER_SERIALIZATION_DIR with a name following the pattern:
    "corpus.tables.lines_N.tokens_M.checkpoint_K.bin"
    where:
        N = number of lines processed
        M = total tokens processed (tnt)
        K = checkpoint sequence number (zero-based)
 */
#if CORPUS_SERIALIZATION_CHECKPOINT_INTERVAL > 0
            // Fully Qualified Name (FQN) of the checkpoint file:            
            std::string fqn = CORPUS_SERIALIZATION_CHECKPOINT_FILENAME + std::string(".") + std::to_string(line_number) + std::string(".") + std::to_string(tnt) + std::string(".") + "check_point_number" + std::string(CORPUS_SERIALIZATION_CHECKPOINT_EXTENSION);
            try
            {
                serialisation.save_tables(tables, starting_bucket, starting_line, line_number, fqn);
                std::cout<< "Written Checkpoint: " << fqn << std::endl; 
            }
            catch (const std::exception& e)
            {
                std::cerr << "Parser::build_hash_table(void) Error: " << e.what() << std::endl; 
            }

            starting_line = 0;
            starting_bucket = 0;
#endif      
            return tables;
        }

        // Iterator access
        Iterator begin()
        { 
            return Iterator(&_ifile);
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
        const std::string& ifilename() const
        { 
            return _ifile_name;
        }

        size_t get_bucket_count(void) const
        {
            return bucket_count;
        }

        size_t get_bucket_used(void) const
        {
            return bucket_used;
        }

        size_t get_mxntpl(void) const
        {
            return mxntpl;
        }

        size_t get_mnntpl(void) const
        {
            return mnntpl;
        }

        size_t get_nol(void) const
        {
            return nol;
        }

        size_t get_tnt(void) const
        {
            return tnt;
        }

        size_t get_vocab_size(void) const
        {
            return bucket_used;
        }
};

#endif // CSV_PARSER_LIB_PARSER_HH