#include "MT5Common.hpp"
#include "MT5Monitor.hpp"
#include "Logging.hpp"

// #include <boost/date_time/posix_time/posix_time.hpp>
// #include <boost/make_shared.hpp>
#include <boost/program_options.hpp>

int wmain(int argc, wchar_t **argv) {
    init_logging();

    namespace po = boost::program_options;

    po::options_description description("Allowed options");

    // clang-format off
    description.add_options()
        ("help", "show this help message")
        ("login", po::value<uint64_t>(), "MT5 manager login")
        ("password", po::wvalue<std::wstring>(), "MT5 manager password")
        ("server", po::wvalue<std::wstring>(), "MT5 upstream server")
    ;
    // clang-format on

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, description), vm);
    po::notify(vm);

    bool show_help = vm.count("help") || !vm.count("login") || !vm.count("password") || !vm.count("server");

    if (show_help) {
        std::cout << description << "\n";
        return 1;
    }

    with_exceptions_to_log([&]{
        MT5Monitor::run_once(vm["server"].as<std::wstring>(), vm["login"].as<uint64_t>(), vm["password"].as<std::wstring>());
    });

    return 0;
}
