#ifndef INTERNAL_PARSER_H
#define INTERNAL_PARSER_H

#include "dconfig/internal/dconfig.h"
#include "dconfig/internal/tokenizer.h"

bool _dcfg_parse_file(DCONFIG* config, DCONFIG_NODE* root, DCONFIG_STRING filename, DCONFIG_STRING file);
bool _dcfg_parse_aggregate(DCONFIG* config, DCONFIG_NODE* aggregate, TOKENIZER_STATE* state);

#endif

