//
// Copyright (C) Michael Yang. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full
// license information.
//

#include "config.h"
#include "funny_pics.h"
#include "joke.h"
#include "message.h"
#include "quote.h"
#include <boost/program_options.hpp>
#include <csignal>
#include <fstream>
#include <future>
#include <iostream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#include <boost/thread/future.hpp>

static std::atomic<bool> keep_running(true);

void signal_handler(int signal) { keep_running = false; }

int main(int argc, char *argv[]) {
    std::string path_to_config;

    boost::program_options::options_description options("options");
    options.add_options()(
        "config", boost::program_options::value<std::string>(&path_to_config)
                      ->value_name("/path/to/config"),
        "specify a path to a custom config file")("help,h",
                                                  "print this text and exit");

    boost::program_options::variables_map map;

    try {
        boost::program_options::store(
            boost::program_options::parse_command_line(argc, argv, options),
            map);
    } catch (const boost::program_options::error &error) {
        std::cerr << "❌ " << error.what() << std::endl;

        return 1;
    }

    if (map.count("help") || map.empty()) {
        std::cout << options;

        return 0;
    }

    boost::program_options::notify(map);

    if (path_to_config.empty()) {
        std::cerr << "❌ option '--config' is missing" << std::endl;

        return 1;
    }

    std::ifstream config_file(path_to_config);
    if (!config_file) {
        std::cerr << "❌ config file opening failed" << std::endl;

        return 1;
    }

    nlohmann::json json;

    try {
        json << config_file;

        ohmyarch::api_uri =
            "https://api.telegram.org/bot" +
            json.at("token").get_ref<const nlohmann::json::string_t &>() + '/';
    } catch (const std::exception &error) {
        std::cerr << "❌ json: " << error.what() << std::endl;

        return 1;
    }

    const auto iterator_http_proxy = json.find("http_proxy");
    if (iterator_http_proxy != json.end())
        try {
            ohmyarch::client_config.set_proxy(web::web_proxy(
                iterator_http_proxy.value()
                    .get_ref<const nlohmann::json::string_t &>()));
        } catch (const std::exception &error) {
            std::cerr << "❌ set_proxy: " << error.what() << std::endl;

            return 1;
        }

    spdlog::set_async_mode(8192, spdlog::async_overflow_policy::block_retry,
                           nullptr, std::chrono::minutes(1));

    try {
        std::vector<spdlog::sink_ptr> sinks{
            std::make_shared<spdlog::sinks::ansicolor_sink>(
                std::make_shared<spdlog::sinks::stdout_sink_mt>()),
            std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                json.at("log_path"), "txt", 1024 * 1024 * 5, 0)};
        auto combined_logger = std::make_shared<spdlog::logger>(
            "logger", sinks.begin(), sinks.end());
        combined_logger->flush_on(spdlog::level::err);

        spdlog::register_logger(combined_logger);
    } catch (const std::exception &error) {
        std::cout << "❌ log failed: " << error.what() << std::endl;

        return 1;
    }

    std::signal(SIGINT, signal_handler);

    const auto bot_username = ohmyarch::get_me();
    if (!bot_username)
        return 1;

    const std::string &username = bot_username.value();

    const std::string quote_command = "/quote@" + username;
    const std::string joke_command = "/joke@" + username;
    const std::string funny_pics_command = "/funny_pics@" + username;
    const std::string about_command = "/about@" + username;

    spdlog::get("logger")->info("🤖️ @{} is running 😉", username);
    spdlog::get("logger")->flush();

    for (;;) {
        if (!keep_running) {
            spdlog::get("logger")->info("🤖️ @{} stopped 😴", username);

            break;
        }

        const auto updates = ohmyarch::get_updates();
        if (updates)
            for (const auto &update : updates.value()) {
                const auto &message = update.message();
                if (message) {
                    const auto &entities = message->entities();
                    if (entities) {
                        std::u16string text =
                            utility::conversions::utf8_to_utf16(
                                message->text().value());

                        for (const auto &entity : entities.value())
                            if (entity.type() == "bot_command") {
                                const std::string bot_command =
                                    utility::conversions::utf16_to_utf8(
                                        text.substr(entity.offset(),
                                                    entity.length()));

                                if (bot_command == "/quote" ||
                                    bot_command == quote_command) {
                                    boost::async(ohmyarch::get_quote)
                                        .then([chat_id = message->chat().id()](
                                            boost::unique_future<
                                                std::experimental::optional<
                                                    ohmyarch::quote>>
                                                task) {
                                            const auto quote = task.get();
                                            if (quote)
                                                ohmyarch::send_message(
                                                    chat_id,
                                                    quote->text() + " - " +
                                                        quote->author());
                                        });
                                } else if (bot_command == "/joke" ||
                                           bot_command == joke_command) {
                                    boost::async(ohmyarch::get_joke)
                                        .then([chat_id = message->chat().id()](
                                            boost::unique_future<
                                                std::experimental::optional<
                                                    std::string>>
                                                task) {
                                            const auto joke = task.get();
                                            if (joke)
                                                ohmyarch::send_message(
                                                    chat_id, joke.value());
                                        });
                                } else if (bot_command == "/funny_pics" ||
                                           bot_command == funny_pics_command) {
                                    boost::async(ohmyarch::get_funny_pics)
                                        .then([chat_id = message->chat().id()](
                                            boost::unique_future<
                                                std::experimental::optional<
                                                    std::vector<std::string>>>
                                                task) {
                                            const auto funny_pics = task.get();
                                            if (funny_pics)
                                                for (const auto &pic_uri :
                                                     funny_pics.value()) {
                                                    if (boost::ends_with(
                                                            pic_uri, "gif"))
                                                        ohmyarch::send_document(
                                                            chat_id, pic_uri);
                                                    else
                                                        ohmyarch::send_message(
                                                            chat_id, pic_uri);
                                                }
                                        });
                                } else if (bot_command == "/about" ||
                                           bot_command == about_command) {
                                    std::async(ohmyarch::send_message,
                                               message->chat().id(),
                                               "https://github.com/ohmyarch/"
                                               "ohmyarch_bot");
                                }
                            }
                    }
                }
            }
    }
}
