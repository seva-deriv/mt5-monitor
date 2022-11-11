#pragma once

#include "MT5Common.hpp"

#include <array>
#include <chrono>
#include <string>
#include <unordered_map>

namespace {

// wsymbol -> weekday in C encoding -> trading intervals starting from Closed
using Calendar = std::unordered_map<std::wstring, std::array<std::vector<std::chrono::minutes>, 7>>;

Calendar get_calendar(IMTManagerAPI& api) {
    auto symbol = CHECKED(api.SymbolCreate());
    auto session = CHECKED(api.SymbolSessionCreate());

    auto symbol_count = api.SymbolTotal();
    Calendar result;
    result.reserve(symbol_count);
    for (auto symbol_num = symbol_count; symbol_num --> 0;) {
        CHECKED(api.SymbolNext(symbol_num, symbol.get()));
        auto& subcalendar_for_symbol = result[symbol->Symbol()];
        for (auto weekday = std::size(subcalendar_for_symbol); weekday --> 0;) {
            const auto session_quote_total = symbol->SessionQuoteTotal(weekday);
            auto& subcalendar = subcalendar_for_symbol[weekday];
            subcalendar.reserve(session_quote_total * 2 + 1);
            subcalendar.push_back(std::chrono::minutes(0));
            auto push_checked = [&subcalendar](auto value) {
                assert(subcalendar.back() <= value);
                subcalendar.push_back(value);
            };
            for (auto session_quote_num = 0; session_quote_num < session_quote_total; ++session_quote_num) {
                CHECKED(symbol->SessionQuoteNext(weekday, session_quote_num, session.get()));
                push_checked(std::chrono::minutes(session->Open()));
                push_checked(std::chrono::minutes(session->Close()));
            }
        }
    }

    return result;
}

}
