/*
    lib/Parser/header.hh

    This header file serves as a central inclusion point for all necessary headers related to the Parser class and its dependencies. 
    It includes the Serialisation header from the Corpus library, which defines the structures and functions needed for reading and writing corpus data, as well as the Parser header itself, which defines the Parser class.

    Maintainer: Sohail.    
 */

/*    
 * ============================================================================
 *  IMPORTANT NOTE – PLEASE READ EVERYTHING BELOW CAREFULLY
 * ============================================================================
 *
 *  THE TOKEN ID MAPPINGS, SPECIAL SENTINELS ([PAD], [UNK], [CLS], [SEP], [MASK]),
 *  AND CONFIGURATION MACROS (MAX POSITIONS, EMBEDDING SIZE) DEFINED IN THIS
 *  HEADER ARE STRUCTURED AROUND THE STANDARD BERT/GPT TRANSFORMER ARCHITECTURE.
 *
 *  HOWEVER, PLEASE BE AWARE THAT THE UNDERLYING PARSER LOGIC AND TOKEN
 *  MANAGEMENT DEFINED BELOW ARE COMPLETELY GENERIC. THEY CAN BE USED IN ANY
 *  CONTEXT WHERE YOU NEED TO HANDLE SEQUENTIAL TOKENS, APPLY PADDING, OR
 *  MANAGE UNKNOWN WORDS – REGARDLESS OF THE ACTUAL MODEL ARCHITECTURE.
 *
 *  BUT OUR PRIMARY AND ORIGINAL INTENTION FOR DESIGNING THIS PARSER AND ITS
 *  ASSOCIATED INPUT HANDLING WAS TO BUILD A DEDICATED INPUT PIPELINE
 *  SPECIFICALLY FOR TRANSFORMER-TYPE MODELS (SUCH AS BERT, GPT, AND
 *  OTHER VARIATIONS OF THE TRANSFORMER FAMILY).
 *
 *  WITH THAT IN MIND, PLEASE READ AND UNDERSTAND EVERY SINGLE DEFINITION AND
 *  COMMENT PROVIDED BELOW, AS THEY FORM THE FOUNDATION OF THIS TOKENIZATION
 *  AND INPUT PIPELINE SYSTEM.
 * ============================================================================
 */

/*
    Limiting the vocabulary size
    ----------------------------
    In the context of natural language processing (NLP) and machine learning, the vocabulary size refers to the number of unique tokens (words or symbols) that a model can recognize and process.
    Limiting the vocabulary size is important for several reasons:
    Memory Efficiency: A smaller vocabulary size reduces the memory footprint of the model, making it more efficient to store and process the data.
        Let V be your vocabulary size, and d be your hidden dimension (e.g., d = 768 for BERT-base).
        Embedding Matrix Memory: 
            - With V = 500,000 unique words and d = 768 of type float32, the embedding matrix would require:
                Memory = V * d * sizeof(float32) = 500,000 * 768 * 4 bytes ≈ 1.5 GB. Just holding the embedding lookup table requires 1.53 GB of memory before you even create a single transformer layer or compute gradients!
            - With V = 50,000 unique words and d = 768 of type float32, the embedding matrix would require:
                Memory = V * d * sizeof(float32) = 50,000 * 768 * 4 bytes ≈ 153 MB. This is a significant reduction in memory usage, making it feasible to train and deploy models on hardware with limited resources.        
        Thus a smaller vocabulary size can lead to faster training and inference times, as the model has fewer parameters to learn and fewer computations to perform.

        The Unknown ([UNK]) Token Strategy
        ----------------------------------
        When you sort your unique words by frequency (n) and pick the top N words (e.g., 20,000 or 30,000):
        - Words that are part of  training data but not in the top N are replaced with a special token called the "unknown" token, often represented as [UNK].
        - Reserve ID 1 (or another fixed index) for [UNK] (Unknown word).        
 */ 

#ifndef CSV_PARSER_LIB_PARSER_HEADER_HH
#define CSV_PARSER_LIB_PARSER_HEADER_HH

