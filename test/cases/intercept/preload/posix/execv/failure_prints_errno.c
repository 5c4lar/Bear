// REQUIRES: preload, c_api_execv
// RUN: %{compile} '-D_PROGRAM="/path/to/not/existing"' -o %t %s
// RUN: %t > %t.without.errno
// RUN: %{intercept} --output %t.sqlite3 -- %t > %t.with.errno
// RUN: diff %t.with.errno %t.without.errno
// RUN: assert_intercepted %t.sqlite3 count -eq 1
// RUN: assert_intercepted %t.sqlite3 contains -program %t

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined HAVE_UNISTD_H
#include <unistd.h>
#endif

int main()
{
    char *const program = _PROGRAM;
    char *const argv[] = { _PROGRAM, "hi there", 0 };

    if (-1 == execv(program, argv)) {
        const int error = errno;
        printf("errno: %d (%s)\n", error, strerror(error));
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}
