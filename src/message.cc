//
// Copyright (C) Michael Yang. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full
// license information.
//

#include "config.h"
#include "message.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace ohmyarch {
static std::int32_t last_update_id = -1;

std::experimental::optional<std::string> get_me() {
    web::http::client::http_client client(api_uri + "getMe", client_config);

    try {
        nlohmann::json json =
            nlohmann::json::parse(client.request(web::http::methods::GET)
                                      .get()
                                      .extract_string()
                                      .get());

        if (!json.at("ok").get<bool>()) {
            spdlog::get("logger")->error(
                "get_me: {}", json.at("description")
                                  .get_ref<const nlohmann::json::string_t &>());

            return {};
        }

        return json.at("result").at("username");
    } catch (const std::exception &error) {
        spdlog::get("logger")->error("❌ get_me: {}", error.what());

        return {};
    }
}

std::experimental::optional<std::vector<update>> get_updates() {
    web::uri_builder builder(api_uri + "getUpdates");

    if (last_update_id != -1)
        builder.append_query("offset", last_update_id);

    web::http::client::http_client client(builder.to_uri(), client_config);

    std::vector<update> updates;

    try {
        nlohmann::json json =
            nlohmann::json::parse(client.request(web::http::methods::GET)
                                      .get()
                                      .extract_string()
                                      .get());

        if (!json.at("ok").get<bool>()) {
            spdlog::get("logger")->error(
                "get_updates: {}",
                json.at("description")
                    .get_ref<const nlohmann::json::string_t &>());

            return {};
        }

        for (const auto &update_object : json.at("result")) {
            update update;
            update.update_id_ = update_object.at("update_id");

            bool edited = false;

            const auto iterator_message = update_object.find("message");
            const auto iterator_edited_message =
                update_object.find("edited_message");

            auto iterator = update_object.end();
            if (iterator_message != update_object.end()) {
                iterator = iterator_message;
            } else if (iterator_edited_message != update_object.end()) {
                iterator = iterator_edited_message;

                edited = true;
            }

            if (iterator != update_object.end()) {
                message message;

                const auto &message_object = iterator.value();
                message.id_ = message_object.at("message_id");

                const auto &chat_object = message_object.at("chat");
                message.chat_.id_ = chat_object.at("id");

                const auto iterator_text = message_object.find("text");
                if (iterator_text != message_object.end())
                    message.text_ =
                        iterator_text.value()
                            .get_ref<const nlohmann::json::string_t &>();

                const auto iterator_entities = message_object.find("entities");
                if (iterator_entities != message_object.end()) {
                    std::vector<message_entity> entities;

                    for (const auto &entity_object : iterator_entities.value())
                        entities.emplace_back(entity_object.at("type"),
                                              entity_object.at("offset"),
                                              entity_object.at("length"));

                    message.entities_.emplace(std::move(entities));
                }

                if (edited)
                    update.edited_message_.emplace(std::move(message));
                else
                    update.message_.emplace(std::move(message));
            }

            updates.emplace_back(std::move(update));
        }

        if (updates.empty())
            return {};

        last_update_id = updates.back().update_id() + 1;

        return std::move(updates);
    } catch (const std::exception &error) {
        spdlog::get("logger")->error("❌ get_updates: {}", error.what());

        return {};
    }
}

void send_message(std::int64_t chat_id, const std::string &text,
                  std::experimental::optional<std::int32_t> rely_to,
                  std::experimental::optional<formatting_options> parse_mode) {
    web::uri_builder builder(api_uri + "sendMessage");
    builder.append_query("chat_id", chat_id);
    builder.append_query("text", text);
    if (parse_mode) {
        if (parse_mode.value() == formatting_options::markdown_style)
            builder.append_query("parse_mode", "Markdown");
        else
            builder.append_query("parse_mode", "HTML");
    }
    if (rely_to)
        builder.append_query("reply_to_message_id", rely_to.value());

    web::http::client::http_client client(builder.to_uri(), client_config);

    try {
        client.request(web::http::methods::GET).get();
    } catch (const std::exception &error) {
        spdlog::get("logger")->error("❌ send_message: {}", error.what());
    }
}

void send_document(std::int64_t chat_id, const std::string &uri) {
    web::uri_builder builder(api_uri + "sendDocument");
    builder.append_query("chat_id", chat_id);
    builder.append_query("document", uri);

    web::http::client::http_client client(builder.to_uri(), client_config);

    try {
        client.request(web::http::methods::GET).get();
    } catch (const std::exception &error) {
        spdlog::get("logger")->error("❌ send_document: {}", error.what());
    }
}
}
