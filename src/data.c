#include "data.h"
#include <string.h>
#include "util.h"

/**
 * Public procedures
 */

char *
data_load (const char *data_name, GError **error)
{
  char *data = NULL;
  if (util_file_exists (data_name))
  {
    g_file_get_contents (data_name, &data, NULL, error);
  }
  else
  {
    if (strcmp (data_name, "ansi.qwerty") == 0)
      data = data_load_internal (KB_DATA_ANSI_QWERTY, error);
    else if (strcmp (data_name, "ansi.dvorak") == 0)
      data = data_load_internal (KB_DATA_ANSI_DVORAK, error);
    else if (strcmp (data_name, "osxopt") == 0)
      data = data_load_internal (KB_DATA_OSXOPT, error);
    else
      make_error (error, "Unknown file: %s", data_name);
  }
  return data;
}

char *
data_load_internal (KbData data_id, GError **error)
{
  switch (data_id)
  {
  case KB_DATA_OSXOPT:
    return g_strdup ((const char *)osxopt);
  case KB_DATA_ANSI_QWERTY:
    return g_strdup ((const char *)ansi_qwerty);
  case KB_DATA_ANSI_DVORAK:
    return g_strdup ((const char *)ansi_dvorak);
  }
  g_assert_not_reached ();
  return NULL;
}

char
lookup_ascii (const char *name)
{
  int low = 0;
  int high = N_NAMES - 1;
  while (low <= high)
  {
    int mid = (low + high) / 2;
    int cmp = strcmp (name, name_to_ascii[mid].name);
    if (cmp < 0)
      high = mid - 1;
    else if (cmp > 0)
      low = mid + 1;
    else
      return name_to_ascii[mid].ascii;
  }

  return '\0';
}

const char *
lookup_name (int ascii)
{
  if (ascii < 0 || ascii > 127)
    return NULL;

  return name_lookup[ascii];
}

const char *
lookup_output (int ascii)
{
  if (ascii < 0 || ascii > 127)
    return NULL;

  return output_lookup[ascii];
}

const char *
lookup_ctrl_code (const char *literal)
{
  if (literal[1] == 0 && literal[0] >= 0)
    return control_codes[(int)*literal];
  return
    NULL;
}

