#include "Logging.hpp"

#include <boost/shared_ptr.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/basic_sink_backend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sinks/syslog_backend.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/expressions/formatters/wrap_formatter.hpp>

#ifdef _WIN32
#include <boost/log/sinks/event_log_backend.hpp>
#endif

template <typename Expr> static void init_builtin_syslog(boost::phoenix::actor<Expr> &formatter);

template <typename Expr> static void init_eventlog(boost::phoenix::actor<Expr> &formatter);

void init_logging() {
    using backend_t = sinks::text_ostream_backend;
    using sink_t = sinks::synchronous_sink<backend_t>;

    logging::add_common_attributes();

    /* define formatter */
    auto formatter =
        expr::stream << expr::attr<boost::posix_time::ptime>(logging::aux::default_attribute_names::timestamp()) << " "
                     << "[" << expr::attr<severity_level>(logging::aux::default_attribute_names::severity()) << "] "
                     << "[Thr "
                     << expr::attr<attrs::current_thread_id::value_type>(
                            logging::aux::default_attribute_names::thread_id())
                     << "] "
                     << "[ " << std::right << std::setw(20) << expr::attr<std::string>("File") << ":" << std::left
                     << std::setw(4) << expr::attr<uint32_t>("Line") << " ] " << expr::smessage;

    /* stdout sink*/
    auto backend = boost::make_shared<backend_t>();
    backend->add_stream(boost::shared_ptr<std::ostream>(&std::clog, [](void *) {}));

    // Enable auto-flushing after each log record written
    backend->auto_flush(true);

    // Wrap it into the frontend and register in the core.
    // The backend requires synchronization in the frontend.
    auto sink = boost::make_shared<sink_t>(backend);
    sink->set_formatter(formatter);
    logging::core::get()->add_sink(sink);

    init_builtin_syslog(formatter);
    init_eventlog(formatter);
}

template <typename Expr> void init_builtin_syslog(boost::phoenix::actor<Expr> &formatter) {
    using backend_t = sinks::syslog_backend;
    using sink_t = sinks::synchronous_sink<backend_t>;

    // Create a new backend
    auto backend = boost::make_shared<backend_t>(keywords::facility = sinks::syslog::local0,
                                                 keywords::use_impl = sinks::syslog::udp_socket_based);

    // Setup the target address and port to send syslog messages to.
    // Defaults to localhost:514, whic is fine
    // backend->set_target_address("192.164.1.10", 514);

    // Create and fill in another level translator for "MyLevel" attribute of type string
    sinks::syslog::custom_severity_mapping<severity_level> mapping("Severity");
    mapping[info] = sinks::syslog::info;
    mapping[warning] = sinks::syslog::warning;
    mapping[error] = sinks::syslog::error;
    backend->set_severity_mapper(mapping);

    // Wrap it into the frontend and register in the core.
    auto sink = boost::make_shared<sink_t>(backend);
    sink->set_formatter(formatter);
    logging::core::get()->add_sink(sink);
}

template <typename Expr> static void init_eventlog(boost::phoenix::actor<Expr> &formatter) {
#if WIN32
    try {
        using backend_t = sinks::simple_event_log_backend;
        using sink_t = sinks::synchronous_sink<backend_t>;

        /* define mapping */
        sinks::event_log::custom_event_type_mapping<severity_level> mapping{"Severity"};
        mapping[info] = sinks::event_log::info;
        mapping[warning] = sinks::event_log::warning;
        mapping[error] = sinks::event_log::error;

        /* windows simple event sink*/
        auto sink = boost::make_shared<sink_t>();
        sink->set_formatter(formatter);
        sink->locked_backend()->set_event_type_mapper(mapping);
        logging::core::get()->add_sink(sink);
    } catch (const std::exception &ex) {
        LOG_ERROR("Event log init failed: " << ex.what());
    }
#endif
}
