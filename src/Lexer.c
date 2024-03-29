#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "Lexer.h"
#include "Dfa.h"
#include "LinkedList.h"


///////////////
// Constants //
///////////////

#define LEXER_MAX_CHAR 20

#define TEXT_RED	"\x1B[31m"
#define TEXT_GRN	"\x1B[32m"
#define TEXT_YLW	"\x1B[33m"
#define TEXT_BLD	"\x1B[1m"
#define TEXT_RST	"\x1B[0m"


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

	int symbol_counter_tokenized;
		// Global index of symbol upto which tokenization is complete
	int line_counter_tokenized;
		// Line number being scanned currently
	int column_counter_tokenized;
		// Column number upto which tokenization is complete
	int symbol_counter_read;
		// Global index of symbol upto which reading is complete

	int flag_errors_found;
		// Errors have been found while lexing
	int flag_error_recovery;
		// Error recovery is active
	int flag_immediate_print_error;
		// Print error as soon as it is encountered

	// Buffer

	FILE *file_ptr;
	int buffer_size;
	LinkedList *buffer_list;
	LinkedList *error_list;

	// Evaluators

	char * (*success_evaluate_function)(Token *, int, char *, int);
	char * (*error_evaluate_function)(Token *, int, char *, int);

} Lexer;

typedef struct ErrorBuffer{
	int line;
	int column;
	char *string;
	int len_string;
	char *error;
} ErrorBuffer;


/////////////////////////////////
// Private function prototypes //
/////////////////////////////////

static Buffer *Buffer_new(int size);

static void Buffer_destroy(Buffer *bfr_ptr);

// Gets the buffer in which character at index exists. Adds buffers to
// buffer_list if character at index does not exist yet. Frees unneeded
// buffers from buffer_list. Returns ptr on success. NULL on failure
static Buffer* buffer_list_get_buffer(Lexer *lxr_ptr, int index);

// Reads required number of characters from input and adds one buffer to
// buffer_list
static int buffer_list_add(Lexer *lxr_ptr);

// Gets a string starting from global index of length len. String is copied
// into dst. Returns 0 on success, -1 on failure
static int buffer_list_get_string(Lexer *lxr_ptr, char *dst, int index, int len);

static ErrorBuffer *ErrorBuffer_new(Token *tkn_ptr, char *string, int len_string, char *error);

static void ErrorBuffer_destroy(ErrorBuffer *bfr_ptr);

static void add_error(Lexer *lxr_ptr, Token *tkn_ptr, char *string, int len_string, char *error);

static void print_error(ErrorBuffer *err_ptr);


////////////////////////////////
// Constructors & Destructors //
////////////////////////////////

Lexer *Lexer_new(Dfa* dfa_ptr, FILE* file_ptr, int buffer_size,
	char * (*success_evaluate_function)(Token *, int, char *, int),
	char * (*error_evaluate_function)(Token *, int, char *, int)
	){

	Lexer *lxr_ptr = malloc( sizeof(Lexer) );
	lxr_ptr->dfa_ptr = dfa_ptr;

	lxr_ptr->symbol_counter_tokenized = 0;
	lxr_ptr->line_counter_tokenized = 1;
	lxr_ptr->column_counter_tokenized = 0;
	lxr_ptr->symbol_counter_read = 0;

	lxr_ptr->flag_errors_found = 0;
	lxr_ptr->flag_error_recovery = 0;
	lxr_ptr->flag_immediate_print_error = 0;

	lxr_ptr->file_ptr = file_ptr;
	lxr_ptr->buffer_size = buffer_size;
	lxr_ptr->buffer_list = LinkedList_new();
	lxr_ptr->error_list = LinkedList_new();

	lxr_ptr->success_evaluate_function = success_evaluate_function;
	lxr_ptr->error_evaluate_function = error_evaluate_function;
}

