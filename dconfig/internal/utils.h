#ifndef INTERNAL_UTILS_H
#define INTERNAL_UTILS_H

#include <stdlib.h>

#include "dconfig/dconfig.h"

void _dcfg_print_error_prefix(DCONFIG_STRING filename, size_t line, DCONFIG_VTABLE* table);

#endif
