#include "slconfig/internal/tokenizer.h"
#include "slconfig/internal/utils.h"

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

static
bool is_naked_string_character(char c)
{
	return !is_whitespace(c) && !is_newline(c) &&
	       c != ':' && c != '$' && c != '{' && c != '~' &&
	       c != '}' && c != ';' && c != '=' && c != '"';
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
bool token_string(SLCONFIG_STRING *str, TOKEN* token, TOKENIZER_STATE* state)
{
	SLCONFIG_STRING pre_quote;
	SLCONFIG_STRING post_quote;
	bool first_quote = false;
	bool second_quote = false;
	size_t start_line = state->line;
	
	pre_quote.start = str->start;
	while(str->start < str->end)
	{
		if(second_quote)
		{
			post_quote.end = str->start;
			
			if(slc_string_equal(pre_quote, post_quote))
			{
				if(is_naked_string_character(*str->start))
				{
					_slc_print_error_prefix(state->config, state->filename, state->line, state->vtable);
					state->vtable->stderr(slc_from_c_str("Error: Unexpected character '"));
					SLCONFIG_STRING c = {str->start, str->start + 1};
					state->vtable->stderr(c);
						
					if(slc_string_length(pre_quote) == 0)
						state->vtable->stderr(slc_from_c_str("' after quoted string.\n"));
					else
						state->vtable->stderr(slc_from_c_str("' after final heredoc string sentinel.\n"));
					token->type = TOKEN_ERROR;
				}
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
			else
			{
				post_quote.start = str->start + 1;
				second_quote = true;
			}
		}
		
		if(!first_quote && !is_naked_string_character(*str->start))
		{
			pre_quote.end = str->start;
			goto exit;
		}
		
		if(!eat_newline(str, &state->line))
			str->start++;
	}
	
	_slc_print_error_prefix(state->config, state->filename, start_line, state->vtable);
	state->vtable->stderr(slc_from_c_str("Error: Unterminated string.\n"));
	token->type = TOKEN_ERROR;
	return true;
exit:
	if(second_quote)
	{
		token->str.start = pre_quote.end + 1;
		token->str.end = post_quote.start - 1;
	}
	else
	{
		token->str = pre_quote;
	}
	token->type = TOKEN_STRING;
	return true;
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

			_slc_print_error_prefix(state->config, state->filename, start_line, state->vtable);
			state->vtable->stderr(slc_from_c_str("Error: Unterminated block comment.\n"));
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
