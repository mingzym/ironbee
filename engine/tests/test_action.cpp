//////////////////////////////////////////////////////////////////////////////
// Licensed to Qualys, Inc. (QUALYS) under one or more
// contributor license agreements.  See the NOTICE file distributed with
// this work for additional information regarding copyright ownership.
// QUALYS licenses this file to You under the Apache License, Version 2.0
// (the "License"); you may not use this file except in compliance with
// the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/// @file
/// @brief IronBee --- Action Test Functions
///
/// @author Craig Forbes <cforbes@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include <ironbee/action.h>
#include <ironbee/server.h>
#include <ironbee/engine.h>
#include <ironbee/mm.h>

#include "gtest/gtest.h"

#include "base_fixture.h"

class ActionTest : public BaseFixture {
};

TEST_F(ActionTest, RegisterTest) {
    ib_status_t status;
    status = ib_action_create_and_register(
        NULL, ib_engine,
        "test_action",
        NULL, NULL,
        NULL, NULL,
        NULL, NULL);
    EXPECT_EQ(IB_OK, status);
}

TEST_F(ActionTest, RegisterDup) {
    ib_status_t status;
    status = ib_action_create_and_register(
        NULL, ib_engine,
        "test_action",
        NULL, NULL,
        NULL, NULL,
        NULL, NULL);
    ASSERT_EQ(IB_OK, status);
    status = ib_action_create_and_register(
        NULL, ib_engine,
        "test_action",
        NULL, NULL,
        NULL, NULL,
        NULL, NULL);
    EXPECT_EQ(IB_EINVAL, status);
}

TEST_F(ActionTest, CallAction) {
    ib_status_t status;
    ib_action_t *action;
    ib_action_inst_t *act;

    status = ib_action_create_and_register(
        &action, ib_engine,
        "test_action",
        NULL, NULL,
        NULL, NULL,
        NULL, NULL);
    ASSERT_EQ(IB_OK, status);

    status = ib_action_inst_create(
        &act,
        ib_engine_mm_main_get(ib_engine),
        ib_context_engine(ib_engine),
        action,
        "parameters"
    );
    ASSERT_EQ(IB_OK, status);

    status = ib_action_inst_execute(act, NULL);
    ASSERT_EQ(IB_OK, status);
}

namespace {
extern "C" {

static bool action_executed = false;
static const char *action_str = NULL;


ib_status_t create_fn(
    ib_mm_t       mm,
    ib_context_t *ctx,
    const char   *parameters,
    void         *instance_data,
    void         *cbdata
)
{
    if (strcmp(parameters, "INVALID") == 0) {
        return IB_EINVAL;
    }
    *(void **)instance_data =
        ib_mm_strdup(mm, parameters);
    return IB_OK;
}

ib_status_t execute_fn(
    const ib_rule_exec_t *rule_exec,
    void                 *data,
    void                 *cbdata
)
{
    action_executed = true;
    action_str = (const char *)data;
    return IB_OK;
}
}
}

TEST_F(ActionTest, ExecuteAction) {
    ib_status_t status;
    ib_action_t *action;
    ib_action_inst_t *act;
    const char *params = "parameters";

    status = ib_action_create_and_register(
        &action, ib_engine,
        "test_action",
        create_fn, NULL,
        NULL, NULL,
        execute_fn, NULL);
    ASSERT_EQ(IB_OK, status);

    status = ib_action_inst_create(
        &act,
        ib_engine_mm_main_get(ib_engine),
        ib_context_engine(ib_engine),
        action,
        "INVALID"
    );
    ASSERT_EQ(IB_EINVAL, status);

    status = ib_action_inst_create(
        &act,
        ib_engine_mm_main_get(ib_engine),
        ib_context_engine(ib_engine),
        action,
        params
    );
    ASSERT_EQ(IB_OK, status);

    action_executed = false;
    status = ib_action_inst_execute(act, NULL);
    ASSERT_EQ(IB_OK, status);
    ASSERT_TRUE(action_executed);
    EXPECT_STREQ(action_str, params);
}
