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

#endif //BOOSTCOMMON_H
