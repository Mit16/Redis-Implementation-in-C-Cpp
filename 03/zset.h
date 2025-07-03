#ifndef ZSET_H
#define ZSET_H

#include "hashtable.h"
#include "avl.h"

struct ZNode {
    struct HNode hmap; // hashtable node
    struct AVLNode tree; // AVL tree node
    double score = 0; // score for sorting
    size_t len = 0; // length of the name
    char name[0]; // variable-length name
};

struct ZSet {
    struct AVLNode *root = NULL; // root of the AVL tree
    struct HMap hmap; // hashtable
};

bool zset_insert(ZSet *zset, const char *name, size_t len, double score);
ZNode *zset_lookup(ZSet *zset, const char *name, size_t len);
void zset_delete(ZSet *zset, ZNode *node);
ZNode *zset_seekge(ZSet *zset, double score, const char *name, size_t len);
void zset_clear(ZSet *zset);
ZNode *znode_offset(ZNode *node, int64_t offset);
#endif // ZSET_H