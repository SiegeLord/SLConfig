#ifndef INTERNAL_UTILS_H
#define INTERNAL_UTILS_H

#include <stdlib.h>

#include "dconfig/dconfig.h"
#include "dconfig/internal/tokenizer.h"

void _dcfg_print_error_prefix(DCONFIG_STRING filename, size_t line, DCONFIG_VTABLE* table);
void _dcfg_expected_after_error(TOKENIZER_STATE* state, size_t line, DCONFIG_STRING expected, DCONFIG_STRING after, DCONFIG_STRING actual);
void _dcfg_expected_error(TOKENIZER_STATE* state, size_t line, DCONFIG_STRING expected, DCONFIG_STRING actual);

#endif
