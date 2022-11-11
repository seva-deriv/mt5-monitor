#pragma once

#include "MT5Common.hpp"
#include "MT5Logger.hpp"
#include "Logging.hpp"
#include "Calendar.hpp"
#include "statsd_client.h"

#include <boost/nowide/convert.hpp>

#include <chrono>

namespace {

class MT5Monitor : public IMTTickSink, public IMTManagerSink {
public:
    using PriceValue = double;
    using Clock = std::chrono::system_clock;
    using Duration = std::chrono::milliseconds;
    using TimeStamp = std::chrono::time_point<Clock, Duration>;

    static constexpr PriceValue max_bid_ask_ratio   = 1;
    static constexpr PriceValue min_bid_ask_ratio   = 0.25;
    static constexpr auto accepted_delay = std::chrono::seconds(1);

    MT5Monitor(Calendar calendar, std::promise<void> disconnected)
        : calendar_(std::move(calendar)), disconnected_(std::move(disconnected))
    {}

    void OnDisconnect() override {
        with_exceptions_to_log([this]{
            LOG_WARN("... disconnected!");
            disconnected_.set_value();
        });
    }

    void OnTick(LPCWSTR wsymbol, const MTTickShort &tick) override {
        with_exceptions_to_log([&]{
            auto mt5_symbol = boost::nowide::narrow(wsymbol);
            put_relay_metrics(mt5_symbol, tick);
            const auto validation_result = validate_tick(wsymbol, tick);
            if (!validation_result.empty()) {
                LOG_WARN("Validation failed for " << mt5_symbol << ": " << validation_result);
                statsd_.inc("feed.listener.status_fail", 1.0f,
                    {"symbol:" + mt5_symbol, "reason:" + validation_result});
            }
        });
    }

    static std::pair<TimeStamp, TimeStamp> get_server_and_client_timestamps(const MTTickShort &tick) {
        const auto server_timestamp = tick.datetime_msc ?
            TimeStamp(std::chrono::milliseconds(tick.datetime_msc)) :
            TimeStamp(std::chrono::seconds(tick.datetime));

        const auto client_timestamp = std::chrono::time_point_cast<Duration>(std::chrono::system_clock::now());
        return {server_timestamp, client_timestamp};
    }

    void put_relay_metrics(const std::string &mt5_symbol, const MTTickShort &tick) {
        static constexpr auto STATSD_RATE = 0.05F;
        const auto [server_timestamp, client_timestamp] = get_server_and_client_timestamps(tick);
        const auto latency = std::chrono::duration_cast<Duration>(client_timestamp - server_timestamp).count();
        statsd_.inc("feed.mt5_relay.quote.count", STATSD_RATE, {mt5_symbol});
        statsd_.timing("feed.mt5_relay.quote.latency", static_cast<std::size_t>(latency), STATSD_RATE, {mt5_symbol});
        statsd_.gauge("feed.mt5_relay.quote.ask", static_cast<std::size_t>(tick.ask * 1000000), STATSD_RATE, {mt5_symbol});
    }

    std::string validate_tick(LPCWSTR wsymbol, const MTTickShort &tick) {
        const auto [server_timestamp, client_timestamp] = get_server_and_client_timestamps(tick);
        const PriceValue ask = tick.ask;
        const PriceValue bid = tick.bid;

        if (ask > 0 && (bid / ask < min_bid_ask_ratio || bid / ask > max_bid_ask_ratio)) {
            return "bad-bid-ask";
        }

        if (server_timestamp > client_timestamp) {
            return "quote-from-future";
        }

        if (server_timestamp < client_timestamp - accepted_delay) {
            return "quote-from-past";
        }

        if (!is_market_open_at(wsymbol, server_timestamp)) {
            return "market-is-closed";
        }

        return {};
    }

    bool is_market_open_at(LPCWSTR wsymbol, TimeStamp when) {
        const auto day = std::chrono::floor<std::chrono::days>(when);
        const auto minutes = std::chrono::floor<std::chrono::minutes>(when - day);
        const auto weekday = std::chrono::weekday(day).c_encoding();
        const auto& subcalendar = calendar_[wsymbol][weekday];
        const auto pos = std::upper_bound(subcalendar.begin(), subcalendar.end(), minutes) - subcalendar.begin();
        return !(pos & 1);
    }

private:
    statsd::StatsdClient statsd_;
    // wsymbol -> weekday in C encoding -> trading intervals starting from Closed
    Calendar calendar_;
    std::promise<void> disconnected_;

public:
    static void run_once(const std::wstring& server, uint64_t login, const std::wstring& password)
    {
        static constexpr auto MT5_CONNECT_TIMEOUT = 30000;

        // Create and init factory
        auto factory = CHECKED(new CMTManagerAPIFactory());
        CHECKED(factory->Initialize());

        // Get, report and check version
        UINT version = 0;
        CHECKED(factory->Version(version));

        LOG_INFO("Manager API version " << version << " has been loaded");
        if (version != MTManagerAPIVersion)
            THROW_ERROR("Manager API version " << MTManagerAPIVersion << " required (got " << version << ")");

        // Create API
        auto api = [&factory] {
            IMTManagerAPI *manager = nullptr;
            CHECKED(factory->CreateManager(MTManagerAPIVersion, &manager));
            return checked("factory.CreateManager", manager); // Ensure that returned value is not null
        }();

        // connect to remote server
        LOG_INFO("Connecting to remote server " << server << "...");
        checked("connect to '" + boost::nowide::narrow(server) + "'",
            api->Connect(server.c_str(), login, password.c_str(), nullptr,
                        IMTManagerAPI::PUMP_MODE_SYMBOLS, MT5_CONNECT_TIMEOUT));
        LOG_INFO("... connected");

        // now we can log to remote server too
        auto logger = boost::make_shared<MT5Logger<IMTManagerAPI>>(*api);

        // Select all symbols
        CHECKED(api->SelectedAddAll());
        // Get calendar for selected symbols
        auto calendar = get_calendar(*api);

        std::promise<void> disconnected_promise;
        auto disconnected_future = disconnected_promise.get_future();
        // Subscribe for receiving ticks for selected symbols
        auto sink = std::make_unique<MT5Monitor>(std::move(calendar), std::move(disconnected_promise));
        CHECKED(api->Subscribe(sink.get()));
        CHECKED(api->TickSubscribe(sink.get()));
        LOG_INFO("Initialization complete, running...");

        disconnected_future.wait();
        LOG_INFO("Finished run once");
    }
};

}
