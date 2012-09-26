#ifndef _INTERNAL_TOKENIZER_H
#define _INTERNAL_TOKENIZER_H

#include <stdbool.h>

#include "slconfig/slconfig.h"
#include "slconfig/internal/slconfig.h"

#define LF ('\x0A')
#define CR ('\x0D')

typedef enum
{
	TOKEN_STRING,
	TOKEN_DOLLAR,
	TOKEN_SEMICOLON,
	TOKEN_LEFT_BRACE,
	TOKEN_RIGHT_BRACE,
	TOKEN_COLON,
	TOKEN_DOUBLE_COLON,
	TOKEN_ASSIGN,
	TOKEN_ERROR,
	TOKEN_TILDE,
	TOKEN_COMMENT,
	TOKEN_HASH,
	TOKEN_EOF
} TOKEN_TYPE;

typedef struct
{
	TOKEN_TYPE type;
	SLCONFIG_STRING str;
	size_t line;
	bool own;
} TOKEN;

typedef struct
{
	CONFIG* config;
	SLCONFIG_STRING filename;
	SLCONFIG_STRING str;
	size_t line;
	SLCONFIG_VTABLE* vtable;
	TOKEN cur_token;
	bool gag_errors;
} TOKENIZER_STATE;

TOKEN _slc_get_next_token(TOKENIZER_STATE* state);
bool _slc_is_naked_string_character(char c);

#endif
