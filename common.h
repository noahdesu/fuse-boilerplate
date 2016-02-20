#ifndef COMMON_H
#define COMMON_H
#include <gasnet.h>

#define GASNET_SAFE(fncall) do {                                     \
    int _retval;                                                     \
    if ((_retval = fncall) != GASNET_OK) {                           \
      fprintf(stderr, "ERROR calling: %s\n"                          \
                      " at: %s:%i\n"                                 \
                      " error: %s (%s)\n",                           \
              #fncall, __FILE__, __LINE__,                           \
              gasnet_ErrorName(_retval), gasnet_ErrorDesc(_retval)); \
      fflush(stderr);                                                \
      gasnet_exit(_retval);                                          \
      exit(_retval);                                                 \
    }                                                                \
  } while(0)

#define BLOCK_SIZE 4096

class Node;
struct Block {
  Node *node;
  size_t addr;
  size_t size;
};

struct gassyfs_opts {
  int rank0_alloc;
  int local_mode;
  size_t heap_size;
};

#endif
