#ifndef INCLUDE_GUARD_8650ED356B0B48CCB6D48AE0D493C4BB
#define INCLUDE_GUARD_8650ED356B0B48CCB6D48AE0D493C4BB

#include <stdio.h>

#include "Dfa.h"
#include "Token.h"

/////////////////////
// Data Structures //
/////////////////////

/**
 * Opaque struct to hold a Lexer
 */
typedef struct Lexer Lexer;


////////////////////////////////
// Constructors & Destructors //
////////////////////////////////

/**
 * Allocates and returns a pointer to a @p Lexer struct
 * @param  dfa_ptr                   Pointer to an initialized Dfa, which will
 * do the scanning
 * @param  file_ptr                  Pointer to an open FILE suitable for
 * reading
 * @param  buffer_size               Number of bytes to read at a time from
 * input
 * @param  success_evaluate_function User defined function which sets the type
 * and data of the @p Token passed as a pointer. User is responsible for
 * allocateing and freeing the data of thr tken. The state, scanned string and
 * its length are also passed to the function for evaluation. In case of
 * evaluation failure, the function returns a pointer to a null terminated error
 * message buffer
 * @param error_evaluate_function    User defined function with a pointer to a
 * Token passed as a parameter, which performs the same as above function. This
 * function will be called when the dfa cannot transition on given input
 * @return                           Pointer to initialized Lexer
 */
Lexer *Lexer_new(Dfa* dfa_ptr, FILE* file_ptr, int buffer_size,
	char * (*success_evaluate_function)(Token *, int, char *, int),
	char * (*error_evaluate_function)(Token *, int, char *, int)
	);

/**
 * Deallocates the struct and all internally allocated memory. The Dfa will not
 * be freed by by this function , it must be free by the caller.
 * @param lxr_ptr Pointer to struct
 */
void Lexer_destroy(Lexer *lxr_ptr);


////////////
// Tokens //
////////////

/**
 * Reads the input and returns the next token
 * @param  lxr_ptr Pointer to struct
 * @return         A heap allocated token. Must be freed by a call to @p
 * Token_destroy() after using. Returns NULL in case of an error
 */
Token *Lexer_get_next_token(Lexer *lxr_ptr);


////////////
// Errors //
////////////

/**
 * Prints information about each error encountered so far
 * @param lxr_ptr Pointer to Lexer struct
 */
void Lexer_print_errors(Lexer *lxr_ptr);


#endif
