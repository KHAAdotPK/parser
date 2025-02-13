/* 
   parser/parser.hh
   Q@khaa.pk
 */

/* A general purpose parser interface */

#include "../string/src/String.hh"

/*
    The public interface should be as such that methods like... 
        go_to_next_line,
        go_to_next_token,
        get_total_number_of_lines, 
        get_total_number_of_tokens, 
        get_line_by_number, 
        get_token_by_number, 
        get_current_line,
        get_current_token,
        max_sequence_lengt
    should be made public. Every thing else is to be made private and not available publicly at all(at implementer's discretion)
*/

#ifndef		CC_TOKENIZER_PARSER_HH
#define		CC_TOKENIZER_PARSER_HH

namespace cc_tokenizer 
{
    template<class T, typename E>
    class parser 
    {
        public:
            /* Pure virtual methods */
	  
            /* Treat the content as a set of lines(the definition of the line is provided by the implementer of this interface) */
            virtual T get_current_line(void) = 0;      
            virtual T get_line_by_number(typename cc_tokenizer::string_character_traits<E>::int_type) = 0; 
            virtual typename cc_tokenizer::string_character_traits<E>::int_type get_total_number_of_lines(void) = 0;      
            virtual typename cc_tokenizer::string_character_traits<E>::int_type go_to_next_line(void) = 0;
            virtual typename cc_tokenizer::string_character_traits<E>::int_type get_current_line_number(void) = 0;
	        virtual typename cc_tokenizer::string_character_traits<E>::int_type remove_line_by_number(typename cc_tokenizer::string_character_traits<E>::int_type) = 0;
	   	  
            /* Treat line as a delimited set of tokens(an ordinary character literal or multicharacter literal). Where the definition of the delimiter is provided by the implementer of this interface */
	        virtual typename cc_tokenizer::string_character_traits<E>::int_type go_to_next_token(void) = 0;
	        virtual T get_current_token(void) = 0;
	        virtual typename cc_tokenizer::string_character_traits<E>::int_type get_total_number_of_tokens(void) = 0;
	        virtual T get_token_by_number(typename cc_tokenizer::string_character_traits<E>::int_type) = 0;
            virtual typename cc_tokenizer::string_character_traits<E>::int_type get_current_token_number(void) = 0;
            
            /**
             * @brief Determines the maximum sequence length across all lines in the batch.
             * 
             * This method scans the entire corpus divided into few or several batches and identifies the 
             * longest tokenized sequence among all batches. 
             * The result represents the highest number of tokens found in any single batch within the 
             * corpus/vocabulary.
             * 
             * @return The size_type representing the length of the longest sequence(in number of tokens) 
             * in the batch.
             * 
             * @note This method is useful for padding shorter sequences to maintain uniform 
             *       input dimensions. Implementations should ensure efficiency when processing 
             *       large batches with many tokens.
             */
            virtual typename cc_tokenizer::string_character_traits<E>::size_type max_sequence_length(void) = 0;
    };
}

#endif
