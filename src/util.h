#ifndef OSX_KB_UTIL_H
#define OSX_KB_UTIL_H

#include "common.h"

bool util_file_exists (const char *path);

bool util_mkdir (const char *path, GError **error);

bool util_move_file (const char *src, const char *dest, GError **error);
bool util_copy_file (const char *src, const char *dest, GError **error);

#endif
