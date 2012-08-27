#include "dconfig/internal/parser.h"
#include "dconfig/internal/tokenizer.h"
#include "dconfig/internal/utils.h"
#include "dconfig/dconfig.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

typedef struct
{
	DCONFIG_STRING comment;
	DCONFIG_NODE* last_node;
	size_t last_node_line;
	
	TOKENIZER_STATE* state;
	
	DCONFIG_STRING filename;
	size_t line;
	TOKEN cur_token;
	DCONFIG_VTABLE* vtable;
} PARSER_STATE;

static
bool advance(PARSER_STATE* state)
{
	TOKEN token = _dcfg_get_next_token(state->state);
	while(token.type == TOKEN_COMMENT)
	{
		DCONFIG_STRING* str_ptr;
		if(state->last_node && state->state->line == state->last_node_line)
		{
			str_ptr = &state->last_node->comment;
			state->last_node->own_comment = true;
		}
		else
		{
			state->last_node = 0;
			str_ptr = &state->comment;
		}
		
		if(dcfg_string_length(*str_ptr) > 0)
			dcfg_append_to_string(str_ptr, dcfg_from_c_str("\n"), state->vtable->realloc);
		dcfg_append_to_string(str_ptr, token.str, state->vtable->realloc);
		
		token = _dcfg_get_next_token(state->state);
	}

	state->cur_token = token;
	state->line = state->state->line;

	if(token.type == TOKEN_ERROR)
		return false;
	else
		return true;
}

static
void set_new_node(PARSER_STATE* state, DCONFIG_NODE* node, size_t line)
{
	if(dcfg_string_length(state->comment))
	{
		DCONFIG_STRING* str_ptr = &node->comment;
		node->own_comment = true;
		if(dcfg_string_length(*str_ptr) > 0)
			dcfg_append_to_string(str_ptr, dcfg_from_c_str("\n"), state->vtable->realloc);
		
		dcfg_append_to_string(str_ptr, state->comment, state->vtable->realloc);
		state->comment.end = state->comment.start;
	}
	
	state->last_node = node;
	state->last_node_line = line;
}

