#include <filesystem>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core/string_type.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/fields.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/message_generator.hpp>
#include <boost/beast/http/verb.hpp>

#include "BoostCommon.h"
#include "../libs/MimeTypes/MimeTypes.h"

#include "Errors.h"

inline beast::string_view mime_type(const std::filesystem::path& path) {
  if (const auto type = MimeTypes::getType(path.c_str())) {
    return type;
  }
  return "application/text";
}

http::message_generator
handle_static_file(const std::filesystem::path& doc_root,
                   http::request<http::dynamic_body> &&req) {
  // 确保HTTP方法合理
  if( req.method() != http::verb::get &&
      req.method() != http::verb::head)
    return bad_request(std::move(req), "Unknown HTTP-method");

  if (req.target().empty() || req.target()[0] != '/')
  {
    return bad_request(std::move(req), "Illegal request-target");
  }

  // std::cout << doc_root / ("." + std::string(req.target())) << std::endl;

  std::filesystem::path path = doc_root / ("." + std::string(req.target()));
  if(is_directory(path))
  {
    path /= "index.html";
  }

  // 尝试打开文件
  beast::error_code ec;
  http::file_body::value_type body;
  body.open(path.c_str(), beast::file_mode::scan, ec);

  if(ec == beast::errc::no_such_file_or_directory)
  {
    return not_found(std::move(req), req.target());
  }

  if(ec)
  {
    return server_error(std::move(req), ec.message());
  }

  // 提前计算文件大小
  auto const size = body.size();

  // HEAD
  if(req.method() == http::verb::head)
  {
    http::response<http::empty_body> res{http::status::ok, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, mime_type(path));
    res.content_length(size);
    res.keep_alive(req.keep_alive());
    return res;
  }

  // GET
  http::response<http::file_body> res{
    std::piecewise_construct,
    std::make_tuple(std::move(body)),
    std::make_tuple(http::status::ok, req.version())};
  res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(http::field::content_type, mime_type(path));
  res.content_length(size);
  res.keep_alive(req.keep_alive());

  if(path.extension()==".js" || path.extension()==".css")
  {
    res.set(http::field::expires, "12h");
  }

  return res;
}
