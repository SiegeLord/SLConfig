#include "dconfig/internal/parser.h"
#include "dconfig/internal/tokenizer.h"
#include "dconfig/internal/utils.h"
#include "dconfig/dconfig.h"

#include <stdio.h>
#include <assert.h>

/* Note that the file is assumed to be owned by the config */
bool _dcfg_parse_file(DCONFIG* config, DCONFIG_NODE* root, DCONFIG_STRING filename, DCONFIG_STRING file)
{
	TOKENIZER_STATE state;
	state.filename = filename;
	state.str = file;
	state.line = 1;
	state.vtable = &config->vtable;
	
	config->files = config->vtable.realloc(config->files, (config->num_files + 1) * sizeof(DCONFIG_STRING));
	config->files[config->num_files++] = file;
	
	_dcfg_get_next_token(&state);
	return _dcfg_parse_aggregate(config, root, &state);
}

bool parse_left_hand_side(DCONFIG* config, DCONFIG_NODE* aggregate, DCONFIG_NODE** lhs_node, TOKENIZER_STATE* state)
{
	size_t name_line = state->line;
	/*
	name;
	name:name;
	type name;
	*/
	DCONFIG_STRING name;
	if(state->cur_token.type == TOKEN_STRING)
	{
		DCONFIG_STRING type_or_name = state->cur_token.str;
		name_line = state->line;
		_dcfg_get_next_token(state);
		/* type name; */
		if(state->cur_token.type == TOKEN_STRING)
		{
			name = state->cur_token.str;
			_dcfg_get_next_token(state);
			DCONFIG_NODE* child = dcfg_add_node(aggregate, type_or_name, false, name, false, state->cur_token.type == TOKEN_LEFT_BRACE);
			assert(child);
			*lhs_node = child;
			return true;
		}
		/* name; */
		else if(state->cur_token.type == TOKEN_SEMICOLON)
		{
			DCONFIG_NODE* child = dcfg_add_node(aggregate, dcfg_from_c_str(""), false, type_or_name, false, state->cur_token.type == TOKEN_LEFT_BRACE);
			assert(child);
			*lhs_node = child;
			return child;
		}
		/* name = or name: */
		else
		{
			name = type_or_name;
		}
	}
	else if(state->cur_token.type == TOKEN_COLON)
	{
		aggregate = config->root;
		_dcfg_get_next_token(state);
		if(state->cur_token.type == TOKEN_STRING)
		{
			name = state->cur_token.str;
			name_line = state->line;
			_dcfg_get_next_token(state);
		}
		else
		{
			_dcfg_expected_after_error(state, state->line, dcfg_from_c_str("a string"), dcfg_from_c_str(":"), state->cur_token.str);
			return false;
		}
	}
	else
	{
		return true;
	}
	
	DCONFIG_NODE* ret;
	while(true)
	{
		printf("Getting... %.*s\n", (int)dcfg_string_length(name), name.start);
		ret = dcfg_get_node(aggregate, name);
		if(!ret)
		{
			_dcfg_print_error_prefix(state->filename, name_line, state->vtable);
			state->vtable->stderr(dcfg_from_c_str("Error: '"));
			DCONFIG_STRING full_name = dcfg_get_full_name(aggregate);
			state->vtable->stderr(full_name);
			state->vtable->realloc((void*)full_name.start, 0);
			state->vtable->stderr(dcfg_from_c_str(":"));
			state->vtable->stderr(name);
			state->vtable->stderr(dcfg_from_c_str("' does not exist.\n"));
			return false;
		}
		
		if(state->cur_token.type == TOKEN_COLON)
		{
			if(!ret->is_aggregate)
			{
				_dcfg_print_error_prefix(state->filename, name_line, state->vtable);
				state->vtable->stderr(dcfg_from_c_str("Error: '"));
				DCONFIG_STRING full_name = dcfg_get_full_name(ret);
				state->vtable->stderr(full_name);
				state->vtable->realloc((void*)full_name.start, 0);
				state->vtable->stderr(dcfg_from_c_str("' is not an aggregate.\n"));
				return false;
			}
			aggregate = ret;
			if(_dcfg_get_next_token(state).type != TOKEN_STRING)
			{
				_dcfg_expected_after_error(state, state->line, dcfg_from_c_str("a string"), dcfg_from_c_str(":"), state->cur_token.str);
				return false;
			}
			name = state->cur_token.str;
		}
		else
		{
			*lhs_node = ret;
			return true;
		}
	}
	assert(0);
	return false;
}

bool parse_assign_expression(DCONFIG* config, DCONFIG_NODE* aggregate, TOKENIZER_STATE* state)
{
	DCONFIG_NODE* lhs = 0;
	if(!parse_left_hand_side(config, aggregate, &lhs, state))
		return false;
	if(lhs)
	{
		if(state->cur_token.type == TOKEN_SEMICOLON)
		{
			_dcfg_get_next_token(state);
			return true;
		}
		else
		{
			_dcfg_expected_error(state, state->line, dcfg_from_c_str(";"), state->cur_token.str);
			return false;
		}
	}
	return true;
}

bool _dcfg_parse_aggregate(DCONFIG* config, DCONFIG_NODE* aggregate, TOKENIZER_STATE* state)
{
	TOKEN tok = state->cur_token;
	size_t start_line = state->line;
	TOKEN_TYPE end_token = tok.type == TOKEN_LEFT_BRACE ? TOKEN_RIGHT_BRACE : TOKEN_EOF;
	if(end_token == TOKEN_RIGHT_BRACE)
		_dcfg_get_next_token(state);
	do
	{
		printf("Here: %zu:%.*s\n", state->line, (int)dcfg_string_length(state->cur_token.str), state->cur_token.str.start);
		if(!parse_assign_expression(config, aggregate, state))
			return false;

		tok = state->cur_token;
		if(tok.type != end_token && tok.type == TOKEN_RIGHT_BRACE)
		{
			if(tok.type == TOKEN_RIGHT_BRACE)
			{
				_dcfg_print_error_prefix(state->filename, state->line, state->vtable);
				state->vtable->stderr(dcfg_from_c_str("Error: Unpaired '}'.\n"));
				return false;
			}
			else if(tok.type == TOKEN_EOF)
			{
				_dcfg_print_error_prefix(state->filename, start_line, state->vtable);
				state->vtable->stderr(dcfg_from_c_str("Error: Unpaired '{'.\n"));
				return false;
			}
		}
	} while(tok.type != end_token);
	
	return true;
}
