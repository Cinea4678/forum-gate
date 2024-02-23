//
// Created by cinea on 24-2-22.
//

#include "ProxyPass.h"

#include <boost/asio/strand.hpp>
#include <utility>

#include "Common.h"

class proxy_session : public std::enable_shared_from_this<proxy_session>
{
    tcp::resolver resolver_;
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    http::request<http::dynamic_body> req_;
    http::response<http::dynamic_body> res_;
    ProxyCallbackFunc callback_func_;

public:
    proxy_session(net::io_context& ioc, http::request<http::dynamic_body>&& req, ProxyCallbackFunc&& callback_func):
        resolver_(make_strand(ioc)),
        stream_(make_strand(ioc)),
        req_(std::move(req)),
        callback_func_(std::move(callback_func))
    {
    }

    void run(const std::string_view& host, const std::string_view& port, const std::string_view& target,
             const int version)
    {
        req_.version(version);
        req_.target(target);
        req_.set(http::field::host, host);
        req_.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        resolver_.async_resolve(
            host,
            port,
            beast::bind_front_handler(
                &proxy_session::on_resolve, shared_from_this()));
    }

    void on_resolve(const beast::error_code& ec, const tcp::resolver::results_type& results)
    {
        if (ec)
            return fail(ec, "resolve");

        stream_.expires_after(std::chrono::seconds(30));

        stream_.async_connect(results, beast::bind_front_handler(&proxy_session::on_connect, shared_from_this()));
    }

    void on_connect(const beast::error_code& ec, tcp::resolver::results_type::endpoint_type)
    {
        if (ec)
            return fail(ec, "connect");

        stream_.expires_after(std::chrono::seconds(30));

        http::async_write(stream_, req_, beast::bind_front_handler(&proxy_session::on_write, shared_from_this()));
    }

    void on_write(const beast::error_code& ec, std::size_t)
    {
        if (ec)
            return fail(ec, "write");

        http::async_read(stream_, buffer_, res_,
                         beast::bind_front_handler(&proxy_session::on_read, shared_from_this()));
    }

    void on_read(const beast::error_code& ec, std::size_t)
    {
        if (ec)
            return fail(ec, "read");

        // 调用回调函数
        callback_func_(std::move(res_));

        do_close();
    }

    void do_close()
    {
        beast::error_code ec;

        boost::ignore_unused(
            stream_.socket().shutdown(tcp::socket::shutdown_both, ec)
        );

        if (ec && ec != beast::errc::not_connected)
            return fail(ec, "shutdown");
    }
};

void proxy_pass::handle(net::io_context& ioc,
                        http::request<http::dynamic_body>&& req,
                        ProxyCallbackFunc&& handler
) const
{
    const auto old_target = req.target().substr(prefix_.length());
    const auto new_target = std::string(url_.encoded_path()) + std::string(old_target);

    std::make_shared<proxy_session>(ioc, std::move(req), std::move(handler))->run(
        url_.host(), url_.port(), new_target, 11);
}




