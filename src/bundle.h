#ifndef OSX_KB_BUNDLE_H
#define OSX_KB_BUNDLE_H

#include "common.h"

typedef struct _Bundle Bundle;

struct _Bundle
{
  const char *name;      // (no default)
  const char *base_url;  // default: akam.chinjir
  const char *version;   // default: ""

  char *bundle_name;     // with the extension (allows spaces and caps)
  char *url;             // uses lowercased and unspaced NAME

  GList *keyboards;
};

Bundle *bundle_new (const char *config_file, GError **error);

bool bundle_load_data (Bundle *bundle, GError **error);
bool bundle_write_bundle (Bundle *bundle, GError **error);

#endif