/*
   [PAD] / [UNK] / [CLS] / [SEP] / [MASK] reserved range
   -----------------------------------------------------
   TOKEN_ID_ORIGINATE_AT_VALUE defines the first valid vocabulary ID. It is an
   offset, not a dense zero-based vocabulary index. Values below this offset are
   reserved for special sentinel slots.

   With the current default value of 5:
   - ID 0: Reserved for padding ([PAD])
   - ID 1: Reserved for unknown words ([UNK])
   - ID 2: Reserved for [CLS]
   - ID 3: Reserved for [SEP]
   - ID 4: Reserved for [MASK]
   - IDs 5 .. 5 + bucket_used - 1: Active vocabulary IDs

   The valid vocabulary range is therefore:
       [TOKEN_ID_ORIGINATE_AT_VALUE, TOKEN_ID_ORIGINATE_AT_VALUE + bucket_used)

   This is intentionally different from a dense 0-based vocabulary index and is
   used by the parser's index table to keep special sentinel values out of the
   active vocabulary range.
 */
#ifndef TOKEN_ID_ORIGINATE_AT_VALUE
#define TOKEN_ID_ORIGINATE_AT_VALUE 5 // First active vocabulary ID. IDs 0..4 are reserved special tokens.
#endif
/* 
   TOKEN_ID_ORIGINATE_AT_VALUE in BERT/GPT/Transformer context:
   - ID 0: Reserved for padding ([PAD] token)
   - ID 1: Reserved for unknown words ([UNK] token)
   - ID 2: Reserved for [CLS] token (start of sequence)
   - ID 3: Reserved for [SEP] token (separator between segments)
   - ID 4: Reserved for [MASK] token (used in masked language modeling)
   - IDs 5 and above: Active vocabulary IDs, offset by this macro

#define BERT_CLS_TOKEN_ID 2 // Reserved for [CLS] token ID, the first token of every sequence/line.
#define BERT_SEP_TOKEN_ID 3 // Reserved for [SEP] token ID, used to separate different segments of the input sequence.
#define BERT_MASK_TOKEN_ID 4 // Reserved for [MASK] token ID, used for masked language modeling tasks.
 
   BERT / GPT / Transformer Initialization
   ---------------------------------------
   The following macro defines the maximum number of positions (tokens) that a transformer model can handle in a single input sequence.
   This is important for models like BERT and GPT, which have a fixed-length input representation.
   The value 512 is commonly used in many transformer architectures, allowing the model to process sequences of up to 512 tokens.
 
#define BERT_MAX_POSITIONS 512
#define BERT_EMBEDDING_SIZE 768
 */

#ifndef PARSER_PADDING_VALUE
#define PARSER_PADDING_VALUE 0 // Reserved for padding ([PAD] token ID), 
                               // This is the index into the embedding table.
                               // This slot of the embedding table is initialized to all zeros and remains zero throughout training.
                               // This value represent any token that is not part of the training data. 
                               // This is also the default value for the left/right context arrays of a single token when there are no valid tokens in that context window.
#endif
#ifndef PARSER_UNKNOWN_VALUE
#define PARSER_UNKNOWN_VALUE 1 // Reserved for unknown words ([UNK] token ID)
#endif

#define CORPUS_SERIALIZATION_FINAL_FILENAME "corpus_final.bin"

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

#include <fstream>   // For std::ifstream
#include <string>    // For std::string, std::getline
#include <sstream>   // For std::stringstream
#include <vector>    // For std::vector
#include <algorithm> // For std::sort, std::find, std::remove_if

/*
    While using (0 - 1) for an unsigned type technically works because of unsigned wrap-around in C++,
    it is often considered a "magic number" trick that can be unclear to others.
    Instead, we use std::numeric_limits<size_t>::max() to get the maximum value of size_t,
    which is a more explicit and self-documenting way to achieve the same result.
*/
#include <limits>    // For std::numeric_limits

/*
    Circular Include Reason: Parser will be used by Corpus and Corpus will be used by Parser.
    Consequence: The include of both of these packages remain order free.
*/
#include "./../Corpus/header.hh"

// Source Files
#include "lib/src/WordRecord.hh"
#include "lib/src/Iterator.hh"
#include "lib/src/Parser.hh"

#endif // CSV_PARSER_LIB_PARSER_HEADER_HH