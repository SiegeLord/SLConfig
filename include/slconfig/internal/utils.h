#ifndef _INTERNAL_UTILS_H
#define _INTERNAL_UTILS_H

#include <stdlib.h>

#include "slconfig/slconfig.h"
#include "slconfig/internal/tokenizer.h"

void _slc_print_error_prefix(SLCONFIG* config, SLCONFIG_STRING filename, size_t line, SLCONFIG_VTABLE* table);
void _slc_expected_after_error(SLCONFIG* config, TOKENIZER_STATE* state, size_t line, SLCONFIG_STRING expected, SLCONFIG_STRING after, SLCONFIG_STRING actual);
void _slc_expected_error(SLCONFIG* config, TOKENIZER_STATE* state, size_t line, SLCONFIG_STRING expected, SLCONFIG_STRING actual);

#endif
