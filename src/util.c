#include "util.h"
#include <glib/gstdio.h>
#include <gio/gio.h>

/**
 * Public procedures
 */

bool
util_file_exists (const char *path)
{
  GFile *file = g_file_new_for_commandline_arg (path);
  bool exists = g_file_query_exists (file, NULL);
  g_object_unref (file);
  return exists;
}

bool
util_mkdir (const char *path, GError **error)
{
  if (g_mkdir (path, 0755) == 0)
    return true;
  else
    return make_system_error (error, "Could not create directory %s", path);
}

bool
util_move_file (const char *src, const char *dest, GError **error)
{
  if (g_rename (src, dest) != 0)
    return make_system_error (error, "Could not rename %s to %s", src, dest);
  else
    return true;
}

bool
util_copy_file (const char *src, const char *dest, GError **error)
{
  GFile *src_file = g_file_new_for_commandline_arg (src);
  GFile *dest_file = g_file_new_for_commandline_arg (dest);

  bool ret = g_file_copy (src_file, dest_file, G_FILE_COPY_NONE /* no flags */, NULL, NULL, NULL, error);

  g_object_unref (src_file);
  g_object_unref (dest_file);

  return ret;
}
