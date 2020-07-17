#include "config.h"

#include <string.h>
#include <stdlib.h>

/*
 * <params>
 *   <profiles>...</profiles>
 * </params>
 */ 

enum params_xml_nodes {
    PROFILES_FILENAME = 1,
};

static const struct fds_xml_args args_params[] = {
    FDS_OPTS_ROOT("params"),
    FDS_OPTS_ELEM(PROFILES_FILENAME, "filename", FDS_OPTS_T_STRING, 0),
    FDS_OPTS_END
};

struct config *
config_parse(ipx_ctx_t *ctx, const char *params)
{
    struct config *cfg = NULL;
    fds_xml_t *parser = NULL;

    cfg = calloc(1, sizeof(struct config));
    if (!cfg) {
        IPX_CTX_ERROR(ctx, "Memory allocation error (%s:%d)", __FILE__, __LINE__);
        goto error;
    }

    parser = fds_xml_create();
    if (!parser) {
        IPX_CTX_ERROR(ctx, "Memory allocation error (%s:%d)", __FILE__, __LINE__);
        goto error;
    }

    if (fds_xml_set_args(parser, args_params) != FDS_OK) {
        IPX_CTX_ERROR(ctx, "Failed to parse the description of an XML document!");
        goto error;
    }

    fds_xml_ctx_t *params_ctx = fds_xml_parse_mem(parser, params, true);
    if (params_ctx == NULL) {
        IPX_CTX_ERROR(ctx, "Failed to parse the configuration: %s", fds_xml_last_err(parser));
        goto error;
    }

    const struct fds_xml_cont *content;
    while (fds_xml_next(params_ctx, &content) == FDS_OK) {
        switch (content->id) {
        case PROFILES_FILENAME:
            assert(content->type == FDS_OPTS_T_STRING);
            if (strlen(content->ptr_string) == 0) {
                IPX_CTX_ERROR(ctx, "Profiles filename is empty!");
                goto error;  
            }
            cfg->profiles_filename = strdup(content->ptr_string);
            if (!cfg->profiles_filename) {
                IPX_CTX_ERROR(ctx, "Memory allocation error (%s:%d)", __FILE__, __LINE__);
                goto error;
            }            
            break;
        }
    }

    fds_xml_destroy(parser);
    return cfg;

error:
    fds_xml_destroy(parser);
    free(cfg);
    return NULL;
}

void
config_destroy(struct config *cfg)
{
    free(cfg->profiles_filename);
    free(cfg);
}