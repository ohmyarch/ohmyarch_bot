//
// Copyright (C) Michael Yang. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full
// license information.
//

#include "quote.h"
#include <cpprest/http_client.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace ohmyarch {
std::experimental::optional<quote> get_quote() {
    web::uri_builder builder("http://api.forismatic.com/api/1.0/");
    builder.append_query("method", "getQuote")
        .append_query("format", "json")
        .append_query("lang", "en");

    web::http::client::http_client client(builder.to_uri());

    try {
        nlohmann::json json =
            nlohmann::json::parse(client.request(web::http::methods::GET)
                                      .get()
                                      .extract_string()
                                      .get());

        return quote(json.at("quoteAuthor"), json.at("quoteText"));
    } catch (const std::exception &error) {
        spdlog::get("logger")->error("❌ get_quote: {}", error.what());

        return {};
    }
}
}
