// ================================================================
//     ______                 __________  ______
//    / ____/___ ___  __  __/ ____/ __ \/ ____/
//   / __/ / __ `__ \/ / / / /   / /_/ / /
//  / /___/ / / / / / /_/ / /___/ ____/ /___
// /_____/_/ /_/ /_/\__,_/\____/_/    \____/
//
// EmuCPC — Amstrad CPC, CPC Plus and GX4000, down to the 1/16 µs
// ================================================================
// Author: Maxime MARCONATO (MaaaX^^, aka EPOCH84)
// ================================================================
// License: GNU General Public License v3.0 or later — see LICENSE.
// SPDX-License-Identifier: GPL-3.0-or-later
// ================================================================

// Checks of the build chain itself: the third-party copies compile under our
// warnings, threads work (MinGW -posix model, static winpthreads), and the
// version header is generated.
// SPDX-License-Identifier: GPL-3.0-or-later

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <atomic>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include <emucpc/version.hpp>

TEST_CASE("version header is generated")
{
    CHECK_FALSE(std::string_view{emucpc::version}.empty());
}

TEST_CASE("ordered_json keeps insertion order and round-trips")
{
    nlohmann::ordered_json j;
    j["zeta"] = 1;
    j["alpha"] = "two";
    j["mid"] = {3, 4};
    const std::string text = j.dump();
    CHECK(text == R"({"zeta":1,"alpha":"two","mid":[3,4]})");
    CHECK(nlohmann::ordered_json::parse(text) == j);
}

TEST_CASE("threads and mutexes")
{
    constexpr int thread_count = 4;
    constexpr int increments = 10000;
    std::mutex mutex;
    long guarded = 0;
    std::atomic<long> atomic_count{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < thread_count; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < increments; ++i) {
                const std::lock_guard lock(mutex);
                ++guarded;
                atomic_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    CHECK(guarded == long{thread_count} * increments);
    CHECK(atomic_count.load() == long{thread_count} * increments);
}
