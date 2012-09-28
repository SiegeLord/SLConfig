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

#include "slconfig/internal/parser.h"
#include "slconfig/internal/tokenizer.h"
#include "slconfig/internal/utils.h"
#include "slconfig/internal/slconfig.h"
#include "slconfig/slconfig.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

/* A wrapper around TOKENIZER_STATE to additionally hold machinery that chomps up the docstrings */
typedef struct
{
	SLCONFIG_STRING comment;
	SLCONFIG_NODE* last_node;
	size_t last_node_line;
	
	TOKENIZER_STATE* state;
	
	SLCONFIG_STRING filename;
	size_t line;
	TOKEN cur_token;
	SLCONFIG_VTABLE* vtable;
	bool free_token;
} PARSER_STATE;

static bool parse_aggregate(CONFIG* config, SLCONFIG_NODE* aggregate, PARSER_STATE* state);

/* Wrapper around _slc_get_next_token to chomp up the docstrings and ignore comments. */
static
bool advance(PARSER_STATE* state)
{
	TOKEN token = _slc_get_next_token(state->state);
	while(token.type == TOKEN_COMMENT)
	{
		if(token.str.start[0] == '*')
		{
			SLCONFIG_STRING* str_ptr;
			if(state->last_node && state->state->line == state->last_node_line)
			{
				str_ptr = &state->last_node->comment;
				state->last_node->own_comment = true;
			}
			else
			{
				state->last_node = NULL;
				str_ptr = &state->comment;
			}
			
			if(slc_string_length(*str_ptr) > 0)
				slc_append_to_string(str_ptr, slc_from_c_str("\n"), state->vtable->realloc);
			token.str.start++;
			slc_append_to_string(str_ptr, token.str, state->vtable->realloc);
		}
		
		token = _slc_get_next_token(state->state);
	}
	
	if(state->free_token)
		slc_destroy_string(&state->cur_token.str, state->vtable->realloc);

	state->cur_token = token;
	state->line = state->state->line;
	state->free_token = token.own;

	if(token.type == TOKEN_ERROR)
		return false;
	else
		return true;
}

/*
 * Used in conjunction with advance to specify which node receives comments. Comments before a node and before a semicolon or opening brace get
 * attached to that node.
 */
static
void set_new_node(PARSER_STATE* state, SLCONFIG_NODE* node, size_t line)
{
	if(slc_string_length(state->comment))
	{
		SLCONFIG_STRING* str_ptr = &node->comment;
		node->own_comment = true;
		if(slc_string_length(*str_ptr) > 0)
			slc_append_to_string(str_ptr, slc_from_c_str("\n"), state->vtable->realloc);
		
		slc_append_to_string(str_ptr, state->comment, state->vtable->realloc);
		state->comment.end = state->comment.start;
	}
	
	state->last_node = node;
	state->last_node_line = line;
}

/*
 * Parse a node reference statement given the first name in the refence statement
 */
