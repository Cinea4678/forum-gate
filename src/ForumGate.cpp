#include <memory>
#include <filesystem>
#include <iostream>
#include <toml.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <utility>

#include "StaticFileHandler.h"
#include "ProxyPass.h"

using namespace std::string_literals;

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http; // from <boost/beast/http.hpp>
namespace net = boost::asio; // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

constexpr size_t MAX_ALIVE_CONN = 1024; // 最多保留多少个Keep-Alive连接

std::atomic_size_t alive_conns;

// 代理列表，后期可以考虑做成配置文件的形式，也很简单
const proxy_pass proxy_passes[]
{
    proxy_pass("/s3"s, boost::urls::parse_uri("http://10.80.43.196:9000").value()),
    proxy_pass("/api"s, boost::urls::parse_uri("http://10.80.43.196:9002").value()),
    // proxy_passes("/avatar"sv, boost::urls::parse_uri("http://10.80.43.196:9000/forum/user-avatar")),
    proxy_pass("/card"s, boost::urls::parse_uri("http://10.80.43.196:9000/forum/user-avatar").value(), "12h"s),
    proxy_pass("/meili"s, boost::urls::parse_uri("http://10.80.42.189:7700").value()),
};

class session : public std::enable_shared_from_this<session>
{
    net::io_context& ioc_;
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    std::shared_ptr<std::filesystem::path const> doc_root_;
    http::request<http::dynamic_body> req_;

    bool keepd_alive{false};

public:
    session(
        net::io_context& ioc,
        tcp::socket&& socket,
        std::shared_ptr<std::filesystem::path const> const& doc_root):
        ioc_(ioc),
        stream_(std::move(socket)),
        doc_root_(doc_root)
    {
    }

    // 开始异步操作
    void run()
    {
        dispatch(stream_.get_executor(), beast::bind_front_handler(
                     &session::do_read,
                     shared_from_this()
                 ));
    }

    void do_read()
    {
        req_ = {}; // 清空请求体

        stream_.expires_after(std::chrono::seconds(30));

        http::async_read(stream_, buffer_, req_,
                         beast::bind_front_handler(
                             &session::on_read,
                             shared_from_this()));
    }

    void on_read(const beast::error_code& ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec == http::error::end_of_stream)
            return do_close();

        if (ec)
            return fail(ec, "read");

        handle_request(*doc_root_, std::move(req_));
    }

    // 网关规则主要在这里写
    void handle_request(const std::filesystem::path& doc_root,
                        http::request<http::dynamic_body>&& req)
    {
        if (req.keep_alive() && !keepd_alive && alive_conns.load() > MAX_ALIVE_CONN)
            // 系统资源不足，不能继续维持长链接
                req.keep_alive(false);

        for (const auto& proxy_pass : proxy_passes)
        {
            if (proxy_pass.match(req.target()))
            {
                req.keep_alive(false); // 代理暂时不维持长链接
                auto callback_func = std::bind(&session::send_response, shared_from_this(), std::placeholders::_1);
                return proxy_pass.handle(ioc_, std::move(req), callback_func);
            }
        }

        // 默认情况
        send_response(handle_static_file(doc_root, std::move(req)));
    }

    void send_response(http::message_generator&& msg)
    {
        bool keep_alive = msg.keep_alive();

        // 写入响应
        beast::async_write(
            stream_, std::move(msg), beast::bind_front_handler(
                &session::on_write, shared_from_this(), keep_alive));
    }

    void on_write(
        const bool keep_alive,
        const beast::error_code& ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "write");

        if (!keep_alive)
            // 可以关闭连接了
            return do_close();

        if(!keepd_alive)
        {
            keepd_alive = true;
            alive_conns.fetch_add(1);
        }

        // 读其他的请求
        do_read();
    }

    void do_close()
    {
        beast::error_code ec;

        if (keepd_alive)
            alive_conns.fetch_sub(1);

        if (const auto errc = stream_.socket().shutdown(tcp::socket::shutdown_send, ec))
            return fail(errc, "close");

        if (ec)
            return fail(ec, "close");

        // 已经可以安全退出了
    }
};

//------------------------------------------------------------------------------

// 接收连接,启动sessions
class listener : public std::enable_shared_from_this<listener>
{
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    std::shared_ptr<std::filesystem::path> doc_root_;

public:
    listener(net::io_context& ioc, const tcp::endpoint& endpoint,
             std::shared_ptr<std::filesystem::path> const& doc_root):
        ioc_(ioc), acceptor_(make_strand(ioc)), doc_root_(doc_root)
    {
        beast::error_code ec;

        // 开启接收器
        boost::ignore_unused(acceptor_.open(endpoint.protocol(), ec));
        if (ec)
        {
            fail(ec, "open");
            return;
        }

        // 允许复用地址
        boost::ignore_unused(acceptor_.set_option(net::socket_base::reuse_address(true), ec));
        if (ec)
        {
            fail(ec, "set_option");
            return;
        }

        // 绑定服务器地址
        boost::ignore_unused(acceptor_.bind(endpoint, ec));
        if (ec)
        {
            fail(ec, "bind");
            return;
        }

        // 开始监听连接
        boost::ignore_unused(
            acceptor_.listen(net::socket_base::max_listen_connections, ec)
        );
        if (ec)
        {
            fail(ec, "listen");
            return;
        }
    }

    void run()
    {
        do_accept();
    }

private:
    void do_accept()
    {
        acceptor_.async_accept(
            make_strand(ioc_),
            beast::bind_front_handler(&listener::on_accept, shared_from_this())
        );
        // std::cerr << "Accepting connections..." << std::endl;
    }

    void on_accept(const beast::error_code& ec, tcp::socket socket)
    {
        if (ec)
        {
            fail(ec, "accept");
            // return do_accept();
            return;
        }

        std::make_shared<session>(
            ioc_, std::move(socket), doc_root_
        )->run();

        do_accept();
    }
};

//------------------------------------------------------------------------------

int main()
{
    if (!std::filesystem::exists("app_config.toml"))
    {
        std::cerr <<
            "'app_config.toml' not found. Please make sure it exists."
            << std::endl;
        return EXIT_FAILURE;
    }

    auto config_data = toml::parse("app_config.toml");

    auto const address_str = toml::find<std::string>(config_data, "address");
    auto const port = toml::find<unsigned short>(config_data, "port");
    auto const doc_root_str = toml::find<std::string>(config_data, "doc_root");
    auto const threads = toml::find<int>(config_data, "threads");

    auto const address = net::ip::make_address(address_str);
    auto const doc_root = std::make_shared<std::filesystem::path>(doc_root_str);

    net::io_context ioc{threads};

    std::make_shared<listener>(ioc, tcp::endpoint{address, port}, doc_root)->run();

    // 在线程上运行IO服务
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for (auto i = threads - 1; i > 0; --i)
        v.emplace_back([&ioc] { ioc.run(); });

    ioc.run();

    return EXIT_SUCCESS;
}

