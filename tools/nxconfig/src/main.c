/**
 * \file            main.c
 * \brief           Offline native C entry for inspect and static generation
 * \author          Nexus Team
 * \version         0.1.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nxconfig/nxconfig.h"
#include <stdio.h>
#include <string.h>

static void usage(void) {
    (void)fputs("Usage: nxconfig inspect|generate --source-root DIR "
                "--assembly FILE [--output DIR]\n",
                stderr);
}

int main(int argc, char* argv[]) {
    if (argc < 2 ||
        (strcmp(argv[1], "inspect") != 0 && strcmp(argv[1], "generate") != 0)) {
        usage();
        return 2;
    }
    const char* root = NULL;
    const char* authored = NULL;
    const char* output = NULL;
    for (int i = 2; i < argc; i += 2) {
        if (i + 1 >= argc) {
            usage();
            return 2;
        }
        const char** option = strcmp(argv[i], "--source-root") == 0 ? &root
                              : strcmp(argv[i], "--assembly") == 0  ? &authored
                              : strcmp(argv[i], "--output") == 0    ? &output
                                                                    : NULL;
        if (option == NULL || *option != NULL) {
            usage();
            return 2;
        }
        *option = argv[i + 1];
    }
    bool generate = strcmp(argv[1], "generate") == 0;
    if (root == NULL || authored == NULL || (generate != (output != NULL))) {
        usage();
        return 2;
    }
    nxconfig_diagnostic_t diagnostic = {0};
    nxconfig_assembly_t* assembly = NULL;
    nxconfig_result_t result =
        nxconfig_resolve(root, authored, &assembly, &diagnostic);
    if (result == NXCONFIG_OK && generate)
        result = nxconfig_emit(assembly, output, &diagnostic);
    if (result != NXCONFIG_OK)
        (void)fprintf(stderr, "%s: %s: %s (code %u)\n", diagnostic.source,
                      diagnostic.context, diagnostic.message, (unsigned)result);
    else {
        const nxconfig_platform_t* platform = nxconfig_platform(assembly);
        (void)printf("board=%s part=%s define=%s startup=%s\n"
                     "controllers=%zu spi_endpoints=%zu backend=baremetal\n",
                     platform->board, platform->part, platform->device_define,
                     platform->startup, nxconfig_controller_count(assembly),
                     nxconfig_spi_endpoint_count(assembly));
    }
    nxconfig_destroy(assembly);
    return result == NXCONFIG_OK ? 0 : 1;
}
