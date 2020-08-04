#ifndef OSX_KB_PREFIX_MAP_H
#define OSX_KB_PREFIX_MAP_H

#include "common.h"

typedef struct _PrefixMap PrefixMap;
typedef struct _PrefixNode PrefixNode;

struct _PrefixMap
{
  PrefixNode *children;
  int n_children;
  size_t capacity;
};

struct _PrefixNode
{
  char key;
  void *data;
  PrefixMap submap;
};

PrefixMap *prefix_map_new (void);

void **prefix_map_lookup (PrefixMap *map, const char *key); // returns a pointer to where you can insert, if it's not already there
void *prefix_map_get (PrefixMap *map, const char *key); // returns NULL if it's not there

void *prefix_map_get_prefix (PrefixMap *map, const char **key);

#endif
