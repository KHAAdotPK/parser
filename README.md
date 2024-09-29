### General Purpose Parser Interface
This C++ project defines an abstract template class `parser` within the `cc_tokenizer` namespace. The purpose of this interface is to provide a generic way to parse data as lines and tokens, where the exact definitions of lines and tokens are left to the implementer.

The parser interface is designed to be flexible and generic, allowing the handling of various types of content. It treats content as a series of lines, where each line is further divided into tokens based on a delimiter. The interface is fully abstract, meaning that any class implementing this interface will need to define the behavior for managing lines and tokens.

#### Template Parameters
- `T`: Represents the type of data that is returned by the `get_line` and `get_token` methods. This could be a string, or any other type that represents a line or a token.
- `E`: The character type used for string processing, e.g., `char` or `wchar_t`.

#### Namespaces and Traits
The parser uses the `cc_tokenizer::string_character_traits` class to manage the character types (`E`) and their associated traits, such as integer types for indexing.

#### Public Interface

The following methods are part of the public interface and must be implemented by any derived class:

##### Line-based Methods
- `T get_current_line()`: Returns the content of the current line.
- `T get_line_by_number(typename cc_tokenizer::string_character_traits<E>::int_type)`: Returns the content of the line by its number.
- `typename cc_tokenizer::string_character_traits<E>::int_type get_total_number_of_lines()`: Returns the total number of lines.
- `typename cc_tokenizer::string_character_traits<E>::int_type go_to_next_line()`: Moves to the next line and returns the line number.
- `typename cc_tokenizer::string_character_traits<E>::int_type get_current_line_number()`: Returns the current line number.
- `typename cc_tokenizer::string_character_traits<E>::int_type remove_line_by_number(typename cc_tokenizer::string_character_traits<E>::int_type)`: Removes a line by its number.

##### Token-based Methods
- `typename cc_tokenizer::string_character_traits<E>::int_type go_to_next_token()`: Moves to the next token and returns the token number.
- `T get_current_token()`: Returns the current token.
- `typename cc_tokenizer::string_character_traits<E>::int_type get_total_number_of_tokens()`: Returns the total number of tokens.
- `T get_token_by_number(typename cc_tokenizer::string_character_traits<E>::int_type)`: Returns the token by its number.
- `typename cc_tokenizer::string_character_traits<E>::int_type get_current_token_number()`: Returns the current token number.

Dependencies
The Parser class has a single dependency: the string library, which can be found at https://github.com/KHAAdotPK/string.git

#### Usage
This class is a pure virtual interface (abstract class), so it cannot be instantiated directly. To use this parser, you need to create a derived class that implements all the virtual methods defined above.

Here’s an example of how you might extend this parser:

```cpp
#include "parser.hh"

class MyParser : public cc_tokenizer::parser<std::string, char> {
    // Implement the virtual methods here
};
```

### License
This project is governed by a license, the details of which can be located in the accompanying file named 'LICENSE.' Please refer to this file for comprehensive information.


