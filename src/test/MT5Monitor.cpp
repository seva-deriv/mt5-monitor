#include <catch2/catch_test_macros.hpp>
#include "../MT5Monitor.hpp"
#include <chrono>

using namespace std::chrono;

TEST_CASE("validate_tick", "[MT5Monitor]")
{
    const auto now = system_clock::now();
    std::wstring wsymbol = L"EURUSD";
    MTTickShort tick {
        .datetime=time_point_cast<seconds>(now).time_since_epoch().count(),
        .bid=1.0,
        .ask=1.0,
        .last=1.0,
        .volume=1,
        .datetime_msc=time_point_cast<milliseconds>(now).time_since_epoch().count(),
        .flags=MTTickShort::TICK_SHORT_FLAG_RAW,
        .volume_ext=1,
    };

    // Create fake "always open" calendar
    Calendar calendar;
    for (auto& subcalendar : calendar[wsymbol]) {
        subcalendar.push_back(minutes(0)); // Sentinel
        subcalendar.push_back(minutes(0)); // Open time
        subcalendar.push_back(minutes(24 * 60)); // Close time
    }

    // Fake promise, it is required by MT5Monitor ctor.
    std::promise<void> disconnected;

    SECTION("positive main") {
        MT5Monitor monitor{std::move(calendar), std::move(disconnected)};
        CHECK(monitor.validate_tick(wsymbol.c_str(), tick) == "");
    }

    SECTION("bad-bid-ask 1") {
        tick.bid = 0.1;
        MT5Monitor monitor{std::move(calendar), std::move(disconnected)};
        CHECK(monitor.validate_tick(wsymbol.c_str(), tick) == "bad-bid-ask");
    }

    SECTION("bad-bid-ask 2") {
        tick.bid = 10;
        MT5Monitor monitor{std::move(calendar), std::move(disconnected)};
        CHECK(monitor.validate_tick(wsymbol.c_str(), tick) == "bad-bid-ask");
    }

    // Is not going to test ask == 0.0 as current behaior is wierd (positive scenario).
    // Maybe additional checks should be added?

    SECTION("quote-from-past") {
        tick.datetime_msc = time_point_cast<milliseconds>(system_clock::now() - seconds(2)).time_since_epoch().count();
        MT5Monitor monitor{std::move(calendar), std::move(disconnected)};
        CHECK(monitor.validate_tick(wsymbol.c_str(), tick) == "quote-from-past");
    }

    SECTION("quote-from-future") {
        tick.datetime_msc = time_point_cast<milliseconds>(system_clock::now() + seconds(2)).time_since_epoch().count();
        MT5Monitor monitor{std::move(calendar), std::move(disconnected)};
        CHECK(monitor.validate_tick(wsymbol.c_str(), tick) == "quote-from-future");
    }

    SECTION("market-is-closed simple") {
        auto cur_wd = weekday(time_point_cast<days>(system_clock::now())).c_encoding();
        auto& subcalendar = calendar[wsymbol][cur_wd];
        subcalendar.clear();
        subcalendar.push_back(minutes(0)); // Put sentinel only. "Closed for the whole day" semantics
        MT5Monitor monitor{std::move(calendar), std::move(disconnected)};
        CHECK(monitor.validate_tick(wsymbol.c_str(), tick) == "market-is-closed");
    }

    SECTION("market-is-closed many-empty-periods") {
        auto cur_wd = weekday(time_point_cast<days>(system_clock::now())).c_encoding();
        auto& subcalendar = calendar[wsymbol][cur_wd];
        subcalendar.clear();
        subcalendar.push_back(minutes(0)); // Sentinel
        for (auto i = 0; i < 24 * 60; ++i) {
            // We open and immediately close
            subcalendar.push_back(minutes(i));
            subcalendar.push_back(minutes(i));
        }
        MT5Monitor monitor{std::move(calendar), std::move(disconnected)};
        CHECK(monitor.validate_tick(wsymbol.c_str(), tick) == "market-is-closed");
    }

    SECTION("positive many-1min-periods") {
        auto cur_wd = weekday(time_point_cast<days>(system_clock::now())).c_encoding();
        auto& subcalendar = calendar[wsymbol][cur_wd];
        subcalendar.clear();
        subcalendar.push_back(minutes(0)); // Sentinel
        for (auto i = 0; i < 24 * 60; ++i) {
            // We open for 1 min every 1 min
            subcalendar.push_back(minutes(i));
            subcalendar.push_back(minutes(i + 1));
        }
        MT5Monitor monitor{std::move(calendar), std::move(disconnected)};
        CHECK(monitor.validate_tick(wsymbol.c_str(), tick) == "");
    }
}
