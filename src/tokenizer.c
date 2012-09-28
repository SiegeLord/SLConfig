/* Copyright 2012 Pavel Sountsov
 * 
 * This file is part of SLConfig.
 *
 * SLConfig is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SLConfig is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser Public License for more details.
 *
 * You should have received a copy of the GNU Lesser Public License
 * along with SLConfig.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "slconfig/internal/tokenizer.h"
#include "slconfig/internal/utils.h"

#include <string.h>
#include <assert.h>

static
bool is_whitespace(char c)
{
	return c == ' ' || c== '\t';
}

static
bool is_newline(char c)
{
	return c == CR || c == LF;
}

bool _slc_is_naked_string_character(char c)
{
	return !is_whitespace(c) && !is_newline(c) &&
	       c != ':' && c != '$' && c != '{' && c != '~' &&
	       c != '}' && c != ';' && c != '=' && c != '"' &&
	       c != '#';
}

static
SLCONFIG_STRING ltrim(SLCONFIG_STRING str)
{
	while(str.start < str.end && is_whitespace(*str.start))
		str.start++;
	return str;
}

static
bool eat_newline(SLCONFIG_STRING *str, size_t* line)
{
	bool ret = false;
	while(str->start < str->end)
	{
		if(*str->start == LF)
		{
			ret = true;
			str->start++;
			(*line)++;
			if(str->start < str->end && *str->start == CR)
				str->start++;
		}
		else if(*str->start == CR)
		{
			ret = true;
			str->start++;
			(*line)++;
			if(str->start < str->end && *str->start == LF)
				str->start++;
		}
		else
		{
			break;
		}
	}
	return ret;
}

static
bool token_character(SLCONFIG_STRING *str, TOKEN* token, char c, TOKEN_TYPE type)
{
	if(*str->start == c)
	{
		token->type = type;
		token->str.start = str->start;
		token->str.end = str->start + 1;
		
		str->start++;
		
		return true;
	}
	else
	{
		return false;
	}
}

static
bool escape_string(SLCONFIG_STRING *str, SLCONFIG_VTABLE* vtable)
{
	SLCONFIG_STRING source = *str;
	SLCONFIG_STRING source_iter = source;
	bool new_allocation = false;
	size_t offset = 0;
	
	while(source_iter.start < source_iter.end)
	{
		if(*source_iter.start == '\\')
		{
			if(!new_allocation)
			{
				str->start = vtable->realloc(0, slc_string_length(source));
				str->end = str->start;
				new_allocation = true;
			}
			
			size_t ammt = source_iter.start - source.start - offset;
			memcpy((char*)str->end, source.start + offset, ammt);
			str->end += ammt;
			offset += ammt + 2; //Skip both the '\' and the character after it
			
			source_iter.start++;
			assert(source_iter.start < source_iter.end);
			
			switch(*source_iter.start)
			{
				case '0':
					*(char*)str->end = '\0';
					break;
				case 'a':
					*(char*)str->end = '\a';
					break;
				case 'b':
					*(char*)str->end = '\b';
					break;
				case 'f':
					*(char*)str->end = '\f';
					break;
				case 'n':
					*(char*)str->end = LF;
					break;
				case 'r':
					*(char*)str->end = CR;
					break;
				case 't':
					*(char*)str->end = '\t';
					break;
				case 'v':
					*(char*)str->end = '\v';
					break;
				default:
					*(char*)str->end = *source_iter.start;
					break;
			}
			
			str->end++;
		}
		
		source_iter.start++;
	}
	
	if(new_allocation)
	{
		size_t ammt = source_iter.start - source.start - offset;
		memcpy((char*)str->end, source.start + offset, ammt);
		str->end += ammt;
	}
	
	return new_allocation;
}

static
bool token_string(SLCONFIG_STRING *str, TOKEN* token, TOKENIZER_STATE* state)
{
	SLCONFIG_STRING pre_quote;
	SLCONFIG_STRING post_quote;
	bool first_quote = false;
	bool second_quote = false;
	size_t start_line = state->line;
	bool escape = false;
	
	pre_quote.start = str->start;
	while(str->start < str->end)
	{
		if(second_quote)
		{
			post_quote.end = str->start;
			
			if(slc_string_equal(pre_quote, post_quote))
			{
				/*if(_slc_is_naked_string_character(*str->start))
				{
					_slc_print_error_prefix(state->config, state->filename, state->line, state->vtable);
					state->vtable->error(slc_from_c_str("Error: Unexpected character '"));
					SLCONFIG_STRING c = {str->start, str->start + 1};
					state->vtable->error(c);
						
					if(slc_string_length(pre_quote) == 0)
						state->vtable->error(slc_from_c_str("' after quoted string.\n"));
					else
						state->vtable->error(slc_from_c_str("' after final heredoc string sentinel.\n"));
					token->type = TOKEN_ERROR;
				}*/
				goto exit;
			}
		}
		
		if(*str->start == '"')
		{
			if(!first_quote)
			{
				pre_quote.end = str->start;
				first_quote = true;
			}
			else if(slc_string_length(pre_quote) != 0 || !escape)
			{
				post_quote.start = str->start + 1;
				second_quote = true;
			}
		}
		
		if(!first_quote && !_slc_is_naked_string_character(*str->start))
		{
			pre_quote.end = str->start;
			goto exit;
		}
		
		escape = !escape && *str->start == '\\'; 
		
		if(!eat_newline(str, &state->line))
			str->start++;
	}
	
	if(!first_quote)
	{
		pre_quote.end = str->start;
		goto exit;
	}
	else if(second_quote)
	{
		post_quote.end = str->start;
		if(slc_string_equal(pre_quote, post_quote))
			goto exit;
	}
	
	if(!state->gag_errors)
	{
		_slc_print_error_prefix(state->config, state->filename, start_line, state->vtable);
		state->vtable->error(slc_from_c_str("Error: Unterminated string.\n"));
	}
	token->type = TOKEN_ERROR;
	return true;
