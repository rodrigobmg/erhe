#include "erhe/net/net_log.hpp"
#include "erhe/log/log.hpp"

namespace erhe::net
{

std::shared_ptr<spdlog::logger> log_net   ;
std::shared_ptr<spdlog::logger> log_socket;
std::shared_ptr<spdlog::logger> log_client;
std::shared_ptr<spdlog::logger> log_server;

void initialize_logging()
{
    log_net    = erhe::log::make_logger("erhe::net::net",    spdlog::level::info);
    log_socket = erhe::log::make_logger("erhe::net::socket", spdlog::level::info);
    log_client = erhe::log::make_logger("erhe::net::client", spdlog::level::info);
    log_server = erhe::log::make_logger("erhe::net::server", spdlog::level::info);
}

} // namespace erhe::net
