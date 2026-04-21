/*
    lib/parser/header.hh
    Q@hackers.pk
 */

#ifndef CSV_PARSER_LIB_PARSER_HEADER_HH
#define CSV_PARSER_LIB_PARSER_HEADER_HH

/*
    <fstream>: Provides functionality for file-based input/output operations.
               It allows us to open, read from, and write to files.
*/

/*
    <string>: Provides functionality for working with strings (dynamic sequences of characters).
              It allows us to create, manipulate, and compare strings.
*/

/*
    <sstream>: Provides functionality for working with strings as input/output streams.
               It allows us to treat strings as if they were files, enabling us to read from or write to them using stream operations (like `>>` and `<<`).
*/

#include <fstream>   // for std::ifstream
#include <string>    // for std::string, std::getline
#include <sstream>   // for std::stringstream

// Source Files
#include "lib/src/WordRecord.hh"
#include "lib/src/Iterator.hh"
#include "lib/src/Parser.hh"

#endif // CSV_PARSER_LIB_PARSER_HEADER_HH