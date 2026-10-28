/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 The Contributors to Eclipse OpenSOVD (see CONTRIBUTORS)
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 */

#include "fault_ffi.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<fault::Context>);
static_assert(!std::is_copy_constructible_v<fault::Reporter>);
static_assert(std::is_nothrow_move_constructible_v<fault::Context>);
static_assert(std::is_nothrow_move_assignable_v<fault::Reporter>);
static_assert(std::is_nothrow_destructible_v<fault::Reporter>);
static_assert(std::is_standard_layout_v<fl_string> && std::is_trivially_copyable_v<fl_string>);
static_assert(std::is_standard_layout_v<fl_metadata> && std::is_trivially_copyable_v<fl_metadata>);
static_assert(std::is_standard_layout_v<fl_reporter_config> &&
              std::is_trivially_copyable_v<fl_reporter_config>);

int main(int argc, char **argv) {
    if (fl_abi_version() != FL_ABI_VERSION) {
        return 1;
    }
    fault::Context context;
    if (fault::Context::create("not JSON", context) != FL_CATALOG_ERROR ||
        fault::last_error().empty()) {
        return 1;
    }
    fault::Context moved(std::move(context));
    context = std::move(moved);
    fault::Reporter reporter;
    auto config = fault::configuration("test");
    if (fault::Reporter::numeric(context, 42, config, reporter) != FL_INVALID_ARGUMENT) {
        return 1;
    }
    fault::Reporter moved_reporter(std::move(reporter));
    reporter = std::move(moved_reporter);
    if (reporter.publish("test", fault::Stage::Failed) != FL_INVALID_ARGUMENT) {
        return 1;
    }
    const auto parent_error = fault::last_error();
    bool thread_error_ok = false;
    std::thread error_thread([&thread_error_ok] {
        fault::Context invalid;
        thread_error_ok = fault::Context::create("not JSON", invalid) == FL_CATALOG_ERROR &&
                          fault::last_error().find("JSON") != std::string::npos;
    });
    error_thread.join();
    if (!thread_error_ok || fault::last_error() != parent_error) {
        return 1;
    }
    reporter.reset();
    context.reset();
    if (argc == 2) {
        std::ifstream file(argv[1]);
        const std::string json((std::istreambuf_iterator<char>(file)), {});
        if (fault::Context::create(json, context) != FL_OK) {
            std::cerr << fault::last_error() << '\n';
            return 1;
        }
        moved = std::move(context);
        std::string catalog_id;
        if (moved.catalog_id(catalog_id) != FL_OK || catalog_id != "hvac") {
            return 1;
        }
        if (fault::Reporter::text(moved, "hvac.blower.speed_sensor_mismatch", config, reporter) != FL_OK) {
            return 1;
        }
        fault::Reporter second;
        if (fault::Reporter::numeric(moved, 0x7001, config, second) != FL_OK) {
            return 1;
        }
        // A failed factory call must preserve a live output and its diagnostic.
        if (fault::Reporter::numeric(moved, UINT32_MAX, config, reporter) != FL_UNKNOWN_FAULT ||
            fault::last_error().empty()) {
            return 1;
        }
        std::array<uint8_t, 16> uuid;
        uuid.fill(7);
        fault::Reporter uuid_reporter;
        if (fault::Reporter::uuid(moved, uuid, config, uuid_reporter) != FL_OK) {
            return 1;
        }
        // Move assignment must release a live destination, not leak or double-free it.
        second = std::move(reporter);
        moved_reporter = std::move(second);
        moved.reset();
        fl_status uuid_status = FL_INTERNAL_ERROR;
        // Transfer exclusive ownership to a foreign thread; other reporters remain usable.
        std::thread worker([owned = std::move(uuid_reporter), &uuid_status]() mutable {
            uuid_status = owned.publish("hvac", fault::Stage::Failed);
        });
        const auto text_status = moved_reporter.publish(
            "hvac", fault::Stage::Failed, {fault::metadata("application", "cpp_consumer")});
        worker.join();
        if (uuid_status != FL_OK || text_status != FL_OK) {
            return 1;
        }
        moved_reporter.reset();
        if (fault::Context::create(json, context) != FL_ALREADY_INITIALIZED) {
            return 1;
        }
    }
    return fl_abi_version() == FL_ABI_VERSION ? 0 : 1;
}
