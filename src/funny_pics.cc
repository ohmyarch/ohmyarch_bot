//
// Copyright (C) Michael Yang. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full
// license information.
//

#include "funny_pics.h"
#include <cpprest/http_client.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace ohmyarch {
static std::mt19937_64
    engine((std::random_device().operator()()));

std::experimental::optional<std::vector<std::string>> get_funny_pics() {
    std::uniform_int_distribution<int> gen_page_index(1, 300);

    web::uri_builder builder(
        "http://jandan.net/?oxwlxojflwblxbsapi=jandan.get_pic_comments");
    builder.append_query("page", gen_page_index(engine));

    web::http::client::http_client client(builder.to_uri());

    try {
        nlohmann::json json =
            nlohmann::json::parse(client.request(web::http::methods::GET)
                                      .get()
                                      .extract_string()
                                      .get());

        if (json.at("status") != "ok")
            return {};

        std::vector<std::string> pics;

        std::uniform_int_distribution<int> gen_comment_index(0, 24);

        for (auto &pic :
             json.at("comments").at(gen_comment_index(engine)).at("pics")) {
            std::string &pic_uri = pic.get_ref<nlohmann::json::string_t &>();
            pic_uri.replace(boost::find_nth(pic_uri, "/", 2).begin() + 1,
                            boost::find_nth(pic_uri, "/", 3).begin(), "large");

            pics.emplace_back(pic_uri);
        }

        return pics;
    } catch (const std::exception &error) {
        spdlog::get("logger")->error("get_funny_pics: {}", error.what());

        return {};
    }
}
}
