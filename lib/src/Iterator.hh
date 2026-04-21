/*
    lib/src/Iterator.hh
    Q@hackers.pk
 */

#ifndef CSV_PARSER_LIB_ITERATOR_HH
#define CSV_PARSER_LIB_ITERATOR_HH

class Iterator
{
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type        = std::vector<std::string>;
        using difference_type   = std::ptrdiff_t;
        using pointer           = value_type*;
        using reference         = value_type&;

        // End iterator constructor
        Iterator() : _stream(nullptr), _current{}, _at_end(true) {}

        // Begin iterator constructor
        explicit Iterator(std::ifstream* stream) : _stream(stream), _current{}, _at_end(false)
        {
            if (_stream && _stream->is_open() && !_stream->eof())
            {
                read_next(); // read first line
            }
            else
            {
                _at_end = true;
            }
        }

        Iterator(const Iterator&) = default;
        Iterator& operator=(const Iterator&) = default;

        // Dereference
        reference operator*()
        { 
            return _current;
        }

        pointer operator->()
        { 
            return &_current; 
        }

        // Pre‑increment
        Iterator& operator++()
        {
            read_next();
            return *this;
        }

        // Post‑increment
        Iterator operator++(int)
        {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        // Comparison
        bool operator==(const Iterator& other) const
        {
            // If both are at end, they are equal (regardless of the stream pointer)
            if (_at_end && other._at_end)
            {
                return true;
            }
            
            // Otherwise compare stream pointer and end flag
            return _at_end == other._at_end && _stream == other._stream;
        }

        bool operator!=(const Iterator& other) const
        {
            return !(*this == other);
        }

    private:
        std::ifstream* _stream;
        std::vector<std::string> _current;
        bool _at_end;

        void read_next()
        {
            if (_at_end || !_stream)
            {
                return;
            }
            
            std::string line;
#ifdef ITERATOR_USER_DEFINED_CLEANER_CODE
            Cleaner cleaner;
#endif
            if (std::getline(*_stream, line))
            {
#ifdef ITERATOR_USER_DEFINED_CLEANER_CODE                
                /*
                    Line Cleaning
                    -------------
                    Remove punctuation and normalize whitespace.
                */
                line = cleaner.cleanLine(line);
#endif                

                _current.clear();
                std::stringstream ss(line);
                std::string field;
                
                // Simple CSV split on comma(default) or user defined delimiter(via macro)
#ifdef CSV_PARSER_TOKEN_DELIMITER
                while (std::getline(ss, field, CSV_PARSER_TOKEN_DELIMITER))
#else
                while (std::getline(ss, field, ','))
#endif                
                {
#ifdef ITERATOR_GUARD_AGAINST_EMPTY_STRING                    
                    // Guard against empty string.
                    // This string may get used in hash generation
                    if (field.empty())
                    {
                        continue;
                    }
#endif                    
                    _current.push_back(field);                                        
                }                
            }
            else
            {
                _at_end = true;
            }
        }
};

#endif // CSV_PARSER_LIB_ITERATOR_HH