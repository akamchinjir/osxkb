#ifndef OSX_KEYS_ERROR_H
#define OSX_KEYS_ERROR_H

#include "common.h"

bool make_error (GError **error, const char *format, ...);
bool make_system_error (GError **error, const char *format, ...);
bool suffix_error (GError **error, const char *format, ...);

#endif