static
bool parse_left_hand_side(DCONFIG* config, DCONFIG_NODE* aggregate, DCONFIG_NODE** lhs_node, bool* expect_assign, PARSER_STATE* state)
{
	size_t name_line = state->line;
	*expect_assign = false;
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
		if(!advance(state))
			return false;
		/* type name; */
		if(state->cur_token.type == TOKEN_STRING)
		{
			name = state->cur_token.str;
			if(!advance(state))
				return false;
			DCONFIG_NODE* child = dcfg_add_node(aggregate, type_or_name, false, name, false, state->cur_token.type == TOKEN_LEFT_BRACE);
			if(!child)
			{
				child = dcfg_get_node(aggregate, name);
				_dcfg_print_error_prefix(state->filename, name_line, state->vtable);
				state->vtable->stderr(dcfg_from_c_str("Error: Cannot change the type of '"));
				DCONFIG_STRING full_name = dcfg_get_full_name(child);
				state->vtable->stderr(full_name);
				state->vtable->realloc((void*)full_name.start, 0);
				state->vtable->stderr(dcfg_from_c_str("' from '"));
				state->vtable->stderr(child->type);
				state->vtable->stderr(dcfg_from_c_str("' to '"));
				state->vtable->stderr(type_or_name);
				state->vtable->stderr(dcfg_from_c_str("'.\n"));
				return false;
			}
			*lhs_node = child;
			return true;
		}
		/* name: */
		else if(state->cur_token.type == TOKEN_COLON)
		{
			name = type_or_name;
		}
		/* name = */
		else if(state->cur_token.type == TOKEN_ASSIGN)
		{
			DCONFIG_NODE* child = dcfg_get_node(aggregate, type_or_name);
			if(child)
			{
				*lhs_node = child;
			}
			else
			{
				child = dcfg_add_node(aggregate, dcfg_from_c_str(""), false, type_or_name, false, state->cur_token.type == TOKEN_LEFT_BRACE);
				assert(child);
				*lhs_node = child;
			}
			*expect_assign = true;
			return true;
		}
		/* name;*/
		else
		{
			DCONFIG_NODE* child = dcfg_get_node(aggregate, type_or_name);
			if(child)
			{
				*lhs_node = child;
				*expect_assign = true; /* This will cause an error upon return */
			}
			else
			{
				child = dcfg_add_node(aggregate, dcfg_from_c_str(""), false, type_or_name, false, state->cur_token.type == TOKEN_LEFT_BRACE);
				assert(child);
				*lhs_node = child;
			}
			return true;
		}
	}
	else if(state->cur_token.type == TOKEN_COLON)
	{
		aggregate = config->root;
		if(!advance(state))
			return false;
		if(state->cur_token.type == TOKEN_STRING)
		{
			name = state->cur_token.str;
			name_line = state->line;
			if(!advance(state))
				return false;
		}
		else
		{
			_dcfg_expected_after_error(state->state, state->line, dcfg_from_c_str("a string"), dcfg_from_c_str(":"), state->cur_token.str);
			return false;
		}
	}
	else
	{
		return true;
	}
	
	*expect_assign = true;
	
	DCONFIG_NODE* ret;
	while(true)
	{
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
				state->vtable->stderr(dcfg_from_c_str("' of type '"));
				state->vtable->stderr(ret->type);
				state->vtable->stderr(dcfg_from_c_str("' is not an aggregate.\n"));
				return false;
			}
			aggregate = ret;
			if(!advance(state))
				return false;
			if(state->cur_token.type != TOKEN_STRING)
			{
				_dcfg_expected_after_error(state->state, state->line, dcfg_from_c_str("a string"), dcfg_from_c_str(":"), state->cur_token.str);
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

static
bool parse_assign_expression(DCONFIG* config, DCONFIG_NODE* aggregate, PARSER_STATE* state)
{
	DCONFIG_NODE* lhs = 0;
	bool expect_assign = false;
	if(!parse_left_hand_side(config, aggregate, &lhs, &expect_assign, state))
		return false;
	if(lhs)
	{
		if(expect_assign)
		{
			if(state->cur_token.type == TOKEN_ASSIGN)
			{
				if(!advance(state))
					return false;
			}
			else
			{
				_dcfg_expected_error(state->state, state->line, dcfg_from_c_str("="), state->cur_token.str);
				return false;
			}
		}

		if(state->cur_token.type == TOKEN_SEMICOLON)
		{
			/* Right before we get to the semi-colon */
			set_new_node(state, lhs, state->line);
			if(!advance(state))
				return false;
			return true;
		}
		else
		{
			_dcfg_expected_error(state->state, state->line, dcfg_from_c_str(";"), state->cur_token.str);
			return false;
		}
	}
	return true;
}

static
bool parse_aggregate(DCONFIG* config, DCONFIG_NODE* aggregate, PARSER_STATE* state)
{
	TOKEN tok = state->cur_token;
	size_t start_line = state->line;
	TOKEN_TYPE end_token = tok.type == TOKEN_LEFT_BRACE ? TOKEN_RIGHT_BRACE : TOKEN_EOF;
	if(end_token == TOKEN_RIGHT_BRACE)
	{
		if(!advance(state))
			return false;
	}
	do
	{
		if(!parse_assign_expression(config, aggregate, state))
			return false;

		if(state->cur_token.type == TOKEN_SEMICOLON)
		{
			if(!advance(state))
				return false;
		}
		
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
	
	PARSER_STATE parser_state;
	memset(&parser_state, 0, sizeof(PARSER_STATE));
	parser_state.state = &state;
	parser_state.line = 1;
	parser_state.filename = filename;
	parser_state.vtable = &config->vtable;
	
	if(advance(&parser_state))
		return parse_aggregate(config, root, &parser_state);
	else
		return false;
}
