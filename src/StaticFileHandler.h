//
// Created by cinea on 24-2-22.
//

#ifndef STATICFILEHANDLER_H
#define STATICFILEHANDLER_H

#include <filesystem>

#include "BoostCommon.h"

http::message_generator
handle_static_file(const std::filesystem::path& doc_root,
                   http::request<http::dynamic_body> &&req);

#endif //STATICFILEHANDLER_H
