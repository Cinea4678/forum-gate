//
// Created by cinea on 24-2-22.
//

#ifndef BOOSTCOMMON_H
#define BOOSTCOMMON_H

#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http; // from <boost/beast/http.hpp>
namespace net = boost::asio; // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

// Report a failure
inline void
fail(const beast::error_code& ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << std::endl;
}

inline time_t fs_time_to_time_t(const std::chrono::time_point<std::filesystem::__file_clock>& fs_time)
{
    // 将file_time_type转换为系统时间点
    const auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(fs_time - std::filesystem::__file_clock::now() + std::chrono::system_clock::now());

    // 将系统时间点转换为time_t
    return std::chrono::system_clock::to_time_t(sctp);
}

#endif //BOOSTCOMMON_H
