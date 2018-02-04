#include <stdlib.h>
#include <stdio.h>

#include "Lexer.h"


/////////////////////
// Data Structures //
/////////////////////

typedef struct Lexer Lexer;


////////////////////////////////
// Constructors & Destructors //
////////////////////////////////

Lexer *Lexer_new(Dfa* dfa_ptr, FILE* file_ptr, int buffer_size{
	
}

void Lexer_destroy(Lexer *lxr_ptr{
	
}


///////////////
// Evaluator //
///////////////

void Lexer_add_state_evaluator(Lexer *lxr_ptr, int state, void (*evaluate_function)(Token *, char *, int){
	
}

void Lexer_add_default_evaluator(Lexer *lxr_ptr, int state, void (*evaluate_function)(Token *, char *, int){
	
}


////////////
// Tokens //
////////////

Token *Lexer_get_next_token(Lexer *lxr_ptr{
	
}
