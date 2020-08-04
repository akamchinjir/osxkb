#ifndef OSX_KB_KEYMAP_H
#define OSX_KB_KEYMAP_H

#include "common.h"
#include "out.h"

typedef struct _KeyMap KeyMap;
typedef struct _KeyMapSet KeyMapSet;
typedef struct _KeyMapSubset KeyMapSubset;
typedef struct _Result Result;

enum
  {
    NO_MODIFIER = 0,
    MOD_OPTION = 1 << 0,
    MOD_CONTROL = 1 << 1
  };

enum
  {
    SHIFTLESS = 0, 
    SHIFTY = 1 << 0,
    CAPSLOCK = 1 << 1
  };

enum
  {
    NO_RESULT,
    RESULT_OUTPUT,
    RESULT_ACTION
  };

struct _KeyMapSet
{
  bool is_control;
  bool capslock_disables;
  bool active_capslock;

  KeyMapSubset *plain_maps;
  KeyMapSubset *opt_maps;
  KeyMapSubset *backup_maps; // a copy of plain_maps, once that's been initialised

  bool dirty;
};

struct _KeyMapSubset
{
  bool is_control;
  bool is_option;
  bool capslock_disables;
  bool active_capslock;
  
  KeyMap *shiftless_map;
  KeyMap *shifty_map;
  KeyMap *capslock_map;
  KeyMap *backup_map; // only when is_control
};

struct _Result
{
  int result_type;
  const char *content;
};

struct _KeyMap
{
  Result keys[128];
  GList *mods;
  int index;
};

KeyMapSet *key_map_set_new (bool is_control, bool capslock_disables);

void key_map_set_set_result (KeyMapSet *set, int mods, int shift_state, int code, int result_type, const char *content);
const Result *key_map_set_get_result (KeyMapSet *set, int mods, int shift_state, int code);

void key_map_set_maybe_unshift (KeyMapSet *set);
void key_map_set_make_backup (KeyMapSet *set);
void key_map_set_assign_mods (KeyMapSet *set, int *index);

bool key_map_set_write_mods (KeyMapSet *set, Out *out, GError **error);
bool key_map_set_write_maps (KeyMapSet *set, Out *out, GError **error);

void key_map_set_set_capslock_active (KeyMapSet *set);

#endif
