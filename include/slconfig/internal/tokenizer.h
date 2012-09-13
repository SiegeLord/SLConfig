#ifndef _INTERNAL_TOKENIZER_H
#define _INTERNAL_TOKENIZER_H

#include <stdbool.h>

#include "slconfig/slconfig.h"

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
	SLCONFIG* config;
	SLCONFIG_STRING filename;
	SLCONFIG_STRING str;
	size_t line;
	SLCONFIG_VTABLE* vtable;
	TOKEN cur_token;
} TOKENIZER_STATE;

TOKEN _slc_get_next_token(TOKENIZER_STATE* state);

#endif
