#ifndef CONFIG_H
#define CONFIG_H

#include <ipfixcol2.h>

struct config {
    const char *profiles_filename;
};

struct config *
config_parse(ipx_ctx_t *ctx, const char *params);

void
config_destroy(struct config *cfg);

#endif // CONFIG_H