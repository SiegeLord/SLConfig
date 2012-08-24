#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "dconfig/dconfig.h"
#include "dconfig/internal/tokenizer.h"

void default_stderr(DCONFIG_STRING s)
{
	fprintf(stderr, "%.*s", (int)dcfg_string_length(s), s.start);
}

DCONFIG_VTABLE default_vtable =
{
	&realloc,
	&default_stderr
};

int main()
{
	FILE* f = fopen("test.cfg", "rb");
	fseek(f, 0, SEEK_END);
	size_t len = ftell(f);
	rewind(f);
	char* text = malloc(len);
	fread(text, 1, len, f);
	fclose(f);
	
	TOKENIZER_STATE state;
	state.filename = dcfg_from_c_str("test.cfg");
	state.str.start = text;
	state.str.end = text + len;
	state.line = 1;
	state.vtable = &default_vtable;
	
	TOKEN tok = _dcfg_get_next_token(&state);
	while(tok.type != TOKEN_EOF)
	{
		printf("|%.*s|\n", (int)dcfg_string_length(tok.str), tok.str.start);
		tok = _dcfg_get_next_token(&state);
		if(tok.type == TOKEN_ERROR)
			break;
	}
	
	free(text);
	return 0;
}
