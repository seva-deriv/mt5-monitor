#pragma once

#include "Logging.hpp"

#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/basic_sink_backend.hpp>
#include <boost/log/sinks/event_log_backend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

template <typename API>
class MT5Logger : public sinks::basic_formatted_sink_backend<wchar_t, sinks::synchronized_feeding>,
                  public boost::enable_shared_from_this<MT5Logger<API>> {
    typedef sinks::synchronous_sink<MT5Logger> sink_t;
    typedef MT5Logger<API> mt5_logger;

  public:
    API &api_;
    boost::shared_ptr<sink_t> _sink;
    MT5Logger(API &api) : api_(api) {}

    void add_sink() {
        _sink = boost::make_shared<sink_t>(this->shared_from_this());
        logging::core::get()->add_sink(_sink);
    }

    ~MT5Logger() { logging::core::get()->remove_sink(_sink); }

    void consume(logging::record_view const &rec, string_type const &message) {
        logging::attribute_value_set const &values = rec.attribute_values();
        logging::attribute_value_set::const_iterator it = values.find("Severity");
        if (it != values.end()) {
            logging::attribute_value const &value = it->second;

            // A single attribute value can also be visited or extracted
            auto s = value.extract<severity_level>();
            int severity_mt5 = MTLogOK;
            if (s == severity_level::info) {
                severity_mt5 = MTLogOK;
            } else if (s == severity_level::warning) {
                severity_mt5 = MTLogWarn;
            } else if (s == severity_level::error) {
                severity_mt5 = MTLogErr;
            }
            api_.LoggerOut(severity_mt5, message.c_str());
        }
    }
};