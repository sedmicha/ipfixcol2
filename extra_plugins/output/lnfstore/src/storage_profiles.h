#ifndef LS_STORAGE_PROFILES_H
#define LS_STORAGE_PROFILES_H

#include "configuration.h"
#include <libnf.h>
#include <ipfixcol2.h>

/**
 * \brief Internal type
 */
typedef struct stg_profiles_s stg_profiles_t;

/**
 * \brief Create a basic storage
 * \param[in] ctx    Instance context (only for logs!)
 * \param[in] params Parameters of this plugin instance
 * \return On success returns a pointer to the storage. Otherwise returns NULL.
 */
stg_profiles_t *
stg_profiles_create(ipx_ctx_t *ctx, const struct conf_params *params);

/**
 * \brief Delete a basic storage
 *
 * Close output file(s) and delete the storage
 * \param[in,out] storage Storage
 */
void
stg_profiles_destroy(stg_profiles_t *storage);

/**
 * \brief Store a LNF record to a storage
 * \param[in,out] storage Storage
 * \param[in]     rec     LNF record
 * \return On success returns 0. Otherwise (failed to write to any of output
 *   files) returns a non-zero value.
 */
int
stg_profiles_store(stg_profiles_t *storage, lnf_rec_t *rec, void *ext_data);

/**
 * \brief Create a new time window
 *
 * Current output file(s) will be closed and new ones will be opened
 * \param[in,out] storage Storage
 * \param[in]     window  Identification time of new window (UTC)
 * \return On success returns 0. Otherwise returns a non-zero value.
 */
int
stg_profiles_new_window(stg_profiles_t *storage, time_t window);

#endif //LS_STORAGE_PROFILES_H
