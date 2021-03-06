/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************/

/**
 * @file
 * @brief IronBee --- Lua Runtime
 *
 * Things used to manage Lua runtime stacks used throughout `ibmod_lua`.
 *
 * A runtime includes a little meta information and a lua_State pointer.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */
#ifndef __MODULES__LUA_RUNTIME_H
#define __MODULES__LUA_RUNTIME_H

#include "lua/ironbee.h"
#include "lua_common_private.h"
#include "lua_private.h"

#include <ironbee/mpool_lite.h>
#include <ironbee/resource_pool.h>

/**
 * Per-connection module data containing a Lua runtime.
 *
 * Created for each connection and stored as the module's connection data.
 */
struct modlua_runtime_t {
    lua_State       *L;         /**< Lua stack */
    ssize_t          use_count; /**< Number of times this stack is used. */
    ib_mpool_lite_t *mp;        /**< Memory pool for this runtime. */
    ib_resource_t   *resource;  /**< Bookkeeping for modlua_releasestate(). */
};
typedef struct modlua_runtime_t modlua_runtime_t;

/**
 * The type of reloading that must be done to initialize a new Lua stack.
 *
 * Lua requires many stacks to be created and initialized. Each
 * stack must have rules and modules reloaded into it in a specific way.
 * This enumeration labels each type of reloading record as a
 * Lua module reload or a Lua rule reload.
 */
enum modlua_reload_type_t {
    MODLUA_RELOAD_RULE,   /**< Reload a Lua rule. */
    MODLUA_RELOAD_MODULE  /**< Reload a Lua module. */
};
typedef enum modlua_reload_type_t modlua_reload_type_t;

/**
 * This represents a Lua item that must be reloaded.
 *
 * Reloading happens when a new Lua stack is created for the
 * resource pool (created by modlua_runtime_resource_pool_create())
 * and when a site-specific Lua file must be loaded.
 *
 * To maximize performance all Lua scripts should be put in in the main
 * context and as few as possible should be put in site contexts.
 */
struct modlua_reload_t {
    modlua_reload_type_t  type;    /**< Is this a module or a rule? */
    ib_module_t          *module;  /**< Lua module (not ibmod_lua.so). */
    const char           *file;    /**< File of the rule or module code. */
    const char           *rule_id; /**< Rule if this is a rule type. */
};
typedef struct modlua_reload_t modlua_reload_t;

/**
 * Set the limit on the number of times a Lua stack may be used.
 *
 * When the limit is exceeded the stack is invalided, destroyed, and replaced
 * in the resource pool.
 *
 * @param[in] cfg The configuration object returned to the user by
 *            modlua_runtime_resource_pool_create().
 * @param[in] limit The limit.
 *
 * @return
 * - IB_OK On success.
 * - IB_EINVAL If @a limit is not a positive, non-zero value.
 */
ib_status_t modlua_runtime_cfg_set_stack_use_limit(
    modlua_runtime_cfg_t *cfg,
    ssize_t               limit
)
NONNULL_ATTRIBUTE(1);

/**
 * Create a resource pool that manages @ref modlua_runtime_t instances.
 *
 * @param[out] resource_pool Resource pool to create.
 * @param[in] ib The IronBee engine made available to the Lua runtime.
 * @param[in] module The IronBee module structure.
 * @param[in] mm The memory manager the resource pool will use.
 * @param[in] cfg Runtime configuration parameters that the user may set
 *            during configuration time.
 *
 *  @returns
 *  - IB_OK On success
 *  - IB_EALLOC If callback data structure cannot be allocated out of @a mp.
 *  - Failures codes ib_resource_pool_create().
 */
ib_status_t modlua_runtime_resource_pool_create(
    ib_resource_pool_t   **resource_pool,
    ib_engine_t           *ib,
    ib_module_t           *module,
    ib_mm_t                mm,
    modlua_runtime_cfg_t **cfg
);

/**
 * Reload @a ctx and all parent contexts except the main context.
 *
 * When a Lua stack is given from the resource pool to a connection,
 * it is assumed that the stack has all the files referenced in the main
 * context already loaded. All site-specific scripts must be reloaded.
 *
 * @param[in] ib The IronBee engine.
 * @param[in] module The module object for the Lua module.
 * @param[in] ctx The configuration context. If this is the main context,
 *            this function does nothing. If this is a context other than
 *            the main context it's parent is recursively passed
 *            to this function until @a ctx is the main context.
 * @param[in] L The Lua State to reload Lua scripts into.
 *
 * @returns
 * - IB_OK On success or if @a ctx is the main context.
 * - IB_EALLOC On an allocation error.
 * - IB_EINVAL If the Lua script fails to load.
 */
ib_status_t modlua_reload_ctx_except_main(
    ib_engine_t  *ib,
    ib_module_t  *module,
    ib_context_t *ctx,
    lua_State    *L
);

/**
 * Reload the main context Lua files.
 *
 * @param[in] ib IronBee engine.
 * @param[in] module The Lua module structure.
 * @param[in] L The Lua state to reload the files into.
 *
 * @returns
 * - IB_OK
 * - Failures of modlua_reload_ctx().
 */
ib_status_t modlua_reload_ctx_main(
    ib_engine_t *ib,
    ib_module_t *module,
    lua_State   *L
);

/**
 * Push the file and the type into the reload list.
 *
 * This list is used to reload modules and rules into independent lua stacks
 * per transaction.
 *
 * @param[in] ib IronBee engine.
 * @param[in] cfg Configuration.
 * @param[in] type The type of the thing to reload.
 * @param[in] module For MODLUA_RELOAD_MODULE types this is a pointer
 *            to the Lua script's module structure.
 * @param[in] rule_id The rule id. This is copied.
 * @param[in] file Where is the Lua file to load. This is copied.
 */
ib_status_t modlua_record_reload(
    ib_engine_t          *ib,
    modlua_cfg_t         *cfg,
    modlua_reload_type_t  type,
    ib_module_t          *module,
    const char           *rule_id,
    const char           *file
);

/**
 * Return a @ref modlua_runtime_t to the resource pool.
 *
 * @param[in] ib IronBee engine.
 * @param[in] cfg The module configuration.
 * @param[out] modlua_runtime The runtime that should be returned to the
 *             resource pool.
 *
 * @returns
 * - IB_OK On success.
 * - Other on failure.
 */
ib_status_t modlua_releasestate(
    ib_engine_t      *ib,
    modlua_cfg_t     *cfg,
    modlua_runtime_t *modlua_runtime
);

/**
 * Acquire a @ref modlua_runtime_t from the resource pool.
 *
 * @param[in] ib IronBee engine.
 * @param[in] cfg The module configuration.
 * @param[out] modlua_runtime The fetched runtime is placed here.
 *
 * @returns
 * - IB_OK On success.
 * - IB_DECLINED If no resources are available.
 * - Other on failure.
 */
ib_status_t modlua_acquirestate(
    ib_engine_t       *ib,
    modlua_cfg_t      *cfg,
    modlua_runtime_t **modlua_runtime
);

#endif /* __MODULES__LUA_RUNTIME_H */
