//
// Created by cinea on 24-2-22.
//

#ifndef PROXYPASS_H
#define PROXYPASS_H

#include <filesystem>
#include <optional>
#include <string>
#include <boost/beast/http/message_generator.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <boost/url.hpp>
#include <utility>

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http; // from <boost/beast/http.hpp>
namespace net = boost::asio; // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>


typedef std::function<http::message_generator(http::message_generator&&)> ErrorHandlerFunc;
typedef std::function<void(http::message_generator&&)> ProxyCallbackFunc;

class proxy_pass : public std::enable_shared_from_this<proxy_pass>
{
    std::string prefix_;
    boost::url_view url_;
    std::optional<std::string> expires_;
    bool need_real_ip_{false};
    std::optional<
        ErrorHandlerFunc
    > error_handler_;

public:
    proxy_pass(std::string prefix, const boost::url_view& url): prefix_(std::move(prefix)), url_(url)
    {
    }

    proxy_pass(std::string prefix, const boost::url_view& url, std::string expires): prefix_(std::move(prefix)),
        url_(url), expires_(std::optional(std::move(expires)))
    {
    }

    proxy_pass(std::string prefix, const boost::url_view& url, std::string expires, const bool need_real_ip):
        prefix_(std::move(prefix)), url_(url),
        expires_(std::optional(std::move(expires))),
        need_real_ip_(need_real_ip)
    {
    }

    proxy_pass(std::string prefix, const boost::url_view& url, std::string expires, const bool need_real_ip,
               ErrorHandlerFunc error_hander):
        prefix_(std::move(prefix)), url_(url),
        expires_(std::optional(std::move(expires))),
        need_real_ip_(need_real_ip),
        error_handler_(std::optional(std::move(error_hander)))
    {
    }

    /**
     * \brief Match if the target matches this proxy.
     */
    [[nodiscard]] bool match(const beast::string_view& target) const{
        const auto target_str = std::string_view(target);
        return target_str.substr(0, prefix_.length()) == prefix_;
    }

    void handle(
        net::io_context& ioc,
        http::request<http::dynamic_body>&& req,
        ProxyCallbackFunc&& handler) const;
};

#endif
