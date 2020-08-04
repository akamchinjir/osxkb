#include "bundle.h"
#include <ctype.h>
#include <string.h>
#include "keyboard.h"
#include "util.h"

typedef struct _KeyboardMeta KeyboardMeta;

struct _KeyboardMeta
{
  const char *name;
  const char *url;
  const char *language;
  const char *icons_source;

  GList *datafiles;
  const char *base_encoding;
  bool osxopt;
  
  int capslock_policy;
};

static char *make_url (const char *base_url, const char *name);

static bool parse_bool (bool *out, const char *in, GError **error);
static bool parse_capslock_policy (int *out, const char *in, GError **error);

static bool bundle_finish (Bundle *bundle, GError **error);
static void bundle_init_kb_meta (Bundle *bundle, KeyboardMeta *meta);
static bool bundle_add_keyboard (Bundle *bundle, KeyboardMeta *meta, GError **error);

static bool bundle_config_keyboard (Bundle *bundle, KeyboardMeta *meta, const char *key, const char *value, GError **error);
static bool bundle_config (Bundle *bundle, const char *key, const char *value, GError **error);

static bool bundle_write_keylayouts (Bundle *bundle, GError **error);
static bool bundle_write_keyboard_info_plists (Bundle *bundle, Out *out, GError **error);
static bool bundle_write_info_plist (Bundle *bundle, const char *path, GError **error);
static bool bundle_copy_icons (Bundle *bundle, const char *path, GError **error);
static bool bundle_move_keylayouts (Bundle *bundle, const char *path, GError **error);

char *
make_url (const char *base_url, const char *name)
{
  char *url = g_malloc (strlen (base_url) + strlen (name) + 2);
  char *out = stpcpy (url, base_url);
  *out++ = '.';
  
  const char *ptr = name;
  while (*ptr)
  {
    while (isspace (*ptr))
      ++ptr;

    *out++ = (char)tolower (*ptr++);
  }
  *out = '\0';

  return url;
}

bool
parse_bool (bool *out, const char *in, GError **error)
{
  if (strcmp (in, "true") == 0)
    *out = true;
  else if (strcmp (in, "false") == 0)
    *out = false;
  else
    return make_error (error, "Expected a boolean, `true' or `false', found `%s'", in);

  return true;
}

bool
parse_capslock_policy (int *out, const char *in, GError **error)
{
  if (strcmp (in, "switch-im") == 0)
    *out = CAPSLOCK_SWITCHES_IM;
  else if (strcmp (in, "disable") == 0)
    *out = CAPSLOCK_DISABLES;
  else
    return make_error (error, "Unknown capslock policy `%s', expected `switch-im' or `disables'", in);

  return true;
}

bool
bundle_finish (Bundle *bundle, GError **error)
{
  if (bundle->name == NULL)
    return make_error (error, "Bundle configured without a name");

  bundle->bundle_name = g_strconcat (bundle->name, ".bundle", NULL);
  if (util_file_exists (bundle->bundle_name))
    return make_error (error, "%s already exists", bundle->bundle_name);

  if (bundle->base_url == NULL)
    bundle->base_url = "org.chinjir.keyboardlayout";
  else
    bundle->base_url = g_strconcat (bundle->base_url, ".keyboardlayout", NULL);
  if (bundle->version == NULL)
    bundle->version = "";

  bundle->url = make_url (bundle->base_url, bundle->name);

  return true;
}

void
bundle_init_kb_meta (Bundle *bundle, KeyboardMeta *meta)
{
  memset (meta, '\0', sizeof (KeyboardMeta));
}

bool
bundle_add_keyboard (Bundle *bundle, KeyboardMeta *meta, GError **error)
{
  if (meta->name == NULL)
    meta->name = bundle->name;

  for (GList *iter = bundle->keyboards; iter != NULL; iter = iter->next)
  {
    Keyboard *kb = iter->data;
    if (strcmp (kb->name, meta->name) == 0)
      return make_error (error, "Duplicate keylayout file name `%s'", meta->name);
  }

  if (meta->datafiles == NULL)
    return make_error (error, "No datafile configured for %s keyboard", meta->name);

  size_t bundle_name_len = strlen (bundle->name);
  size_t kb_name_len = strlen (meta->name);

  const char *url_extra;
  if (bundle_name_len < kb_name_len && strncmp (bundle->name, meta->name, bundle_name_len) == 0)
    url_extra = meta->name + bundle_name_len;
  else
    url_extra = meta->name;
  
  meta->url = make_url (bundle->url, url_extra);

  if (meta->language == NULL)
    meta->language = "en";

  Keyboard *kb = keyboard_new (meta->name,
                               meta->url,
                               meta->language,
                               meta->icons_source,
                               meta->datafiles,
                               meta->base_encoding,
                               meta->osxopt,
                               meta->capslock_policy);

  bundle->keyboards = g_list_append (bundle->keyboards, kb);

  return true;
}

