#include <filesystem>
#include <iostream>
#include <shared_mutex>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core/string_type.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/fields.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/message_generator.hpp>
#include <boost/beast/http/verb.hpp>

#include <date-rfc/date-rfc.h>

#include "Common.h"
#include "../libs/MimeTypes/MimeTypes.h"
#include "../libs/lrucache11/LRUCache11.hpp"

#include "Errors.h"

inline beast::string_view mime_type(const std::filesystem::path& path) {
  if (const auto type = MimeTypes::getType(path.c_str())) {
    return type;
  }
  return "application/text";
}

std::vector<u_char> load_static_file(const std::string& path, beast::error_code& ec)
{
  // 打开文件
  beast::file_posix file;
  file.open(path.c_str(), beast::file_mode::scan, ec);

  if (ec) { return std::vector<u_char>{}; }

  // 读取文件内容到缓存
  std::vector<u_char> file_contents(file.size(ec));
  file.read(file_contents.data(), file.size(ec), ec);

  if (ec) { return std::vector<u_char>{}; }

  return file_contents;
}

std::tuple<http::vector_body<u_char>::value_type, std::time_t> get_static_file(
  const std::filesystem::path& path, const std::optional<std::time_t> if_modified_since, beast::error_code& ec)
{
  static lru11::Cache<std::string, std::vector<u_char>> static_file_cache(20);
  static std::unordered_map<std::string, std::time_t> static_file_version;
  static std::shared_mutex mutex;

  if (!exists(path))
  {
    ec = beast::error_code(beast::errc::no_such_file_or_directory, boost::system::generic_category());
    return std::make_tuple(std::vector<u_char>(), 0);
  }

  const auto path_str = std::string(path);
  const auto last_modified = fs_time_to_time_t(last_write_time(path));

  if (if_modified_since.has_value() && *if_modified_since >= last_modified)
  {
    // 客户端缓存有效
    return std::make_tuple(std::vector<u_char>(), last_modified);
  }

  {
    std::shared_lock guard(mutex);
    if (static_file_cache.contains(path_str))
    {
      // 检查最后修改时间
      if (last_modified <= static_file_version.find(path_str)->second)
        return std::make_tuple(static_file_cache.get(path), last_modified);
    }
  }

  auto body = load_static_file(path, ec);
  if (ec) return std::make_tuple(std::move(body), last_modified);

  if (body.size() < 10 * 1024 * 1024) // 10MB
  {
    std::lock_guard guard(mutex);

    static_file_version.insert_or_assign(path_str, last_modified);
    static_file_cache.remove(path_str);
    static_file_cache.insert(path_str, body);
  }

  return std::make_tuple(std::move(body), last_modified);
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

  // 解析缓存相关属性
  std::optional<std::time_t> if_modified_since;
  if (const auto str = req[http::field::if_modified_since]; !str.empty())
  {
    time_t dt;
    std::istringstream iss(str);
    iss >> date::format_rfc1123(dt);
    if_modified_since = std::optional(dt);
  }

  // 尝试打开文件
  beast::error_code ec;
  http::vector_body<u_char>::value_type body;
  std::time_t last_modified;
  std::tie(body, last_modified) = get_static_file(path, if_modified_since, ec);

  if (ec == beast::errc::no_such_file_or_directory)
  {
    using namespace std::string_view_literals;

    if (req.target() != "/index.html"sv)
    {
      req.target("/index.html");
      return handle_static_file(doc_root, std::move(req)); // 使用index.html重试，目的是兼容Vue Router的h5历史
    }

    return not_found(std::move(req), req.target());
  }

  if(ec)
  {
    return server_error(std::move(req), ec.message());
  }

  // 制作LastModified字符串
  std::string last_modified_http_date;
  {
    std::ostringstream oss;
    oss << date::format_rfc1123(last_modified);
    last_modified_http_date = oss.str();
  }

  // 提前计算文件大小
  auto const size = body.size();

  // 客户端缓存是否命中？
  if (if_modified_since.has_value() && *if_modified_since >= last_modified)
  {
    http::response<http::empty_body> res{http::status::not_modified, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::cache_control, "public");
    res.set(http::field::content_type, mime_type(path));
    res.set(http::field::last_modified, last_modified_http_date);
    res.keep_alive(req.keep_alive());
    return res;
  }

  // HEAD
  if(req.method() == http::verb::head)
  {
    http::response<http::empty_body> res{http::status::ok, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, mime_type(path));
    res.set(http::field::last_modified, last_modified_http_date);
    res.content_length(size);
    res.keep_alive(req.keep_alive());
    return res;
  }

  // 制作Expires字符串
  std::string expires;
  {
    std::ostringstream oss;
    auto exp_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() + std::chrono::hours(12));
    oss << date::format_rfc1123(exp_time);
    expires = oss.str();
  }

  // GET
  http::response<http::vector_body<u_char>> res{
    std::piecewise_construct,
    std::make_tuple(std::move(body)),
    std::make_tuple(http::status::ok, req.version())};
  res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(http::field::cache_control, "public");
  res.set(http::field::content_type, mime_type(path));
  res.set(http::field::last_modified, last_modified_http_date);
  res.content_length(size);
  res.keep_alive(req.keep_alive());

  if(path.extension()==".js" || path.extension()==".css")
  {
    res.set(http::field::expires, expires);
  }

  return res;
}
