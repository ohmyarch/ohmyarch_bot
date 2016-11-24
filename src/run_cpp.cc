//
// Copyright (C) Michael Yang. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full
// license information.
//

#include "run_cpp.h"
#include <cpprest/http_client.h>
#include <spdlog/spdlog.h>

namespace ohmyarch {
std::experimental::optional<std::string> run_cpp(const std::string &code) {
    web::http::client::http_client client(
        "http://coliru.stacked-crooked.com/compile");

    web::json::value body_data;
    body_data["cmd"] = web::json::value::string(
        "g++ -std=c++1z -fconcepts -fgnu-tm -O3 -Wall -Wextra "
        "-pedantic-errors main.cpp -pthread -lm -latomic -lstdc++fs && "
        "./a.out");
    body_data["src"] = web::json::value::string(code);

    try {
        return client.request(web::http::methods::POST, {}, body_data)
            .get()
            .extract_string()
            .get();
    } catch (const std::exception &error) {
        spdlog::get("logger")->error("❌ run_cpp: {}", error.what());

        return {};
    }
}
}
