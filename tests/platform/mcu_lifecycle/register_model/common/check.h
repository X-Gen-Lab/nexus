#ifndef NEXUS_REGISTER_MODEL_CHECK_H
#define NEXUS_REGISTER_MODEL_CHECK_H
#include <stdio.h>
#define CHECK(expression)                                                      \
    do {                                                                       \
        if (!(expression)) {                                                   \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression);   \
            return 1;                                                          \
        }                                                                      \
    } while (0)
#endif
