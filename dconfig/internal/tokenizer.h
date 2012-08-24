#ifndef INTERNAL_TOKENIZER_H
#define INTERNAL_TOKENIZER_H

#include <stdbool.h>

#include "dconfig/dconfig.h"

#define LF ('\x0A')
#define CR ('\x0D')

typedef enum
{
	TOKEN_DCONFIG_STRING,
	TOKEN_DOLLAR,
	TOKEN_SEMICOLON,
	TOKEN_LEFT_BRACE,
	TOKEN_RIGHT_BRACE,
	TOKEN_COLON,
	TOKEN_ASSIGN,
	TOKEN_ERROR,
	TOKEN_TILDE,
	TOKEN_COMMENT,
	TOKEN_EOF
} TOKEN_TYPE;

typedef struct
{
	TOKEN_TYPE type;
	DCONFIG_STRING str;
	size_t line;
	bool own;
} TOKEN;

typedef struct
{
	DCONFIG_STRING filename;
	DCONFIG_STRING str;
	size_t line;
	DCONFIG_VTABLE* vtable;
} TOKENIZER_STATE;

TOKEN _dcfg_get_next_token(TOKENIZER_STATE* state);

#endif
