#!/usr/bin/lua

--[[--------------------------------------------------------------------------
-- Licensed to Qualys, Inc. (QUALYS) under one or more
-- contributor license agreements.  See the NOTICE file distributed with
-- this work for additional information regarding copyright ownership.
-- QUALYS licenses this file to You under the Apache License, Version 2.0
-- (the "License"); you may not use this file except in compliance with
-- the License.  You may obtain a copy of the License at
--
--     http://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.
--]]--------------------------------------------------------------------------

--
-- IronBee Waggle --- Predicate 
--
-- @author Sam Baskinger <sbaskinger@qualys.com>
--

-- ###########################################################################
-- Predicate
-- ###########################################################################
local BaseRule = require('ironbee/waggle/base_rule')

local Predicate = {}
Predicate.__index = Predicate
Predicate.type = "predicate"
setmetatable(Predicate, BaseRule)

-- Create a new external rule. The operator of this should be
-- similar to "lua:/path/to/script". Notice no @.
Predicate.new = function(self, id, rev, db)
    local si = BaseRule:new(id, rev, db)
    si.data.rule_type = 'Predicate'

    -- Tailor functionality to predicate
    si.field = nil
    si.fields = nil
    si.op = nil
    si.expr = si.predicate
    si.predicate = nil

    return setmetatable(si, self)
end

-- Report if this rule type is a stream rule or not.
-- TODO: Change once/if predicate supports streaming
Predicate.is_streaming = function(self)
    return false
end

return Predicate