bool
bundle_config_keyboard (Bundle *bundle, KeyboardMeta *meta, const char *key, const char *value, GError **error)
{
  // name, url, language, icons, datafile, base-encoding, osxopt, disable-on-capslock
  if (strcmp (key, "name") == 0)
    meta->name = value;
  else if (strcmp (key, "language") == 0)
    meta->language = value;
  else if (strcmp (key, "base-encoding") == 0)
    meta->base_encoding = value;
  else if (strcmp (key, "osxopt") == 0)
    return parse_bool (&meta->osxopt, value, error);
  else if (strcmp (key, "datafile") == 0)
    meta->datafiles = g_list_append (meta->datafiles, (char *)value);
  else if (strcmp (key, "capslock-policy") == 0)
    return parse_capslock_policy (&meta->capslock_policy, value, error);
  else if (strcmp (key, "icons") == 0)
    meta->icons_source = value;

  return true;
}

bool
bundle_config (Bundle *bundle, const char *key, const char *value, GError **error)
{
  // name, baseurl, version, switch-im-on-capslock

  if (strcmp (key, "name") == 0)
    bundle->name = value;
  else if (strcmp (key, "baseurl") == 0)
    bundle->base_url = value;
  else if (strcmp (key, "version") == 0)
    bundle->version = value;
  else
    return make_error (error, "Unknown bundle configuration key `%s'", key);

  return true;                   
}

bool
bundle_write_keylayouts (Bundle *bundle, GError **error)
{
  for (GList *iter = bundle->keyboards; iter != NULL; iter = iter->next)
  {
    Keyboard *kb = iter->data;
    if (!keyboard_write_keylayout (kb, error))
      return false;
  }

  return true;
}

bool
bundle_write_keyboard_info_plists (Bundle *bundle, Out *out, GError **error)
{
  for (GList *iter = bundle->keyboards; iter != NULL; iter = iter->next)
  {
    Keyboard *kb = iter->data;
    if (!keyboard_write_info_plist (kb, out, error))
      return false;
  }

  return true;
}

bool
bundle_write_info_plist (Bundle *bundle, const char *path, GError **error)
{
  Out *out = out_open (path, error);
  if (out == NULL)
    return false;

  if (!out_print (out, error,
                  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                  "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
                  "<plist version=\"1.0\">\n"
                  "  <dict>\n")

      || !out_printf (out, error,
                      "    <key>CFBundleIdentifier</key>\n"
                      "    <string>%s</string>\n",
                      bundle->url)

      || !out_printf (out, error,
                      "    <key>CFBundleName</key>\n"
                      "    <string>%s</string>\n",
                      bundle->name) // not ->bundle_name, because that includes ".bundle"
      || !out_printf (out, error,
                      "    <key>CFBundleVersion</key>\n"
                      "    <string>%s</string>\n",
                      bundle->version)

      || !bundle_write_keyboard_info_plists (bundle, out, error)

      || !out_print (out, error,
                     "  </dict>\n"
                     "</plist>\n"))
  {
    /** actually nothing */
  }

  out_close (out, error);

  return *error == NULL;
}

bool
bundle_copy_icons (Bundle *bundle, const char *path, GError **error)
{
  for (GList *iter = bundle->keyboards; iter != NULL; iter = iter->next)
  {
    Keyboard *kb = iter->data;
    if (kb->icons_source)
    {
      char *icons_path = g_build_filename (path, kb->icons_basename, NULL);
      if (!util_copy_file (kb->icons_source, icons_path, error))
        return false;
    }
  }

  return true;
}

bool
bundle_move_keylayouts (Bundle *bundle, const char *path, GError **error)
{
  for (GList *iter = bundle->keyboards; iter != NULL; iter = iter->next)
  {
    Keyboard *kb = iter->data;
    char *full_path = g_build_filename (path, kb->keylayout_basename, NULL);
    if (!util_move_file (kb->keylayout_basename, full_path, error))
      return false;
  }

  return true;
}

