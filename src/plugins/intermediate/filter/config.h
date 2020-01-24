#ifndef CONFIG_H
#define CONFIG_H

#include <ipfixcol2.h>

typedef struct config {
    const char *expr;
} config_s;

config_s *
config_parse(ipx_ctx_t *ctx, const char *params);

void
config_destroy(config_s *cfg);

#endif // CONFIG_H