void Lexer_destroy(Lexer *lxr_ptr){
	// Free all buffers
	while(1){
		Buffer *bfr_ptr = LinkedList_pop(lxr_ptr->buffer_list);
		if(bfr_ptr == NULL)	break;
		else	Buffer_destroy(bfr_ptr);
	}
	LinkedList_destroy(lxr_ptr->buffer_list);

	// Free all error buffers
	while(1){
		ErrorBuffer *bfr_ptr = LinkedList_pop(lxr_ptr->error_list);
		if(bfr_ptr == NULL)	break;
		else	ErrorBuffer_destroy(bfr_ptr);
	}
	LinkedList_destroy(lxr_ptr->error_list);

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

static ErrorBuffer *ErrorBuffer_new(Token *tkn_ptr, char *string, int len_string, char *error){
	ErrorBuffer *bfr_ptr = malloc( sizeof(ErrorBuffer) );

	bfr_ptr->line = tkn_ptr->line;
	bfr_ptr->column = tkn_ptr->column;

	bfr_ptr->string = malloc( sizeof(char) * len_string );
	strncpy(bfr_ptr->string, string, len_string);

	// Replace non printable chars with space
	for (int i = 0; i < len_string; ++i)
		if( !isprint(bfr_ptr->string[i]) )
			bfr_ptr->string[i] = ' ';

	bfr_ptr->len_string = len_string;
	bfr_ptr->error = error;
}

static void ErrorBuffer_destroy(ErrorBuffer *bfr_ptr){
	free(bfr_ptr->string);
	free(bfr_ptr);
}


////////////
// Tokens //
////////////

Token *Lexer_get_next_token(Lexer *lxr_ptr){

	int dfa_run_status = DFA_RUN_RESULT_UNKNOWN;
	int bfr_update_status;

	// Set to start state, invalidate last final state
	Dfa_reset_state(lxr_ptr->dfa_ptr);

	while(1){

		// Get how many characters have been processed by DFA
		int dfa_symbol_counter;
		Dfa_get_current_configuration(lxr_ptr->dfa_ptr, NULL, NULL, &dfa_symbol_counter);

		// Update buffer list to make sure expected character exists in it and
		// get the required buffer
		Buffer *bfr_ptr = buffer_list_get_buffer(lxr_ptr, dfa_symbol_counter + 1);

		if(bfr_ptr == NULL){
			// Unrecoverable condition
			fprintf(stderr, "Error updating buffers\n");
			return NULL;
		}

		// Run Dfa
		dfa_run_status = Dfa_run(lxr_ptr->dfa_ptr, bfr_ptr->string, bfr_ptr->len_string, bfr_ptr->global_index_start);

		if(dfa_run_status == DFA_RUN_RESULT_TRAP){
			// Token (success or error) can be sent

			int dfa_retract_status;
			dfa_retract_status = Dfa_retract(lxr_ptr->dfa_ptr);

			int dfa_state;
			int dfa_symbol_counter;
			Dfa_get_current_configuration(lxr_ptr->dfa_ptr, &dfa_state, NULL, &dfa_symbol_counter);

			Token* tkn_ptr = Token_new();
			tkn_ptr->line = lxr_ptr->line_counter_tokenized;
			tkn_ptr->column = 1 + lxr_ptr->column_counter_tokenized;

			// Get len. If Error, increment by one so that error character can
			// be included in string
			int len_string = dfa_symbol_counter - lxr_ptr->symbol_counter_tokenized;
			if(dfa_retract_status == DFA_RETRACT_RESULT_FAIL)	len_string++;

			// Extract the string from buffers
			char string[len_string];
			buffer_list_get_string(lxr_ptr, string, 1 + lxr_ptr->symbol_counter_tokenized, len_string);

			// Modify the token
			tkn_ptr->len = len_string;
			tkn_ptr->position = dfa_symbol_counter - len_string + 1;

			if(dfa_retract_status == DFA_RETRACT_RESULT_FAIL){
				// Scanning error in input
				lxr_ptr->flag_errors_found = 1;

				if(lxr_ptr->flag_error_recovery == 1){
					// Error recovery already active, no need to record additional error
				}
				else{
					// Enable error recovery, record error
					char *error = lxr_ptr->error_evaluate_function(tkn_ptr, dfa_state, string, len_string);
					add_error(lxr_ptr, tkn_ptr, string, len_string, error);
				}


				// Skip invalid character index
				Dfa_skip(lxr_ptr->dfa_ptr);
			}

			else if(dfa_retract_status == DFA_RETRACT_RESULT_SUCCESS){
				// Scanning successful

				// Disable error recovery
				lxr_ptr->flag_error_recovery = 0;

				// Check for other errors
				char *error = lxr_ptr->success_evaluate_function(tkn_ptr, dfa_state, string, len_string);

				if(error != NULL){
					lxr_ptr->flag_errors_found = 1;
					add_error(lxr_ptr, tkn_ptr, string, len_string, error);
				}
			}

			// Calculate number of LF characters in string and set column
			// counter
			int num_LF = 0;
			for (int i = 0; i < len_string; ++i){
				if(string[i] == '\n'){
					num_LF += 1;
					lxr_ptr->column_counter_tokenized = 0;
				}
				else{
					lxr_ptr->column_counter_tokenized++;
				}
			}

				// Increment lxr_ptr's other counters
				lxr_ptr->symbol_counter_tokenized += len_string;
				lxr_ptr->line_counter_tokenized += num_LF;


			// Return the token after evaluation
			return tkn_ptr;
		}

		else if(dfa_run_status == DFA_RUN_RESULT_WRONG_INDEX){
			// Unrecoverable condition
			fprintf(stderr, "Error in getting correct index character\n");
			return NULL;
		}

		else if(dfa_run_status == DFA_RUN_RESULT_MORE_INPUT){
			// Dfa requires more input
			continue;
		}
	}
}


////////////
// Buffer //
////////////

static Buffer* buffer_list_get_buffer(Lexer *lxr_ptr, int index){

	if(index <= lxr_ptr->symbol_counter_tokenized){
		// Unrecoverable condition
		return NULL;
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
			Buffer_destroy( LinkedList_popback(lxr_ptr->buffer_list) );
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
				return NULL;
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
				return NULL;
			}
		}
	}

	// Search for buffer to return
	LinkedListIterator *itr_ptr = LinkedListIterator_new(lxr_ptr->buffer_list);
	LinkedListIterator_move_to_first(itr_ptr);
	while(1){
		Buffer *bfr_ptr = LinkedListIterator_get_item(itr_ptr);

		if(bfr_ptr == NULL){
			// Impossible condition
			LinkedListIterator_destroy(itr_ptr);
			return NULL;
		}
		else if(bfr_ptr->global_index_start <= index && index <= bfr_ptr->global_index_end){
			// Character at index exists in this buffer
			LinkedListIterator_destroy(itr_ptr);
			return bfr_ptr;
		}
		else{
			// Check older buffers
			LinkedListIterator_move_to_next(itr_ptr);
		}
	}
}