exit:
	if(second_quote)
	{
		SLCONFIG_STRING string_content;
		string_content.start = pre_quote.end + 1;
		string_content.end = post_quote.start - 1;
		if(!slc_string_length(pre_quote))
		{
			token->own = escape_string(&string_content, state->vtable);
		}
		
		token->str = string_content;
	}
	else
	{
		token->str = pre_quote;
	}
	token->type = TOKEN_STRING;
	return true;
}

static
bool token_double_colon(SLCONFIG_STRING* str, TOKEN* token)
{
	if(str->start < str->end && *str->start == ':')
	{
		str->start++;
		if(str->start < str->end && *str->start == ':')
		{
			str->start++;
			token->str.start = str->start;
			token->type = TOKEN_DOUBLE_COLON;
			token->str.end = str->start;
			
			return true;
		}
		else
		{
			str->start--;
		}
	}
	return false;
}

static
bool token_line_comment(SLCONFIG_STRING* str, TOKEN* token)
{
	if(str->start < str->end && *str->start == '/')
	{
		str->start++;
		if(str->start < str->end && *str->start == '/')
		{
			str->start++;
			token->str.start = str->start;
			token->type = TOKEN_COMMENT;
			while(str->start < str->end)
			{
				if(is_newline(*str->start))
					break;
				str->start++;
			}
			token->str.end = str->start;
			
			return true;
		}
		else
		{
			str->start--;
		}
	}
	return false;
}

static
bool token_block_comment(SLCONFIG_STRING* str, TOKEN* token, TOKENIZER_STATE* state)
{
	if(*str->start == '/')
	{
		str->start++;
		if(str->start < str->end && *str->start == '*')
		{
			int opened_comments = 1;
			size_t start_line = state->line;
			str->start++;
			
			token->str.start = str->start;
			while(str->start < str->end)
			{
				if(*str->start == '/')
				{
					str->start++;
					if(str->start < str->end && *str->start == '*')
					{
						opened_comments++;
						str->start++;
						continue;
					}
				}
				
				if(*str->start == '*')
				{
					str->start++;
					if(str->start < str->end && *str->start == '/')
					{
						opened_comments--;
						str->start++;
						continue;
					}
				}
				
				if(opened_comments == 0)
				{
					token->str.end = str->start - 2;
					goto exit;
				}

				if(!eat_newline(str, &state->line))
					str->start++;
			}

			if(!state->gag_errors)
			{
				_slc_print_error_prefix(state->config, state->filename, start_line, state->vtable);
				state->vtable->error(slc_from_c_str("Error: Unterminated block comment.\n"));
			}
			token->type = TOKEN_ERROR;
			return true;
		exit:
			token->type = TOKEN_COMMENT;
			return true;
		}
		else
		{
			str->start--;
		}
	}
	return false;
}

TOKEN _slc_get_next_token(TOKENIZER_STATE* state)
{
	const char* old_start;
	/* Eat whitespace */
	do
	{
		old_start = state->str.start;
		state->str = ltrim(state->str);
		eat_newline(&state->str, &state->line);
	} while(old_start != state->str.start);
	
	TOKEN ret;
	ret.own = false;
	ret.line = state->line;
	
	if(state->str.start == state->str.end)
	{
		ret.type = TOKEN_EOF;
		ret.str = slc_from_c_str("<EOF>");
		goto exit;
	}
	if(token_double_colon(&state->str, &ret))
		goto exit;
	if(token_character(&state->str, &ret, '$', TOKEN_DOLLAR))
		goto exit;
	if(token_character(&state->str, &ret, ';', TOKEN_SEMICOLON))
		goto exit;
	if(token_character(&state->str, &ret, '{', TOKEN_LEFT_BRACE))
		goto exit;
	if(token_character(&state->str, &ret, '}', TOKEN_RIGHT_BRACE))
		goto exit;
	if(token_character(&state->str, &ret, ':', TOKEN_COLON))
		goto exit;
	if(token_character(&state->str, &ret, '=', TOKEN_ASSIGN))
		goto exit;
	if(token_character(&state->str, &ret, '~', TOKEN_TILDE))
		goto exit;
	if(token_character(&state->str, &ret, '#', TOKEN_HASH))
		goto exit;
	if(token_line_comment(&state->str, &ret))
		goto exit;
	if(token_block_comment(&state->str, &ret, state))
		goto exit;
	if(token_string(&state->str, &ret, state))
		goto exit;
exit:
	state->cur_token = ret;
	return ret;
}
