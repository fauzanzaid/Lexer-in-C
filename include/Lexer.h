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
 * @param  dfa_ptr     Pointer to an initialized Dfa, which will do the scanning
 * @param  file_ptr    Pointer to an open FILE suitable for reading
 * @param  buffer_size Number of bytes to read at a time from input
 * @return             Pointer to initialized Lexer
 */
Lexer *Lexer_new(Dfa* dfa_ptr, FILE* file_ptr, int buffer_size);

/**
 * Deallocates the struct and all internally allocated memory. The Dfa will not
 * be freed by by this function , it must be free by the caller.
 * @param lxr_ptr Pointer to struct
 */
void Lexer_destroy(Lexer *lxr_ptr);


///////////////
// Evaluator //
///////////////

/**
 * Add evaluation function which will be called on reaching the specified @p
 * state, when no further transitions to a final state with the current scanned
 * string are possible
 * @param lxr_ptr           Pointer to struct
 * @param state             State at which the function will be called
 * @param evaluate_function User defined function which sets the type and data
 * of the @p Token passed as a pointer. The scanned string and its length are
 * also passed to the function for evaluation
 */
void Lexer_add_state_evaluator(Lexer *lxr_ptr, int state, void (*evaluate_function)(Token *, char *, int));

/**
 * Add evaluation function which will be called if no specfic function has been
 * assigned to the state. If not default evaluator is added, and a state
 * evaluator is not available, the call to @p Lexer_get_next_token() will return
 * an error
 * @param lxr_ptr           Pointer to struct
 * @param evaluate_function User defined function
 */
void Lexer_add_default_evaluator(Lexer *lxr_ptr, int state, void (*evaluate_function)(Token *, char *, int));


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


#endif