static int buffer_list_add(Lexer *lxr_ptr){
	Buffer *bfr_ptr = Buffer_new(lxr_ptr->buffer_size);

	int char_read = fread(bfr_ptr->string, sizeof(char), lxr_ptr->buffer_size, lxr_ptr->file_ptr);
	bfr_ptr->len_string = char_read;

	bfr_ptr->global_index_start = 1 + lxr_ptr->symbol_counter_read;
	bfr_ptr->global_index_end = bfr_ptr->global_index_start + bfr_ptr->len_string - 1;

	lxr_ptr->symbol_counter_read += char_read;

	if(char_read != 0){
		// Only add if some characters have been read
		LinkedList_push(lxr_ptr->buffer_list, bfr_ptr);
	}
	else{
		// Otherwise free buffer
		Buffer_destroy(bfr_ptr);
	}

	if(char_read < lxr_ptr->buffer_size){
		// EOF or error

		if( feof(lxr_ptr->file_ptr) ){
			// EOF: Add a buffer containing the EOF control character, followed
			// by \n to trigger fail and retract to EOF state of the DFA
			Buffer *bfr_eof_ptr = Buffer_new( 2 );

			(bfr_eof_ptr->string)[0] = 0x04;	// EOT character
			(bfr_eof_ptr->string)[1] = '\n';
			bfr_eof_ptr->len_string = 2;
			bfr_eof_ptr->global_index_start = 1 + lxr_ptr->symbol_counter_read;
			bfr_eof_ptr->global_index_end = 1 + bfr_eof_ptr->global_index_start;

			lxr_ptr->symbol_counter_read += 2;

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

static int buffer_list_get_string(Lexer *lxr_ptr, char *dst, int index, int len){
	LinkedListIterator *itr_ptr = LinkedListIterator_new(lxr_ptr->buffer_list);
	LinkedListIterator_move_to_last(itr_ptr);

	Buffer *bfr_ptr;
	int char_copied = 0;	// Also index in dst where to copy

	// Iterate until index is in buffer. Then copy
	while(1){
		bfr_ptr = LinkedListIterator_get_item(itr_ptr);

		if(bfr_ptr == NULL){
			// Unrecoverable condition
			LinkedListIterator_destroy(itr_ptr);
			return -1;
		}

		else if(bfr_ptr->global_index_start > index + char_copied){
			// Unrecoverable condition
			LinkedListIterator_destroy(itr_ptr);
			return -1;
		}

		else if(bfr_ptr->global_index_end < index + char_copied){
			// Move to previous(newer) buffer
			LinkedListIterator_move_to_previous(itr_ptr);
		}

		else if(bfr_ptr->global_index_start <= (index + char_copied) &&
			bfr_ptr->global_index_end >= (index + char_copied))
		{
			// Index for copying is within this buffer. Copy

			int num_char_left = len - char_copied;
				// Number of characters which are left to be copied into dst

			int bfr_index = (index+char_copied) - bfr_ptr->global_index_start;
				// Index in buffer from which to start copying

			int num_char_available = bfr_ptr->global_index_end - (index+char_copied) + 1;
				// Number of characters which are available in the buffer from
				// appropriate index which can be copied

			if(num_char_left <= num_char_available){
				// More or equal number of characters in buffer than required
				// to be copied. No need to look into further buffers.

				// Copy all the left characters
				memcpy(dst + char_copied,
					bfr_ptr->string + bfr_index,
					num_char_left);

				// Increase char_copied
				char_copied += num_char_left;

				// return as no more characters need to be copied
				LinkedListIterator_destroy(itr_ptr);
				return 0;
			}

			else{
				// Less characters available to copy than required. Copy and
				// look into further buffers

				// Copy all available characters
				memcpy(dst + char_copied,
					bfr_ptr->string + bfr_index,
					num_char_available
					);

				// Increase char_copied
				char_copied += num_char_available;

				// Continue in the loop, moving to newer buffer
			}
		}
	}
}


////////////
// Errors //
////////////

void Lexer_print_errors(Lexer *lxr_ptr){
	LinkedListIterator *itr_ptr = LinkedListIterator_new(lxr_ptr->error_list);
	LinkedListIterator_move_to_first(itr_ptr);

	ErrorBuffer *err_ptr = LinkedListIterator_get_item(itr_ptr);
	while(err_ptr){
		print_error(err_ptr);
		LinkedListIterator_move_to_next(itr_ptr);
		err_ptr = LinkedListIterator_get_item(itr_ptr);
	}

	LinkedListIterator_destroy(itr_ptr);
}

void Lexer_set_immediate_print_error(Lexer *lxr_ptr, int val){
	lxr_ptr->flag_immediate_print_error = val;
}

static void add_error(Lexer *lxr_ptr, Token *tkn_ptr, char *string, int len_string, char *error){
	ErrorBuffer *err_ptr = ErrorBuffer_new(tkn_ptr, string, len_string, error);

	if(lxr_ptr->flag_immediate_print_error)
		print_error(err_ptr);

	LinkedList_pushback(lxr_ptr->error_list, err_ptr);
}

static void print_error(ErrorBuffer *err_ptr){
	printf( TEXT_BLD "%d:%d: " TEXT_RST, err_ptr->line, err_ptr->column);
	printf( TEXT_BLD TEXT_RED "lexical error: " TEXT_RST);

	if(err_ptr->string != NULL){
		printf("Got ");

		if(err_ptr->len_string > LEXER_MAX_CHAR){
			// Truncate string
			printf("\"" TEXT_BLD TEXT_YLW "" "%.*s" TEXT_RST "...\"", LEXER_MAX_CHAR, err_ptr->string);
		}
		else{
			// Print as is. String is not null terminated, use len_string
			printf("\"" TEXT_BLD TEXT_YLW "" "%.*s" TEXT_RST "\"", err_ptr->len_string, err_ptr->string);
		}
		printf(". ");
	}

	if(err_ptr->error != NULL){
		printf(TEXT_BLD TEXT_GRN "%s" TEXT_RST, err_ptr->error);
	}

	printf("\n");
}
