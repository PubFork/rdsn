/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Microsoft Corporation
 *
 * -=- Robust Distributed System Nucleus (rDSN) -=-
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <dist/replication/common/replication_common.h>
#include <fmt/format.h>
#include <dsn/utility/string_conv.h>
#include <dsn/dist/fmt_logging.h>
#include <dsn/dist/replication/replica_envs.h>
#include "app_env_validator.h"

namespace dsn {
namespace replication {

bool validate_app_env(const std::string &env_name,
                      const std::string &env_value,
                      std::string &hint_message)
{
    return app_env_validator::instance().validate_app_env(env_name, env_value, hint_message);
}

bool check_slow_query(const std::string &env_value, std::string &hint_message)
{
    uint64_t threshold = 0;
    if (!dsn::buf2uint64(env_value, threshold) ||
        threshold < replica_envs::MIN_SLOW_QUERY_THRESHOLD_MS) {
        hint_message = fmt::format("Slow query threshold must be >= {}ms",
                                   replica_envs::MIN_SLOW_QUERY_THRESHOLD_MS);
        return false;
    }
    return true;
}

bool check_rocksdb_iteration(const std::string &env_value, std::string &hint_message)
{
    uint64_t threshold = 0;
    if (!dsn::buf2uint64(env_value, threshold) || threshold < 0) {
        hint_message = "Rocksdb iteration threshold must be greater than zero";
        return false;
    }
    return true;
}

bool check_write_throttling(const std::string &env_value, std::string &hint_message)
{
    std::vector<std::string> sargs;
    utils::split_args(env_value.c_str(), sargs, ',');
    if (sargs.empty()) {
        hint_message = "The value shouldn't be empty";
        return false;
    }

    // example for sarg: 100K*delay*100 / 100M*reject*100
    bool reject_parsed = false;
    bool delay_parsed = false;
    for (std::string &sarg : sargs) {
        std::vector<std::string> sub_sargs;
        utils::split_args(sarg.c_str(), sub_sargs, '*', true);
        if (sub_sargs.size() != 3) {
            hint_message = fmt::format("The field count of {} should be 3", sarg);
            return false;
        }

        // check the first part, which is must be a positive number followed with 'K' or 'M'
        int64_t units = 0;
        if (!sub_sargs[0].empty() &&
            ('M' == *sub_sargs[0].rbegin() || 'K' == *sub_sargs[0].rbegin())) {
            sub_sargs[0].pop_back();
        }
        if (!buf2int64(sub_sargs[0], units) || units < 0) {
            hint_message = fmt::format("{} should be non-negative int", sub_sargs[0]);
            return false;
        }

        // check the second part, which is must be "delay" or "reject"
        if (sub_sargs[1] == "delay") {
            if (delay_parsed) {
                hint_message = "duplicate delay config";
                return false;
            }
            delay_parsed = true;
        } else if (sub_sargs[1] == "reject") {
            if (reject_parsed) {
                hint_message = "duplicate reject config";
                return false;
            }
            reject_parsed = true;
        } else {
            hint_message = fmt::format("{} should be \"delay\" or \"reject\"", sub_sargs[1]);
            return false;
        }

        // check the third part, which is must be a positive number or 0
        int64_t delay_ms = 0;
        if (!buf2int64(sub_sargs[2], delay_ms) || delay_ms < 0) {
            hint_message = fmt::format("{} should be non-negative int", sub_sargs[2]);
            return false;
        }
    }

    return true;
}

bool app_env_validator::validate_app_env(const std::string &env_name,
                                         const std::string &env_value,
                                         std::string &hint_message)
{
    auto func_iter = _validator_funcs.find(env_name);
    if (func_iter != _validator_funcs.end()) {
        // check function == nullptr means no check
        if (nullptr != func_iter->second && !func_iter->second(env_value, hint_message)) {
            dwarn_f("{}={} is invalid.", env_name, env_value);
            return false;
        }

        return true;
    }

    hint_message = fmt::format("app_env \"{}\" is not supported", env_name);
    return false;
}

void app_env_validator::register_all_validators()
{
    _validator_funcs = {
        {replica_envs::SLOW_QUERY_THRESHOLD,
         std::bind(&check_slow_query, std::placeholders::_1, std::placeholders::_2)},
        {replica_envs::WRITE_QPS_THROTTLING,
         std::bind(&check_write_throttling, std::placeholders::_1, std::placeholders::_2)},
        {replica_envs::WRITE_SIZE_THROTTLING,
         std::bind(&check_write_throttling, std::placeholders::_1, std::placeholders::_2)},
        {replica_envs::ROCKSDB_ITERATION_THRESHOLD_TIME_MS,
         std::bind(&check_rocksdb_iteration, std::placeholders::_1, std::placeholders::_2)},
        // TODO(zhaoliwei): not implemented
        {replica_envs::BUSINESS_INFO, nullptr},
        {replica_envs::DENY_CLIENT_WRITE, nullptr},
        {replica_envs::TABLE_LEVEL_DEFAULT_TTL, nullptr},
        {replica_envs::ROCKSDB_USAGE_SCENARIO, nullptr},
        {replica_envs::ROCKSDB_CHECKPOINT_RESERVE_MIN_COUNT, nullptr},
        {replica_envs::ROCKSDB_CHECKPOINT_RESERVE_TIME_SECONDS, nullptr},
        {replica_envs::MANUAL_COMPACT_DISABLED, nullptr},
        {replica_envs::MANUAL_COMPACT_MAX_CONCURRENT_RUNNING_COUNT, nullptr},
        {replica_envs::MANUAL_COMPACT_ONCE_TRIGGER_TIME, nullptr},
        {replica_envs::MANUAL_COMPACT_ONCE_TARGET_LEVEL, nullptr},
        {replica_envs::MANUAL_COMPACT_ONCE_BOTTOMMOST_LEVEL_COMPACTION, nullptr},
        {replica_envs::MANUAL_COMPACT_PERIODIC_TRIGGER_TIME, nullptr},
        {replica_envs::MANUAL_COMPACT_PERIODIC_TARGET_LEVEL, nullptr},
        {replica_envs::MANUAL_COMPACT_PERIODIC_BOTTOMMOST_LEVEL_COMPACTION, nullptr}};
}

} // namespace replication
} // namespace dsn
