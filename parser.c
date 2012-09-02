#include "slconfig/internal/parser.h"
#include "slconfig/internal/tokenizer.h"
#include "slconfig/internal/utils.h"
#include "slconfig/slconfig.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

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
} PARSER_STATE;

static bool parse_aggregate(SLCONFIG* config, SLCONFIG_NODE* aggregate, PARSER_STATE* state);

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
				state->last_node = 0;
				str_ptr = &state->comment;
			}
			
			if(slc_string_length(*str_ptr) > 0)
				slc_append_to_string(str_ptr, slc_from_c_str("\n"), state->vtable->realloc);
			token.str.start++;
			slc_append_to_string(str_ptr, token.str, state->vtable->realloc);
		}
		
		token = _slc_get_next_token(state->state);
	}

	state->cur_token = token;
	state->line = state->state->line;

	if(token.type == TOKEN_ERROR)
		return false;
	else
		return true;
}

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

static
bool parse_node_ref_name(SLCONFIG* config, SLCONFIG_NODE* aggregate, SLCONFIG_STRING name, size_t name_line, SLCONFIG_NODE** ref_node, PARSER_STATE* state)
{
	(void)config;
	SLCONFIG_NODE* ret = 0;
	while(true)
	{
		if(ret == 0)
			ret = _slc_search_node(aggregate, name);
		else
			ret = slc_get_node(aggregate, name);
		if(!ret)
		{
			_slc_print_error_prefix(state->filename, name_line, state->vtable);
			state->vtable->stderr(slc_from_c_str("Error: '"));
			SLCONFIG_STRING full_name = slc_get_full_name(aggregate);
			state->vtable->stderr(full_name);
			_slc_free(config, (void*)full_name.start);
			state->vtable->stderr(slc_from_c_str(":"));
			state->vtable->stderr(name);
			state->vtable->stderr(slc_from_c_str("' does not exist.\n"));
			return false;
		}
		
		if(state->cur_token.type == TOKEN_COLON)
		{
			if(!ret->is_aggregate)
			{
				_slc_print_error_prefix(state->filename, name_line, state->vtable);
				state->vtable->stderr(slc_from_c_str("Error: '"));
				SLCONFIG_STRING full_name = slc_get_full_name(ret);
				state->vtable->stderr(full_name);
				_slc_free(config, (void*)full_name.start);
				state->vtable->stderr(slc_from_c_str("' of type '"));
				state->vtable->stderr(ret->type);
				state->vtable->stderr(slc_from_c_str("' is not an aggregate.\n"));
				return false;
			}
			aggregate = ret;
			if(!advance(state))
				return false;
			if(state->cur_token.type != TOKEN_STRING)
			{
				_slc_expected_after_error(state->state, state->line, slc_from_c_str("a string"), slc_from_c_str(":"), state->cur_token.str);
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
bool parse_node_ref(SLCONFIG* config, SLCONFIG_NODE* aggregate, SLCONFIG_NODE** ref_node, PARSER_STATE* state)
{
	SLCONFIG_STRING name;
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
			_slc_expected_after_error(state->state, state->line, slc_from_c_str("a string"), slc_from_c_str(":"), state->cur_token.str);
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
bool parse_right_hand_side(SLCONFIG* config, SLCONFIG_NODE* aggregate, SLCONFIG_STRING* rhs, PARSER_STATE* state)
{	
	SLCONFIG_STRING str;
		
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
			_slc_expected_after_error(state->state, state->line, slc_from_c_str("a string or ':'"), slc_from_c_str("$"), state->cur_token.str);
			return false;
		}
		
		SLCONFIG_NODE* ref_node;
		if(!parse_node_ref(config, aggregate, &ref_node, state))
			return false;
		
		if(ref_node->is_aggregate)
		{
			_slc_print_error_prefix(state->filename, state->line, state->vtable);
			state->vtable->stderr(slc_from_c_str("Error: Trying to extract a string from '"));
			SLCONFIG_STRING full_name = slc_get_full_name(ref_node);
			state->vtable->stderr(full_name);
			_slc_free(config, (void*)full_name.start);
			state->vtable->stderr(slc_from_c_str("' of type '"));
			state->vtable->stderr(ref_node->type);
			state->vtable->stderr(slc_from_c_str("' which is an aggregate.\n"));
		}
		
		str = ref_node->value;
	}
	else
	{
		_slc_expected_after_error(state->state, state->line, slc_from_c_str("a string"), slc_from_c_str("="), state->cur_token.str);
		return false;
	}

	slc_append_to_string(rhs, str, config->vtable.realloc);
	
	return true;
}

static
bool parse_left_hand_side(SLCONFIG* config, SLCONFIG_NODE* aggregate, SLCONFIG_NODE** lhs_node, bool* expect_assign, PARSER_STATE* state)
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
		name_line = state->line;
		if(!advance(state))
			return false;
		/* type name; */
		if(state->cur_token.type == TOKEN_STRING)
		{
			name = state->cur_token.str;
			if(!advance(state))
				return false;
			bool is_aggregate = state->cur_token.type == TOKEN_LEFT_BRACE;
			SLCONFIG_NODE* child = _slc_add_node_no_attach(aggregate, type_or_name, false, name, false, is_aggregate);
			if(!child)
			{
				child = slc_get_node(aggregate, name);
				_slc_print_error_prefix(state->filename, name_line, state->vtable);
				state->vtable->stderr(slc_from_c_str("Error: Cannot change the type of '"));
				SLCONFIG_STRING full_name = slc_get_full_name(child);
				state->vtable->stderr(full_name);
				_slc_free(config, (void*)full_name.start);
				state->vtable->stderr(slc_from_c_str("' from '"));
				state->vtable->stderr(child->type);
				state->vtable->stderr(slc_from_c_str("' ("));
				state->vtable->stderr(slc_from_c_str(child->is_aggregate ? "aggregate" : "string"));
				state->vtable->stderr(slc_from_c_str(") to "));
				state->vtable->stderr(slc_from_c_str("'"));
				state->vtable->stderr(type_or_name);
				state->vtable->stderr(slc_from_c_str("' ("));
				state->vtable->stderr(slc_from_c_str(is_aggregate ? "aggregate" : "string"));
				state->vtable->stderr(slc_from_c_str(").\n"));
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
			SLCONFIG_NODE* child = slc_get_node(aggregate, type_or_name);
			if(child)
			{
				*lhs_node = child;
			}
			else
			{
				child = _slc_add_node_no_attach(aggregate, slc_from_c_str(""), false, type_or_name, false, state->cur_token.type == TOKEN_LEFT_BRACE);
				assert(child);
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
			_slc_expected_after_error(state->state, state->line, slc_from_c_str("a string"), slc_from_c_str(":"), state->cur_token.str);
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
bool parse_assign_expression(SLCONFIG* config, SLCONFIG_NODE* aggregate, PARSER_STATE* state)
{
	SLCONFIG_NODE* lhs = 0;
	bool is_new = false;
	bool expect_assign = false;
	if(!parse_left_hand_side(config, aggregate, &lhs, &expect_assign, state))
		goto error;
	if(lhs)
	{
		bool was_aggregate = false;
		is_new = lhs->parent == 0;
		if(expect_assign)
		{
			if(state->cur_token.type == TOKEN_ASSIGN)
			{
				if(lhs->is_aggregate)
				{
					_slc_print_error_prefix(state->filename, state->line, state->vtable);
					state->vtable->stderr(slc_from_c_str("Error: Trying to assign a string to '"));
					SLCONFIG_STRING full_name = slc_get_full_name(lhs);
					state->vtable->stderr(full_name);
					_slc_free(config, (void*)full_name.start);
					state->vtable->stderr(slc_from_c_str("' of type '"));
					state->vtable->stderr(lhs->type);
					state->vtable->stderr(slc_from_c_str("' which is an aggregate.\n"));
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
					_slc_free(config, (void*)lhs->value.start);
				lhs->value = rhs;
				lhs->own_value = true;
			}
			else if(state->cur_token.type == TOKEN_LEFT_BRACE)
			{
				if(!lhs->is_aggregate)
				{
					_slc_print_error_prefix(state->filename, state->line, state->vtable);
					state->vtable->stderr(slc_from_c_str("Error: Trying to assign an aggregate to '"));
					SLCONFIG_STRING full_name = slc_get_full_name(lhs);
					state->vtable->stderr(full_name);
					_slc_free(config, (void*)full_name.start);
					state->vtable->stderr(slc_from_c_str("' of type '"));
					state->vtable->stderr(lhs->type);
					state->vtable->stderr(slc_from_c_str("' which is not an aggregate.\n"));
					goto error;
				}
				
				set_new_node(state, lhs, state->line);
				was_aggregate = true;
				
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
				
				if(!parse_aggregate(config, &temp_node, state))
					goto error;
				
				for(size_t ii = 0; ii < lhs->num_children; ii++)
					_slc_destroy_node(lhs->children[ii], false);
				
				memcpy(lhs, &temp_node, sizeof(SLCONFIG_NODE));
				if(is_new)
					lhs->parent = 0; /* So the attach code below works */
				for(size_t ii = 0; ii < lhs->num_children; ii++)
					lhs->children[ii]->parent = lhs;
			}
			else
			{
				_slc_expected_error(state->state, state->line, slc_from_c_str(lhs->is_aggregate ? "{" : "="), state->cur_token.str);
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
				_slc_expected_error(state->state, state->line, slc_from_c_str(";"), state->cur_token.str);
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

static
bool parse_remove(SLCONFIG* config, SLCONFIG_NODE* aggregate, PARSER_STATE* state)
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
			_slc_expected_error(state->state, state->line, slc_from_c_str(";"), state->cur_token.str);
			return false;
		}
	}
	return true;
}

static
bool parse_expand_aggregate(SLCONFIG* config, SLCONFIG_NODE* aggregate, PARSER_STATE* state)
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
			_slc_print_error_prefix(state->filename, state->line, state->vtable);
			state->vtable->stderr(slc_from_c_str("Error: Trying to expand '"));
			SLCONFIG_STRING full_name = slc_get_full_name(ref_node);
			state->vtable->stderr(full_name);
			_slc_free(config, (void*)full_name.start);
			state->vtable->stderr(slc_from_c_str("' of type '"));
			state->vtable->stderr(ref_node->type);
			state->vtable->stderr(slc_from_c_str("' which is not an aggregate.\n"));
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
				
				_slc_print_error_prefix(state->filename, state->line, state->vtable);
				state->vtable->stderr(slc_from_c_str("Error: Cannot expand '"));
				
				full_name = slc_get_full_name(ref_node);
				state->vtable->stderr(full_name);
				_slc_free(config, (void*)full_name.start);
				
				state->vtable->stderr(slc_from_c_str("' of type '"));
				state->vtable->stderr(ref_node->type);
				state->vtable->stderr(slc_from_c_str("'. Its child '"));
				
				full_name = slc_get_full_name(child);
				state->vtable->stderr(full_name);
				_slc_free(config, (void*)full_name.start);
				
				state->vtable->stderr(slc_from_c_str("' of type '"));
				state->vtable->stderr(child->type);
				state->vtable->stderr(slc_from_c_str("' ("));
				state->vtable->stderr(slc_from_c_str(child->is_aggregate ? "aggregate" : "string"));
				state->vtable->stderr(slc_from_c_str(") conflicts with '"));
				
				full_name = slc_get_full_name(old_node);
				state->vtable->stderr(full_name);
				_slc_free(config, (void*)full_name.start);
				
				state->vtable->stderr(slc_from_c_str("' of type '"));
				state->vtable->stderr(old_node->type);
				state->vtable->stderr(slc_from_c_str("' ("));
				state->vtable->stderr(slc_from_c_str(old_node->is_aggregate ? "aggregate" : "string"));
				state->vtable->stderr(slc_from_c_str(").\n"));
				
				return false;
			}
			
			for(size_t ii = 0; ii < new_node->num_children; ii++)
				_slc_destroy_node(new_node->children[ii], false);
			
			_slc_free(config, new_node->children);
			
			if(new_node->own_value)
				_slc_free(config, (void*)new_node->value.start);
			
			_slc_copy_into(new_node, child);
		}
		
		if(state->cur_token.type != TOKEN_SEMICOLON)
		{
			_slc_expected_error(state->state, state->line, slc_from_c_str(";"), state->cur_token.str);
			return false;
		}
	}
	return true;
}

static
bool parse_aggregate(SLCONFIG* config, SLCONFIG_NODE* aggregate, PARSER_STATE* state)
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
				_slc_print_error_prefix(state->filename, state->line, state->vtable);
				state->vtable->stderr(slc_from_c_str("Error: Unpaired '}'.\n"));
				return false;
			}
			else if(tok.type == TOKEN_EOF)
			{
				_slc_print_error_prefix(state->filename, start_line, state->vtable);
				state->vtable->stderr(slc_from_c_str("Error: Unpaired '{'.\n"));
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
bool _slc_parse_file(SLCONFIG* config, SLCONFIG_NODE* root, SLCONFIG_STRING filename, SLCONFIG_STRING file)
{
	TOKENIZER_STATE state;
	state.filename = filename;
	state.str = file;
	state.line = 1;
	state.vtable = &config->vtable;
	
	config->files = config->vtable.realloc(config->files, (config->num_files + 1) * sizeof(SLCONFIG_STRING));
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
