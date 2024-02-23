//
// Created by cinea on 24-2-22.
//

#ifndef ERRORS_H
#define ERRORS_H

#include "Common.h"

http::message_generator
bad_request(http::request<http::dynamic_body>&& req,
            const beast::string_view& why);

http::message_generator
not_found(http::request<http::dynamic_body> &&req,
          const beast::string_view& target);

http::message_generator
server_error(http::request<http::dynamic_body>&& req,
             const beast::string_view& what);

#endif //ERRORS_H