static
bool parse_node_ref_name(CONFIG* config, SLCONFIG_NODE* aggregate, SLCONFIG_STRING name, size_t name_line, SLCONFIG_NODE** ref_node, PARSER_STATE* state)
{
	(void)config;
	SLCONFIG_NODE* ret = NULL;
	while(true)
	{
		/* The idea here is to prevent going up the hierarchy once we went down one step in it */
		if(!ret)
			ret = _slc_search_node(aggregate, name);
		else
			ret = slc_get_node(aggregate, name);
		if(!ret)
		{
			_slc_print_error_prefix(config, state->filename, name_line, state->vtable);
			state->vtable->error(slc_from_c_str("Error: '"));
			SLCONFIG_STRING full_name = slc_get_full_name(aggregate);
			state->vtable->error(full_name);
			slc_destroy_string(&full_name, config->vtable.realloc);
			state->vtable->error(slc_from_c_str(":"));
			state->vtable->error(name);
			state->vtable->error(slc_from_c_str("' does not exist.\n"));
			return false;
		}
		
		if(state->cur_token.type == TOKEN_COLON)
		{
			if(!ret->is_aggregate)
			{
				_slc_print_error_prefix(config, state->filename, name_line, state->vtable);
				state->vtable->error(slc_from_c_str("Error: '"));
				SLCONFIG_STRING full_name = slc_get_full_name(ret);
				state->vtable->error(full_name);
				slc_destroy_string(&full_name, config->vtable.realloc);
				state->vtable->error(slc_from_c_str("' of type '"));
				state->vtable->error(ret->type);
				state->vtable->error(slc_from_c_str("' is not an aggregate.\n"));
				return false;
			}
			aggregate = ret;
			if(!advance(state))
				return false;
			if(state->cur_token.type != TOKEN_STRING)
			{
				_slc_expected_after_error(config, state->state, state->line, slc_from_c_str("a string"), slc_from_c_str(":"), state->cur_token.str);
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

/*
 * Parse a node reference statement
 */
static
bool parse_node_ref(CONFIG* config, SLCONFIG_NODE* aggregate, SLCONFIG_NODE** ref_node, PARSER_STATE* state)
{
	SLCONFIG_STRING name;
	size_t name_line;
	if(state->cur_token.type == TOKEN_DOUBLE_COLON)
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
			_slc_expected_after_error(config, state->state, state->line, slc_from_c_str("a string"), slc_from_c_str(":"), state->cur_token.str);
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
		assert(0);
		return false;
	}
	
	if(!advance(state))
		return false;
	
	return parse_node_ref_name(config, aggregate, name, name_line, ref_node, state);
}

/* Get the string value of a single expression on the right hand side of a string assign statement and append it to the current rhs string */
static
bool parse_right_hand_side(CONFIG* config, SLCONFIG_NODE* aggregate, SLCONFIG_STRING* rhs, PARSER_STATE* state)
{	
	SLCONFIG_STRING str;
	bool own_str = false;
		
	if(state->cur_token.type == TOKEN_STRING)
	{	
		str = state->cur_token.str;
		own_str = state->cur_token.own;
		state->free_token = false;
		if(!advance(state))
			return false;
	}
	else if(state->cur_token.type == TOKEN_DOLLAR)
	{
		if(!advance(state))
			return false;
		
		if(state->cur_token.type != TOKEN_STRING && state->cur_token.type != TOKEN_DOUBLE_COLON)
		{
			_slc_expected_after_error(config, state->state, state->line, slc_from_c_str("a string or '::'"), slc_from_c_str("$"), state->cur_token.str);
			return false;
		}
		
		SLCONFIG_NODE* ref_node;
		if(!parse_node_ref(config, aggregate, &ref_node, state))
			return false;
		
		if(ref_node->is_aggregate)
		{
			_slc_print_error_prefix(config, state->filename, state->line, state->vtable);
			state->vtable->error(slc_from_c_str("Error: Trying to extract a string from '"));
			SLCONFIG_STRING full_name = slc_get_full_name(ref_node);
			state->vtable->error(full_name);
			slc_destroy_string(&full_name, config->vtable.realloc);
			state->vtable->error(slc_from_c_str("' of type '"));
			state->vtable->error(ref_node->type);
			state->vtable->error(slc_from_c_str("' which is an aggregate.\n"));
		}
		
		str = ref_node->value;
	}
	else
	{
		_slc_expected_after_error(config, state->state, state->line, slc_from_c_str("a string"), slc_from_c_str("="), state->cur_token.str);
		return false;
	}

	slc_append_to_string(rhs, str, config->vtable.realloc);
	
	if(own_str)
		slc_destroy_string(&str, config->vtable.realloc);
	
	return true;
}

/*
 * Parse the left-hand side expression of an assign statement. Tricky bit is determining whether an existing node needs to be found
 * or a new node needs to be created
 */
static
bool parse_left_hand_side(CONFIG* config, SLCONFIG_NODE* aggregate, SLCONFIG_NODE** lhs_node, bool* expect_assign, PARSER_STATE* state)
{
	size_t name_line = state->line;
	*expect_assign = false;
	/*
	name;
	name:name;
	type name;
	*/
	SLCONFIG_STRING name;
	if(state->cur_token.type == TOKEN_STRING)
	{
		SLCONFIG_STRING type_or_name = state->cur_token.str;
		bool own_type_or_name = state->cur_token.own;
		state->free_token = false;
		name_line = state->line;
		if(!advance(state))
			return false;
		/* type name; */
		if(state->cur_token.type == TOKEN_STRING)
		{
			name = state->cur_token.str;
			bool own_name = state->cur_token.own;
			state->free_token = false;
			if(!advance(state))
				return false;
			bool is_aggregate = state->cur_token.type == TOKEN_LEFT_BRACE;
			SLCONFIG_NODE* child = _slc_add_node_no_attach(aggregate, type_or_name, false, name, false, is_aggregate);
			if(!child)
			{
				child = slc_get_node(aggregate, name);
				_slc_print_error_prefix(config, state->filename, name_line, state->vtable);
				state->vtable->error(slc_from_c_str("Error: Cannot change the type of '"));
				SLCONFIG_STRING full_name = slc_get_full_name(child);
				state->vtable->error(full_name);
				slc_destroy_string(&full_name, config->vtable.realloc);
				state->vtable->error(slc_from_c_str("' from '"));
				state->vtable->error(child->type);
				state->vtable->error(slc_from_c_str("' ("));
				state->vtable->error(slc_from_c_str(child->is_aggregate ? "aggregate" : "string"));
				state->vtable->error(slc_from_c_str(") to "));
				state->vtable->error(slc_from_c_str("'"));
				state->vtable->error(type_or_name);
				state->vtable->error(slc_from_c_str("' ("));
				state->vtable->error(slc_from_c_str(is_aggregate ? "aggregate" : "string"));
				state->vtable->error(slc_from_c_str(").\n"));
				return false;
			}
			
			child->own_type = own_type_or_name;
			child->own_name = own_name;
			
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
			SLCONFIG_NODE* child = slc_get_node(aggregate, type_or_name);
			if(child)
			{
				*lhs_node = child;
			}
			else
			{
				child = _slc_add_node_no_attach(aggregate, slc_from_c_str(""), false, type_or_name, false, state->cur_token.type == TOKEN_LEFT_BRACE);
				assert(child);
				child->own_name = own_type_or_name;
				*lhs_node = child;
			}
			*expect_assign = true;
			return true;
		}
		/* name;*/
		else
		{
			SLCONFIG_NODE* child = slc_get_node(aggregate, type_or_name);
			if(child)
			{
				*lhs_node = child;
				*expect_assign = true; /* This will cause an error upon return */
			}
			else
			{
				child = _slc_add_node_no_attach(aggregate, slc_from_c_str(""), false, type_or_name, false, state->cur_token.type == TOKEN_LEFT_BRACE);
				assert(child);
				child->own_name = own_type_or_name;
				*lhs_node = child;
			}
			return true;
		}
	}
	else if(state->cur_token.type == TOKEN_DOUBLE_COLON)
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
			_slc_expected_after_error(config, state->state, state->line, slc_from_c_str("a string"), slc_from_c_str(":"), state->cur_token.str);
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

/* Parse an assign statement */
static
bool parse_assign_expression(CONFIG* config, SLCONFIG_NODE* aggregate, PARSER_STATE* state)
{
	SLCONFIG_NODE* lhs = NULL;
	bool is_new = false;
	bool expect_assign = false;
	if(!parse_left_hand_side(config, aggregate, &lhs, &expect_assign, state))
		goto error;
	if(lhs)
	{
		bool was_aggregate = false;
		is_new = lhs->parent == NULL;
		if(expect_assign)
		{
			if(state->cur_token.type == TOKEN_ASSIGN)
			{
				if(lhs->is_aggregate)
				{
					_slc_print_error_prefix(config, state->filename, state->line, state->vtable);
					state->vtable->error(slc_from_c_str("Error: Trying to assign a string to '"));
					SLCONFIG_STRING full_name = slc_get_full_name(lhs);
					state->vtable->error(full_name);
					slc_destroy_string(&full_name, config->vtable.realloc);
					state->vtable->error(slc_from_c_str("' of type '"));
					state->vtable->error(lhs->type);
					state->vtable->error(slc_from_c_str("' which is an aggregate.\n"));
					goto error;
				}
				
				if(!advance(state))
					goto error;
				
				SLCONFIG_STRING rhs = {0, 0};
				do
				{
					if(!parse_right_hand_side(config, aggregate, &rhs, state))
						goto error;
				} while(state->cur_token.type != TOKEN_SEMICOLON);
				
				if(lhs->own_value)
					slc_destroy_string(&lhs->value, config->vtable.realloc);
				lhs->value = rhs;
				lhs->own_value = true;
			}
			else if(state->cur_token.type == TOKEN_LEFT_BRACE)
			{
				if(!lhs->is_aggregate)
				{
					_slc_print_error_prefix(config, state->filename, state->line, state->vtable);
					state->vtable->error(slc_from_c_str("Error: Trying to assign an aggregate to '"));
					SLCONFIG_STRING full_name = slc_get_full_name(lhs);
					state->vtable->error(full_name);
					slc_destroy_string(&full_name, config->vtable.realloc);
					state->vtable->error(slc_from_c_str("' of type '"));
					state->vtable->error(lhs->type);
					state->vtable->error(slc_from_c_str("' which is not an aggregate.\n"));
					goto error;
				}
				
				set_new_node(state, lhs, state->line);
				was_aggregate = true;
				
				/* A temporary is created so that we can keep referencing the original node before it gets overwritten */
				SLCONFIG_NODE temp_node;
				memset(&temp_node, 0, sizeof(SLCONFIG_NODE));
				temp_node.parent = aggregate;
				temp_node.is_aggregate = true;
				temp_node.config = config;
				temp_node.type = lhs->type;
				temp_node.own_type = lhs->own_type;
				temp_node.name = lhs->name;
				temp_node.own_name = lhs->own_name;
				temp_node.comment = lhs->comment;
				temp_node.own_comment = lhs->own_comment;
				temp_node.user_data = lhs->user_data;
				temp_node.user_destructor = lhs->user_destructor;
				
				if(!parse_aggregate(config, &temp_node, state))
					goto error;
				
				for(size_t ii = 0; ii < lhs->num_children; ii++)
					_slc_destroy_node(lhs->children[ii], false);
				_slc_free(config, lhs->children);
				
				memcpy(lhs, &temp_node, sizeof(SLCONFIG_NODE));
				if(is_new)
					lhs->parent = NULL; /* So the attach code below works */
				for(size_t ii = 0; ii < lhs->num_children; ii++)
					lhs->children[ii]->parent = lhs;
			}
			else
			{
				_slc_expected_error(config, state->state, state->line, slc_from_c_str(lhs->is_aggregate ? "{" : "="), state->cur_token.str);
				goto error;
			}
		}

		if(!was_aggregate)
		{
			if(state->cur_token.type == TOKEN_SEMICOLON)
			{
				/* Right before we get to the semi-colon */
				set_new_node(state, lhs, state->line);
			}
			else
			{
				_slc_expected_error(config, state->state, state->line, slc_from_c_str(";"), state->cur_token.str);
				goto error;
			}
		}
	}
	if(is_new)
		_slc_attach_node(aggregate, lhs);
	return true;
error:
	if(is_new)
		slc_destroy_node(lhs);
	return false;
}

/* Parse the remove statement*/
static
bool parse_remove(CONFIG* config, SLCONFIG_NODE* aggregate, PARSER_STATE* state)
{
	if(state->cur_token.type == TOKEN_TILDE)
	{
		if(!advance(state))
			return false;
		
		SLCONFIG_NODE* ref_node;
		if(!parse_node_ref(config, aggregate, &ref_node, state))
			return false;
		
		slc_destroy_node(ref_node);
		
		if(state->cur_token.type != TOKEN_SEMICOLON)
		{
			_slc_expected_error(config, state->state, state->line, slc_from_c_str(";"), state->cur_token.str);
			return false;
		}
	}
	return true;
}

/* Parse an expand aggregate statement */
static
bool parse_expand_aggregate(CONFIG* config, SLCONFIG_NODE* aggregate, PARSER_STATE* state)
{
	if(state->cur_token.type == TOKEN_DOLLAR)
	{
		if(!advance(state))
			return false;
		
		SLCONFIG_NODE* ref_node;
		if(!parse_node_ref(config, aggregate, &ref_node, state))
			return false;
		
		if(!ref_node->is_aggregate)
		{
			_slc_print_error_prefix(config, state->filename, state->line, state->vtable);
			state->vtable->error(slc_from_c_str("Error: Trying to expand '"));
			SLCONFIG_STRING full_name = slc_get_full_name(ref_node);
			state->vtable->error(full_name);
			slc_destroy_string(&full_name, config->vtable.realloc);
			state->vtable->error(slc_from_c_str("' of type '"));
			state->vtable->error(ref_node->type);
			state->vtable->error(slc_from_c_str("' which is not an aggregate.\n"));
			return false;
		}
		
		for(size_t ii = 0; ii < ref_node->num_children; ii++)
		{
			SLCONFIG_NODE* child = ref_node->children[ii];
			SLCONFIG_NODE* new_node = slc_add_node(aggregate, child->type, false, child->name, false, child->is_aggregate);
			if(!new_node)
			{
				SLCONFIG_NODE* old_node = slc_get_node(aggregate, child->name);
				SLCONFIG_STRING full_name;
				
				_slc_print_error_prefix(config, state->filename, state->line, state->vtable);
				state->vtable->error(slc_from_c_str("Error: Cannot expand '"));
				
				full_name = slc_get_full_name(ref_node);
				state->vtable->error(full_name);
				slc_destroy_string(&full_name, config->vtable.realloc);
				
				state->vtable->error(slc_from_c_str("' of type '"));
				state->vtable->error(ref_node->type);
				state->vtable->error(slc_from_c_str("'. Its child '"));
				
				full_name = slc_get_full_name(child);
				state->vtable->error(full_name);
				slc_destroy_string(&full_name, config->vtable.realloc);
				
				state->vtable->error(slc_from_c_str("' of type '"));
				state->vtable->error(child->type);
				state->vtable->error(slc_from_c_str("' ("));
				state->vtable->error(slc_from_c_str(child->is_aggregate ? "aggregate" : "string"));
				state->vtable->error(slc_from_c_str(") conflicts with '"));
				
				full_name = slc_get_full_name(old_node);
				state->vtable->error(full_name);
				slc_destroy_string(&full_name, config->vtable.realloc);
				
				state->vtable->error(slc_from_c_str("' of type '"));
				state->vtable->error(old_node->type);
				state->vtable->error(slc_from_c_str("' ("));
				state->vtable->error(slc_from_c_str(old_node->is_aggregate ? "aggregate" : "string"));
				state->vtable->error(slc_from_c_str(").\n"));
				
				return false;
			}
			
			for(size_t ii = 0; ii < new_node->num_children; ii++)
				_slc_destroy_node(new_node->children[ii], false);
			
			_slc_free(config, new_node->children);
			
			if(new_node->own_value)
				slc_destroy_string(&new_node->value, config->vtable.realloc);
			
			_slc_copy_into(new_node, child);
		}
		
		if(state->cur_token.type != TOKEN_SEMICOLON)
		{
			_slc_expected_error(config, state->state, state->line, slc_from_c_str(";"), state->cur_token.str);
			return false;
		}
	}
	return true;
}

/* Parse an include statement */
static
bool parse_include_expression(CONFIG* config, SLCONFIG_NODE* aggregate, PARSER_STATE* state)
{
	if(state->cur_token.type == TOKEN_HASH)
	{
		if(!advance(state))
			return false;
		if(state->cur_token.type != TOKEN_STRING)
		{
			_slc_expected_after_error(config, state->state, state->line, slc_from_c_str("include"), slc_from_c_str("#"), state->cur_token.str);
			return false;
		}
		if(!slc_string_equal(state->cur_token.str, slc_from_c_str("include")))
		{
			_slc_expected_after_error(config, state->state, state->line, slc_from_c_str("include"), slc_from_c_str("#"), state->cur_token.str);
			return false;
		}
		
		if(!advance(state))
			return false;
		
		if(state->cur_token.type != TOKEN_STRING)
		{
			_slc_expected_after_error(config, state->state, state->line, slc_from_c_str("a string"), slc_from_c_str("#"), state->cur_token.str);
			return false;
		}
		
		SLCONFIG_STRING filename = state->cur_token.str;
		
		if(!_slc_add_include(config, filename, false, state->line))
		{
			_slc_print_error_prefix(config, state->filename, state->line, state->vtable);
			state->vtable->error(slc_from_c_str("Error: Circular include.\n"));
			return false;
		}
		
		SLCONFIG_STRING file = {0, 0};
		if(!_slc_load_file(config, filename, &file))
		{
			_slc_print_error_prefix(config, state->filename, state->line, state->vtable);
			state->vtable->error(slc_from_c_str("Error: File '"));
			state->vtable->error(filename);
			state->vtable->error(slc_from_c_str("' does not exist.\n"));
			if(config->num_search_dirs)
			{
				state->vtable->error(slc_from_c_str("Search directories:\n"));
				for(size_t ii = 0; ii < config->num_search_dirs; ii++)
				{
					state->vtable->error(config->search_dirs[ii]);
					state->vtable->error(slc_from_c_str("\n"));
				}
			}
			return false;
		}
		
		if(!_slc_parse_file(config, aggregate, filename, file))
			return false;
		
		_slc_pop_include(config);
		
		if(!advance(state))
			return false;
		
		if(state->cur_token.type != TOKEN_SEMICOLON)
		{
			_slc_expected_error(config, state->state, state->line, slc_from_c_str(";"), state->cur_token.str);
			return false;
		}
	}
	return true;
}

/* Chomp up the statements in the root, or between braces in an aggregate. The braces are taken care of by this function */
static
bool parse_aggregate(CONFIG* config, SLCONFIG_NODE* aggregate, PARSER_STATE* state)
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
		if(!parse_include_expression(config, aggregate, state))
			return false;
		if(!parse_assign_expression(config, aggregate, state))
			return false;
		if(!parse_remove(config, aggregate, state))
			return false;
		if(!parse_expand_aggregate(config, aggregate, state))
			return false;

		tok = state->cur_token;
		if(tok.type == TOKEN_SEMICOLON)
		{
			if(!advance(state))
				return false;
		}
		
		tok = state->cur_token;
		if(tok.type != end_token && tok.type == TOKEN_RIGHT_BRACE)
		{
			if(tok.type == TOKEN_RIGHT_BRACE)
			{
				_slc_print_error_prefix(config, state->filename, state->line, state->vtable);
				state->vtable->error(slc_from_c_str("Error: Unpaired '}'.\n"));
				return false;
			}
			else if(tok.type == TOKEN_EOF)
			{
				_slc_print_error_prefix(config, state->filename, start_line, state->vtable);
				state->vtable->error(slc_from_c_str("Error: Unpaired '{'.\n"));
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

bool _slc_parse_file(CONFIG* config, SLCONFIG_NODE* root, SLCONFIG_STRING filename, SLCONFIG_STRING file)
{	
	TOKENIZER_STATE state;
	state.filename = filename;
	state.line = 1;
	state.vtable = &config->vtable;
	state.str = file;
	state.config = config;
	state.gag_errors = false;
	
	PARSER_STATE parser_state;
	memset(&parser_state, 0, sizeof(PARSER_STATE));
	parser_state.state = &state;
	parser_state.line = 1;
	parser_state.filename = filename;
	parser_state.vtable = &config->vtable;
	parser_state.free_token = false;
	
	bool ret;
	if(advance(&parser_state))
		ret = parse_aggregate(config, root, &parser_state);
	else
		ret = false;
	
	slc_destroy_string(&parser_state.comment, config->vtable.realloc);
	
	return ret;
}

SLCONFIG_NODE* slc_get_node_by_reference(SLCONFIG_NODE* aggregate, SLCONFIG_STRING reference)
{
	assert(aggregate);
	if(!aggregate->is_aggregate)
		return NULL;

	TOKENIZER_STATE state;
	state.filename = slc_from_c_str("");
	state.line = 1;
	state.vtable = &aggregate->config->vtable;
	state.str = reference;
	state.config = aggregate->config;
	state.gag_errors = true;
	
	SLCONFIG_NODE* ret = NULL;
	
	_slc_get_next_token(&state);
	if(state.cur_token.type == TOKEN_DOUBLE_COLON)
	{
		aggregate = aggregate->config->root;
		_slc_get_next_token(&state);
	}
	
	while(state.cur_token.type == TOKEN_STRING)
	{
		if(!ret)
			ret = _slc_search_node(aggregate, state.cur_token.str);
		else
			ret = slc_get_node(aggregate, state.cur_token.str);

		if(state.cur_token.own)
			slc_destroy_string(&state.cur_token.str, aggregate->config->vtable.realloc);
		
		if(!ret)
			return NULL;
		
		_slc_get_next_token(&state);
		if(state.cur_token.type == TOKEN_COLON)
		{
			if(!ret->is_aggregate)
				return NULL;
			
			aggregate = ret;
			_slc_get_next_token(&state);
		}
		else if(state.cur_token.type == TOKEN_EOF)
		{
			return ret;
		}
		else
		{
			break;
		}
	}
	
	return NULL;
}
