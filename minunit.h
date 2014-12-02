#ifndef MINUNIT_H
#define MINUNIT_H
#include "debug.h"

#define mu_assert(message, test) do { if (!(test)) return KRED message KNRM; } while (0)
#define mu_run_test(test) do { char *message = test(); tests_run++; \
                               if (message) return message; } while (0)
extern int tests_run;
#endif
