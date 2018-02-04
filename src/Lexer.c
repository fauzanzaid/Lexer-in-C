#include <stdlib.h>
#include <stdio.h>

#include "Lexer.h"
#include "Dfa.h"
#include "LinkedList.h"
#include "HashTable.h"


/////////////////////
// Data Structures //
/////////////////////

typedef struct Buffer{
	int size;	// Size of buffer or alllocated memory
	char *string;	// Pointer to array of symbols

	int len_string;	// Number of valid symbols in buffer, always <= size
	int global_index_start;	// Global index of first symbol in buffer
	int global_index_end;	// Global index of last symbol in buffer

} Buffer;

typedef struct Lexer{

	// State

	Dfa *dfa_ptr;
	int *dfa_states;
	int dfa_len_states;
	int symbol_counter_complete;
		// Global index of symbol upto which tokenization is complete

	// Buffer

	LinkedList *buffer_list;


	// Evaluators

	HashTable *evaluator_table;
	void (*evaluator_default)(Token *, char *, int);

} Lexer;


/////////////////////////////////
// Private function prototypes //
/////////////////////////////////

static Buffer *Buffer_new(int size);
static void Buffer_destroy(Buffer *bfr_ptr);
static int hash_function(void *key);
static int key_compare(void *key1, void *key2);


////////////////////////////////
// Constructors & Destructors //
////////////////////////////////

Lexer *Lexer_new(Dfa* dfa_ptr, FILE* file_ptr, int buffer_size){
	Lexer *lxr_ptr = malloc( sizeof(Lexer) );

	lxr_ptr->dfa_ptr = dfa_ptr;
	lxr_ptr->symbol_counter_complete = 0;
	Dfa_get_state_lists(dfa_ptr, &lxr_ptr->dfa_states, &lxr_ptr->dfa_len_states, NULL, NULL, NULL);

	lxr_ptr->buffer_list = LinkedList_new();

	lxr_ptr->evaluator_table = HashTable_new(lxr_ptr->dfa_len_states, hash_function, key_compare);
}

void Lexer_destroy(Lexer *lxr_ptr){
	// Free all buffers
	while(1){
		Buffer *bfr_ptr = LinkedList_pop(lxr_ptr->buffer_list);
		if(bfr_ptr == NULL)	break;
		else	Buffer_destroy(bfr_ptr);
	}

	LinkedList_destroy(lxr_ptr->buffer_list);
	HashTable_destroy(lxr_ptr->evaluator_table);

	free(lxr_ptr);
}

static Buffer *Buffer_new(int size){
	Buffer *bfr_ptr = malloc( sizeof(Buffer) );
	bfr_ptr->string = malloc( sizeof(char) * size );
	bfr_ptr->size = size;
}

static void Buffer_destroy(Buffer *bfr_ptr){
	free(bfr_ptr->string);
	free(bfr_ptr);
}


///////////////
// Evaluator //
///////////////

void Lexer_add_state_evaluator(Lexer *lxr_ptr, int state, void (*evaluate_function)(Token *, char *, int)){

}

void Lexer_add_default_evaluator(Lexer *lxr_ptr, int state, void (*evaluate_function)(Token *, char *, int)){

}


////////////
// Tokens //
////////////

Token *Lexer_get_next_token(Lexer *lxr_ptr){

}


///////////
// Other //
///////////

int hash_function(void *key){
	return (*(int *)(key));
}

int key_compare(void *key1, void *key2){
	return *(int *)(key1) - *(int *)(key2);
}
