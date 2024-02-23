#include <boost/beast.hpp>
#include <boost/beast/core/string_type.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/fields.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/message_generator.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/version.hpp>
#include <string>

#include "BoostCommon.h"

using namespace std::string_literals;

http::message_generator
bad_request(http::request<http::dynamic_body> &&req,
            const beast::string_view& why) {
  http::response<http::string_body> res{http::status::bad_request,
                                        req.version()};

  res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(http::field::content_type, "text/html");
  res.keep_alive(req.keep_alive());
  res.body() = std::string(why);
  res.prepare_payload();
  return res;
}

http::message_generator
not_found(http::request<http::dynamic_body> &&req,
          const beast::string_view& target) {
  http::response<http::string_body> res{http::status::not_found, req.version()};

  res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(http::field::content_type, "text/html");
  res.keep_alive(req.keep_alive());
  res.body() = "The resource '"s + std::string(target) + "' was not found"s;
  res.prepare_payload();
  return res;
}

http::message_generator
server_error(http::request<http::dynamic_body> &&req,
             const beast::string_view& what) {
  http::response<http::string_body> res{http::status::internal_server_error,
                                        req.version()};

  res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(http::field::content_type, "text/html");
  res.keep_alive(req.keep_alive());
  res.body() = "An error occured: '"s + std::string(what) + "'"s;
  res.prepare_payload();
  return res;
}
