#pragma once

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//  Warning! Always include this file instead of MT5APIManager.h
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

// asio must be included prior to Windows.h
// Otherwise compilation will fail
#include <boost/asio.hpp>

// MT5APIManager.h uses types defined in Windows.h, but doesn't include it
#include <Windows.h>

#include <MT5APIManager.h>
#include "Logging.hpp"

namespace {

class MT5Deleter {
  public:
    template <typename TInstance> void operator()(TInstance *instance) {
        if constexpr (std::is_same<TInstance, CMTManagerAPIFactory>::value) {
            instance->Shutdown();
        } else {
            instance->Release();
        }
    }
};

template<typename TString>
void checked(TString&& name, MTAPIRES result) {
    if (result != MT_RET_OK) {
        THROW_ERROR(std::forward<TString>(name) << " failed (" << result << ")");
    }
}

template<typename T, typename TString>
std::unique_ptr<T, MT5Deleter> checked(TString&& name, T* result) {
    if (!result) {
        THROW_ERROR(std::forward<TString>(name) << " failed. OOM?");
    }
    return std::unique_ptr<T, MT5Deleter>(result);
}

#define CHECKED(...) [&]{ return checked(#__VA_ARGS__, __VA_ARGS__); }()

}