/**
 * Public procedures
 */

Bundle *
bundle_new (const char *config_file, GError **error)
{
  char *data;
  if (!g_file_get_contents (config_file, &data, NULL, error))
    return NULL;

  Bundle *bundle = g_slice_alloc0 (sizeof (Bundle));

  KeyboardMeta kb_meta = { NULL, };
  bool on_keyboard = false;

  int lineno = 1;
  char *ptr = data;
  while (true)
  {
    while (isspace (*ptr))
    {
      if (*ptr == '\n')
        ++lineno;
      ++ptr;
    }

    if (*ptr == '\0')
      break;

    char *line = ptr;
    char *end = strchr (line, '\n');
    if (end == NULL)
      end = strchr (line, '\0');

    bool at_newline;
    char *next;
    if (*end == '\n')
    {
      at_newline = true;
      next = end + 1;
      *end = '\0';
    }
    else
    {
      at_newline = false;
    }

    if (*ptr == '#')
    {
      /** do nothing with this line */
    }
    else if (*line == '[' && *(end - 1) == ']')
    {
      char *header = line + 1;
      *(end - 1) = '\0';

      if (strcmp (header, "keyboard") == 0)
      {
        if (on_keyboard)
        {
          if (!bundle_add_keyboard (bundle, &kb_meta, error))
          {
            suffix_error (error, "%s, line %d", config_file, lineno);
            return NULL;
          }
        }
        else
        {
          if (!bundle_finish (bundle, error))
          {
            suffix_error (error, "%s, line %d", config_file, lineno);
            return NULL;
          }
          on_keyboard = true;
        }
        bundle_init_kb_meta (bundle, &kb_meta);
      }
      else
      {
        make_error (error, "Unknown configuration section `%s' (expected `keyboard'): %s, line %d", header, config_file, lineno);
        return NULL;
      }
    }
    else // expect key = value
    {
      char *eq = ptr;
      while (*eq != '=' && *eq != '\0')
        ++eq;

      if (*eq == '\0')
      {
        make_error (error, "Missing `=': %s, line %d", config_file, lineno);
        return NULL;
      }

      char *key = ptr;
      char *value = eq + 1;
      while (isspace (*value))
        ++value;
      while (eq != key && isspace (*(eq-1)))
        --eq;
      *eq = '\0'; // key set

      char *ptr = end;
      while (isspace (*(ptr - 1)))
        --ptr;
      *ptr = '\0'; // value set

      if (on_keyboard)
        bundle_config_keyboard (bundle, &kb_meta, key, value, error);
      else
        bundle_config (bundle, key, value, error);

      if (*error)
      {
        suffix_error (error, "%s, line %d", config_file, lineno);
        return NULL;
      }
    }

    if (at_newline)
    {
      ptr = next;
      ++lineno;
    }
    else
    {
      break;
    }
  }

  if (on_keyboard)
  {
    if (!bundle_add_keyboard (bundle, &kb_meta, error))
    {
      suffix_error (error, "%s, line %d", config_file, lineno);
      return NULL;
    }
  }
  else
  {
    make_error (error, "No keyboards configured: %s", config_file);
    return NULL;
  }

  return bundle;
}

bool
bundle_load_data (Bundle *bundle, GError **error)
{
  for (GList *iter = bundle->keyboards; iter != NULL; iter = iter->next)
  {
    Keyboard *kb = iter->data;
    if (!keyboard_load_data (kb, error))
      return false;
  }

  return true;
}

bool
bundle_write_bundle (Bundle *bundle, GError **error)
{
  char *contents_path = g_build_filename (bundle->bundle_name, "Contents", NULL);
  char *info_plist_path = g_build_filename (contents_path, "Info.plist", NULL);
  char *resources_path = g_build_filename (contents_path, "Resources", NULL);

  if (!bundle_write_keylayouts (bundle, error)
      || !util_mkdir (bundle->bundle_name, error)
      || !util_mkdir (contents_path, error)
      || !util_mkdir (resources_path, error)
      || !bundle_write_info_plist (bundle, info_plist_path, error)
      || !bundle_copy_icons (bundle, resources_path, error)
      || !bundle_move_keylayouts (bundle, resources_path, error))
  {
    return false;
  }
  
  return true;
}
