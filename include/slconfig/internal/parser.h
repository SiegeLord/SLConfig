#ifndef _INTERNAL_PARSER_H
#define _INTERNAL_PARSER_H

#include "slconfig/internal/slconfig.h"
#include "slconfig/internal/tokenizer.h"

bool _slc_parse_file(SLCONFIG* config, SLCONFIG_NODE* root, SLCONFIG_STRING filename, SLCONFIG_STRING file);

#endif

