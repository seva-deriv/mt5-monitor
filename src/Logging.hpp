#pragma once

#include <sstream>
#include <boost/log/core.hpp>
#include <boost/log/common.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/expressions/keyword_fwd.hpp>
#include <boost/log/expressions/keyword.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/trivial.hpp>
#include <boost/filesystem.hpp>

namespace {
using namespace boost::log::trivial; // Provides severity_level

namespace attrs = boost::log::attributes;
namespace keywords = boost::log::keywords;
namespace logging = boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;
} // namespace

BOOST_LOG_INLINE_GLOBAL_LOGGER_DEFAULT(my_logger, src::logger_mt)
#define LOG(s, message)                                                                                                \
    do {                                                                                                               \
        src::severity_logger<severity_level> slg;                                                                      \
        logging::record rec = slg.open_record(keywords::severity = s);                                                 \
        if (rec) {                                                                                                     \
            rec.attribute_values().insert(                                                                             \
                "File", attrs::make_attribute_value(boost::filesystem::path(__FILE__).filename().string()));           \
            rec.attribute_values().insert("Line", attrs::make_attribute_value(uint32_t(__LINE__)));                    \
            logging::record_ostream strm(rec);                                                                         \
            strm << message;                                                                                           \
            strm.flush();                                                                                              \
            slg.push_record(boost::move(rec));                                                                         \
        }                                                                                                              \
    } while (false)

#define LOG_DEBUG(m) LOG(severity_level::debug, m)
#define LOG_INFO(m) LOG(severity_level::info, m)
#define LOG_WARN(m) LOG(severity_level::warning, m)
#define LOG_ERROR(m) LOG(severity_level::error, m)

#define THROW_ERROR(message) throw std::runtime_error((std::ostringstream{} << message).str())
#define LOG_OR_THROW_ERROR(nothrow_condition, message) \
    do { if(nothrow_condition) { LOG_ERROR(message); } else { THROW_ERROR(message); } } while(false)

void init_logging();

namespace {

template<typename TFn>
void with_exceptions_to_log(TFn&& fn) noexcept {
    try {
        try {
            fn();
        } catch (const std::exception &ex) {
            LOG_ERROR("Exception: " << ex.what());
        } catch (...) {
            LOG_ERROR("Exception: <unknown>");
        }
    } catch (...) {
        // Swallow
    }
}

}