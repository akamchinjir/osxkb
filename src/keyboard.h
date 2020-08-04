#ifndef OSX_KB_KEYBOARD
#define OSX_KB_KEYBOARD

#include "common.h"
#include "keymap.h"
#include "prefixmap.h"

typedef struct _Keyboard Keyboard;

enum
  {
    PLAIN_CAPSLOCK,
    CAPSLOCK_SWITCHES_IM,
    CAPSLOCK_DISABLES
  };

struct _Keyboard
{
  // url, name, language, icons, capslock_disables
  const char *name;
  const char *keylayout_basename;
  const char *icons_basename;
  const char *url;
  const char *language;
  const char *icons_source;
  GList *datafiles;
  const char *base_encoding;
  bool osxopt;
  int capslock_policy;

  KeyMapSet *base_keymaps;
  KeyMapSet *control_keymaps;

  PrefixMap *literals;
  GTree *actions;
  GTree *terminators;

  bool active_capslock; // this means that for some key in the base encoding, the result when holding capslock is distinct from both shifty and shiftless
};

Keyboard *keyboard_new (const char *name,
                        const char *url,
                        const char *language,
                        const char *icons_source,
                        GList *datafiles,
                        const char *base_encoding,
                        bool osxopt,
                        int capslock_policy);

bool keyboard_load_data (Keyboard *kb, GError **error);

bool keyboard_write_keylayout (Keyboard *kb, GError **error);
bool keyboard_write_info_plist (Keyboard *kb, Out *out, GError **error);

#endif
