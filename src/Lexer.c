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

	int symbol_counter_tokenized;
		// Global index of symbol upto which tokenization is complete
	int symbol_counter_read;
		// Global index of symbol upto which reading is complete

	// Buffer

	FILE *file_ptr;
	int buffer_size;
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

// Adds buffers to buffer_list if character at index does not exist yet. Frees
// unneeded buffers from buffer_list.
// Returns 0 on success. -1 on failure
static int buffer_list_update(Lexer *lxr_ptr, int index);

// Reads required number of characters from input and adds one buffer to
// buffer_list
static int buffer_list_add(Lexer *lxr_ptr);


////////////////////////////////
// Constructors & Destructors //
////////////////////////////////

Lexer *Lexer_new(Dfa* dfa_ptr, FILE* file_ptr, int buffer_size){
	Lexer *lxr_ptr = malloc( sizeof(Lexer) );

	lxr_ptr->dfa_ptr = dfa_ptr;
	Dfa_get_state_lists(dfa_ptr, &lxr_ptr->dfa_states, &lxr_ptr->dfa_len_states, NULL, NULL, NULL);

	lxr_ptr->symbol_counter_tokenized = 0;
	lxr_ptr->symbol_counter_read = 0;

	lxr_ptr->file_ptr = file_ptr;
	lxr_ptr->buffer_size = buffer_size;
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
	HashTable_add(lxr_ptr->evaluator_table, &state, evaluate_function);
}

void Lexer_add_default_evaluator(Lexer *lxr_ptr, int state, void (*evaluate_function)(Token *, char *, int)){
	lxr_ptr->evaluator_default = evaluate_function;
}


////////////
// Tokens //
////////////

Token *Lexer_get_next_token(Lexer *lxr_ptr){

	int dfa_run_status = DFA_RUN_RESULT_UNKNOWN;
	int bfr_update_status;

	while(1){
		Buffer *bfr_ptr = LinkedList_peek(lxr_ptr->buffer_list);

		if(bfr_ptr != NULL){
			dfa_run_status = Dfa_run(lxr_ptr->dfa_ptr, bfr_ptr->string, bfr_ptr->len_string, bfr_ptr->global_index_start);
		}

		if(bfr_ptr == NULL ||
			dfa_run_status == DFA_RUN_RESULT_MORE_INPUT ||
			dfa_run_status == DFA_RUN_RESULT_WRONG_INDEX){
			// Get how many characters have been processed by DFA
			int dfa_symbol_counter;
			Dfa_get_current_configuration(lxr_ptr->dfa_ptr, NULL, NULL, &dfa_symbol_counter);

			// Update buffer list to make sure expected character exists in it
			bfr_update_status = buffer_list_update(lxr_ptr, dfa_symbol_counter + 1);
			if(bfr_update_status == -1){
				// Error
				fprintf(stderr, "Error updating buffers\n");
				return NULL;
			}
		}

		else if(dfa_run_status == DFA_RUN_RESULT_TRAP){
			int dfa_retract_status;
			dfa_retract_status = Dfa_retract(lxr_ptr->dfa_ptr);

			int dfa_state;
			int dfa_symbol_counter;
			Dfa_get_current_configuration(lxr_ptr->dfa_ptr, &dfa_state, NULL, &dfa_symbol_counter);

			if(dfa_retract_status == DFA_RETRACT_RESULT_FAIL){
				// Lexical error in input
			}
			else if(dfa_retract_status == DFA_RETRACT_RESULT_SUCCESS){
				// Send string to evaluator
			}

		}

	}

}


////////////
// Buffer //
////////////

static int buffer_list_update(Lexer *lxr_ptr, int index){
	if(index <= lxr_ptr->symbol_counter_tokenized){
		// Unrecoverable condition
		return -1;
	}

	// Pop unneeded buffers
	while(1){
		// Check the last buffer in list
		Buffer *bfr_ptr = LinkedList_peekback(lxr_ptr->buffer_list);

		if(bfr_ptr == NULL){
			// No buffers exist in list
			break;
		}

		else if(bfr_ptr->global_index_end > lxr_ptr->symbol_counter_tokenized){
			// Buffer contains untokenized characters
			break;
		}

		else{
			LinkedList_popback(lxr_ptr->buffer_list);
		}
	}

	// Add required buffers
	while(1){
		// Check the first buffer in list
		Buffer *bfr_ptr = LinkedList_peek(lxr_ptr->buffer_list);

		if(bfr_ptr == NULL){
			// No buffers exist in list, add one
			int status = buffer_list_add(lxr_ptr);
			if(status == -1){
				// Error while reading
				return -1;
			}
		}

		else if(bfr_ptr->global_index_end >= index){
			// Required character in buffer
			break;
		}

		else{
			// Required character not in buffer, add
			int status = buffer_list_add(lxr_ptr);
			if(status == -1){
				// Error while reading
				return -1;
			}
		}
	}

	return 0;
}

static int buffer_list_add(Lexer *lxr_ptr){
	Buffer *bfr_ptr = Buffer_new(lxr_ptr->buffer_size);

	bfr_ptr->len_string = fread(bfr_ptr->string, sizeof(char), lxr_ptr->buffer_size, lxr_ptr->file_ptr);
	bfr_ptr->global_index_start = 1 + lxr_ptr->symbol_counter_read;
	bfr_ptr->global_index_end = bfr_ptr->global_index_start + bfr_ptr->len_string - 1;

	lxr_ptr->symbol_counter_read += bfr_ptr->len_string;

	LinkedList_push(lxr_ptr->buffer_list, bfr_ptr);

	if(bfr_ptr->len_string < lxr_ptr->buffer_size){
		// EOF or error

		if( feof(lxr_ptr->file_ptr) ){
			// EOF
			// Add a buffer containing only the EOF constant
			Buffer *bfr_eof_ptr = Buffer_new( sizeof(char) );

			*(bfr_eof_ptr->string) = 0x04;	// EOT character
			bfr_eof_ptr->len_string = 1;
			bfr_ptr->global_index_start = 1 + lxr_ptr->symbol_counter_read;
			bfr_ptr->global_index_end = bfr_ptr->global_index_start;

			lxr_ptr->symbol_counter_read += 1;

			LinkedList_push(lxr_ptr->buffer_list, bfr_eof_ptr);
		}

		else{
			// Error
			fprintf(stderr, "Error reading at position %d\n", lxr_ptr->symbol_counter_read);
			return -1;
		}
	}

	return 0;
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
