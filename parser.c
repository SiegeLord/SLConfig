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

static bool parse_aggregate(DCONFIG* config, DCONFIG_NODE* aggregate, PARSER_STATE* state);

static
bool advance(PARSER_STATE* state)
{
	TOKEN token = _dcfg_get_next_token(state->state);
	while(token.type == TOKEN_COMMENT)
	{
		if(token.str.start[0] == '*')
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
			token.str.start++;
			dcfg_append_to_string(str_ptr, token.str, state->vtable->realloc);
		}
		
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
bool parse_node_ref_name(DCONFIG* config, DCONFIG_NODE* aggregate, DCONFIG_STRING name, size_t name_line, DCONFIG_NODE** ref_node, PARSER_STATE* state)
{
	(void)config;
	DCONFIG_NODE* ret = 0;
	while(true)
	{
		if(ret == 0)
			ret = _dcfg_search_node(aggregate, name);
		else
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
			name_line = state->line;
			if(!advance(state))
				return false;
		}
		else
		{
			*ref_node = ret;
			return true;
		}
	}
}

static
bool parse_node_ref(DCONFIG* config, DCONFIG_NODE* aggregate, DCONFIG_NODE** ref_node, PARSER_STATE* state)
{
	DCONFIG_STRING name;
	size_t name_line;
	if(state->cur_token.type == TOKEN_COLON)
	{
		aggregate = config->root;
		if(!advance(state))
			return false;
		if(state->cur_token.type == TOKEN_STRING)
		{
			name = state->cur_token.str;
			name_line = state->line;
		}
		else
		{
			_dcfg_expected_after_error(state->state, state->line, dcfg_from_c_str("a string"), dcfg_from_c_str(":"), state->cur_token.str);
			return false;
		}
	}
	else if(state->cur_token.type == TOKEN_STRING)
	{
		name = state->cur_token.str;
		name_line = state->line;
	}
	else
	{
		return true;
	}
	
	if(!advance(state))
		return false;
	
	return parse_node_ref_name(config, aggregate, name, name_line, ref_node, state);
}

static
bool parse_right_hand_side(DCONFIG* config, DCONFIG_NODE* aggregate, DCONFIG_STRING* rhs, PARSER_STATE* state)
{	
	DCONFIG_STRING str;
		
	if(state->cur_token.type == TOKEN_STRING)
	{	
		str = state->cur_token.str;
		if(!advance(state))
			return false;
	}
	else if(state->cur_token.type == TOKEN_DOLLAR)
	{
		if(!advance(state))
			return false;
		
		if(state->cur_token.type != TOKEN_STRING && state->cur_token.type != TOKEN_COLON)
		{
			_dcfg_expected_after_error(state->state, state->line, dcfg_from_c_str("a string or ':'"), dcfg_from_c_str("$"), state->cur_token.str);
			return false;
		}
		
		DCONFIG_NODE* ref_node;
		if(!parse_node_ref(config, aggregate, &ref_node, state))
			return false;
		
		if(ref_node->is_aggregate)
		{
			_dcfg_print_error_prefix(state->filename, state->line, state->vtable);
			state->vtable->stderr(dcfg_from_c_str("Error: Trying to extract a string from '"));
			DCONFIG_STRING full_name = dcfg_get_full_name(ref_node);
			state->vtable->stderr(full_name);
			state->vtable->realloc((void*)full_name.start, 0);
			state->vtable->stderr(dcfg_from_c_str("' of type '"));
			state->vtable->stderr(ref_node->type);
			state->vtable->stderr(dcfg_from_c_str("' which is an aggregate.\n"));
		}
		
		str = ref_node->value;
	}
	else
	{
		_dcfg_expected_after_error(state->state, state->line, dcfg_from_c_str("a string"), dcfg_from_c_str("="), state->cur_token.str);
		return false;
	}

	dcfg_append_to_string(rhs, str, config->vtable.realloc);
	
	return true;
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
			*expect_assign = state->cur_token.type != TOKEN_SEMICOLON;
			return true;
		}
		/* name: */
		else if(state->cur_token.type == TOKEN_COLON)
		{
			name = type_or_name;
			name_line = state->line;
		}
		/* name = */
		else if(state->cur_token.type == TOKEN_ASSIGN || state->cur_token.type == TOKEN_LEFT_BRACE)
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
	
	return parse_node_ref_name(config, aggregate, name, name_line, lhs_node, state);
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
		bool was_aggregate = false;
		if(expect_assign)
		{
			if(state->cur_token.type == TOKEN_ASSIGN)
			{
				if(lhs->is_aggregate)
				{
					_dcfg_print_error_prefix(state->filename, state->line, state->vtable);
					state->vtable->stderr(dcfg_from_c_str("Error: Trying to assign a string to '"));
					DCONFIG_STRING full_name = dcfg_get_full_name(lhs);
					state->vtable->stderr(full_name);
					state->vtable->realloc((void*)full_name.start, 0);
					state->vtable->stderr(dcfg_from_c_str("' of type '"));
					state->vtable->stderr(lhs->type);
					state->vtable->stderr(dcfg_from_c_str("' which is an aggregate.\n"));
					return false;
				}
				
				if(!advance(state))
					return false;
				
				DCONFIG_STRING rhs = {0, 0};
				do
				{
					if(!parse_right_hand_side(config, aggregate, &rhs, state))
						return false;
				} while(state->cur_token.type != TOKEN_SEMICOLON);
				
				if(lhs->own_value)
					config->vtable.realloc((void*)lhs->value.start, 0);
				lhs->value = rhs;
				lhs->own_value = true;
			}
			else if(state->cur_token.type == TOKEN_LEFT_BRACE)
			{
				if(!lhs->is_aggregate)
				{
					_dcfg_print_error_prefix(state->filename, state->line, state->vtable);
					state->vtable->stderr(dcfg_from_c_str("Error: Trying to an aggregate to '"));
					DCONFIG_STRING full_name = dcfg_get_full_name(lhs);
					state->vtable->stderr(full_name);
					state->vtable->realloc((void*)full_name.start, 0);
					state->vtable->stderr(dcfg_from_c_str("' of type '"));
					state->vtable->stderr(lhs->type);
					state->vtable->stderr(dcfg_from_c_str("' which is not an aggregate.\n"));
					return false;
				}
				
				set_new_node(state, lhs, state->line);
				was_aggregate = true;
				
				DCONFIG_NODE temp_node;
				memset(&temp_node, 0, sizeof(DCONFIG_NODE));
				temp_node.parent = aggregate;
				temp_node.is_aggregate = true;
				temp_node.config = config;
				temp_node.type = lhs->type;
				temp_node.own_type = lhs->own_type;
				temp_node.name = lhs->name;
				temp_node.own_name = lhs->own_name;
				temp_node.comment = lhs->comment;
				temp_node.own_comment = lhs->own_comment;
				
				if(!parse_aggregate(config, &temp_node, state))
					return false;
				_dcfg_clear_node(lhs);
				memcpy(lhs, &temp_node, sizeof(DCONFIG_NODE));
			}
			else
			{
				_dcfg_expected_error(state->state, state->line, dcfg_from_c_str(lhs->is_aggregate ? "{" : "="), state->cur_token.str);
				return false;
			}
		}

		if(!was_aggregate)
		{
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
	
	if(end_token == TOKEN_RIGHT_BRACE)
	{
		if(!advance(state))
			return false;
	}
	
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
