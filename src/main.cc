//
// Copyright (C) Michael Yang. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full
// license information.
//

#include "config.h"
#include "funny_pics.h"
#include "girl_pics.h"
#include "joke.h"
#include "message.h"
#include "quote.h"
#include "run_cpp.h"
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/program_options.hpp>
#include <csignal>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

enum class bot_command : std::uint8_t {
    quote,
    joke,
    funny_pics,
    girl_pics,
    run_cpp,
    about
};

class message {
  public:
    message(bot_command command) : command_(command) {}
    message(bot_command command, std::int32_t id, std::string &&code) noexcept
        : command_(command), id_(id), code_(std::move(code)) {}

    bot_command command() const { return command_; }
    std::int32_t id() const { return id_; }
    const std::string &code() const { return code_; }

  private:
    bot_command command_;
    std::int32_t id_;
    std::string code_;
};

using namespace std::chrono_literals;
using message_queue =
    boost::lockfree::spsc_queue<message, boost::lockfree::capacity<100>>;

static std::atomic<bool> keep_running(true);

static std::unordered_map<std::int64_t, std::shared_ptr<message_queue>>
    message_queue_map;

static std::mutex map_mutex;

static void signal_handler(int signal) { keep_running = false; }

static void consumer(std::int64_t chat_id,
                     std::shared_ptr<message_queue> queue) {
    spdlog::get("logger")->info("ℹ️ thread is created for 💬<{}>",
                                chat_id);

    std::chrono::time_point<std::chrono::steady_clock> start;

    for (;;) {
        if (!queue->consume_all([&chat_id, &start](message message) {
                switch (message.command()) {
                case bot_command::quote: {
                    const auto quote = ohmyarch::get_quote();
                    if (quote)
                        ohmyarch::send_message(
                            chat_id,
                            "_" + quote->text() + " - " + quote->author() + "_",
                            {}, ohmyarch::formatting_options::markdown_style);

                    break;
                }
                case bot_command::joke: {
                    const auto joke = ohmyarch::get_joke();
                    if (joke)
                        ohmyarch::send_message(chat_id, joke.value());

                    break;
                }
                case bot_command::funny_pics: {
                    const auto funny_pics = ohmyarch::get_funny_pics();
                    if (funny_pics) {
                        for (const auto &pic_uri : funny_pics.value())
                            if (boost::ends_with(pic_uri, "gif"))
                                ohmyarch::send_document(chat_id, pic_uri);
                            else
                                ohmyarch::send_message(chat_id, pic_uri);
                    }

                    break;
                }
                case bot_command::girl_pics: {
                    const auto girl_pics = ohmyarch::get_girl_pics();
                    if (girl_pics) {
                        for (const auto &pic_uri : girl_pics.value())
                            if (boost::ends_with(pic_uri, "gif"))
                                ohmyarch::send_document(chat_id, pic_uri);
                            else
                                ohmyarch::send_message(chat_id, pic_uri);
                    }

                    break;
                }
                case bot_command::run_cpp: {
                    auto output = ohmyarch::run_cpp(message.code());
                    if (output) {
                        std::string &output_str = output.value();
                        boost::replace_all(output.value(), "\n", "`\n`");

                        ohmyarch::send_message(
                            chat_id, '`' + output_str + '`', message.id(),
                            ohmyarch::formatting_options::markdown_style);
                    }

                    break;
                }
                case bot_command::about: {
                    ohmyarch::send_message(
                        chat_id, "https://github.com/ohmyarch/ohmyarch_bot");

                    break;
                }
                }

                start = std::chrono::steady_clock::now();
            })) {
            if (std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                              start)
                    .count() > 10.0) {
                break;
            } else {
                std::this_thread::yield();
                std::this_thread::sleep_for(100ms);
            }
        }
    }

    {
        std::lock_guard<std::mutex> guard(map_mutex);
        message_queue_map.erase(chat_id);
    }

    spdlog::get("logger")->info("ℹ️ thread is destroyed for 💬<{}>",
                                chat_id);
}

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

    spdlog::set_async_mode(8192);

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
    const std::string girl_pics_command = "/girl_pics@" + username;
    const std::string run_cpp_command = "/run_cpp@" + username;
    const std::string about_command = "/about@" + username;

    spdlog::get("logger")->info("🤖️ @{} is running 😉", username);
    spdlog::get("logger")->flush();

    while (keep_running) {
        const auto updates = ohmyarch::get_updates();
        if (updates)
            for (const auto &update : updates.value()) {
                const auto &message = update.message();
                const auto &edited_message = update.edited_message();
                if (message) {
                    const auto &entities = message->entities();
                    if (entities) {
                        const std::u16string text =
                            utility::conversions::utf8_to_utf16(
                                message->text().value());

                        std::vector<class message> messages;

                        for (const auto &entity : entities.value())
                            if (entity.type() == "bot_command") {
                                const std::string command_text =
                                    utility::conversions::utf16_to_utf8(
                                        text.substr(entity.offset(),
                                                    entity.length()));
                                if (command_text == "/quote" ||
                                    command_text == quote_command) {
                                    messages.emplace_back(bot_command::quote);
                                } else if (command_text == "/joke" ||
                                           command_text == joke_command) {
                                    messages.emplace_back(bot_command::joke);
                                } else if (command_text == "/funny_pics" ||
                                           command_text == funny_pics_command) {
                                    messages.emplace_back(
                                        bot_command::funny_pics);
                                } else if (command_text == "/girl_pics" ||
                                           command_text == girl_pics_command) {
                                    messages.emplace_back(
                                        bot_command::girl_pics);
                                } else if (command_text == "/run_cpp" ||
                                           command_text == run_cpp_command) {
                                    if (entity.offset() == 0) {
                                        const std::size_t entities_size =
                                            entities->size();
                                        if (entities_size > 1) {
                                            bool code_or_pre = true;

                                            std::string code;

                                            for (int index = 1;
                                                 index < entities_size;
                                                 ++index) {
                                                const auto &code_entity =
                                                    entities->at(index);
                                                if (code_entity.type() ==
                                                        "code" ||
                                                    code_entity.type() ==
                                                        "pre") {
                                                    code +=
                                                        utility::conversions::
                                                            utf16_to_utf8(text.substr(
                                                                code_entity
                                                                    .offset(),
                                                                code_entity
                                                                    .length())) +
                                                        '\n';
                                                } else {
                                                    code_or_pre = false;

                                                    break;
                                                }
                                            }

                                            if (code_or_pre)
                                                messages.emplace_back(
                                                    bot_command::run_cpp,
                                                    message->id(),
                                                    std::move(code));
                                        }

                                        break;
                                    }
                                } else if (command_text == "/about" ||
                                           command_text == about_command) {
                                    messages.emplace_back(bot_command::about);
                                }
                            }

                        if (messages.empty())
                            continue;

                        const std::int64_t chat_id = message->chat().id();

                        std::unique_lock<std::mutex> lock(map_mutex);
                        const auto pair = message_queue_map.emplace(
                            chat_id, std::make_shared<message_queue>());
                        lock.unlock();

                        auto &queue = pair.first->second;
                        queue->push(messages.begin(), messages.end());

                        if (pair.second) {
                            std::thread consumer_thread(consumer, chat_id,
                                                        queue);
                            consumer_thread.detach();
                        }
                    }
                } else if (edited_message) {
                    const auto &entities = edited_message->entities();
                    if (entities) {
                        const std::size_t entities_size = entities->size();
                        if (entities_size > 1) {
                            const auto &command_entity = entities->front();
                            if (command_entity.offset() == 0 &&
                                command_entity.type() == "bot_command") {
                                const std::u16string text =
                                    utility::conversions::utf8_to_utf16(
                                        edited_message->text().value());
                                const std::string command_text =
                                    utility::conversions::utf16_to_utf8(
                                        text.substr(0,
                                                    command_entity.length()));
                                if (command_text == "/run_cpp" ||
                                    command_text == run_cpp_command) {
                                    bool code_or_pre = true;

                                    std::string code;

                                    for (int index = 1; index < entities_size;
                                         ++index) {
                                        const auto &code_entity =
                                            entities->at(index);
                                        if (code_entity.type() == "code" ||
                                            code_entity.type() == "pre") {
                                            code +=
                                                utility::conversions::
                                                    utf16_to_utf8(text.substr(
                                                        code_entity.offset(),
                                                        code_entity.length())) +
                                                '\n';
                                        } else {
                                            code_or_pre = false;

                                            break;
                                        }
                                    }

                                    if (code_or_pre) {
                                        const std::int64_t chat_id =
                                            edited_message->chat().id();

                                        std::unique_lock<std::mutex> lock(
                                            map_mutex);
                                        const auto pair =
                                            message_queue_map.emplace(
                                                chat_id, std::make_shared<
                                                             message_queue>());
                                        lock.unlock();

                                        auto &queue = pair.first->second;
                                        queue->push({bot_command::run_cpp,
                                                     edited_message->id(),
                                                     std::move(code)});

                                        if (pair.second) {
                                            std::thread consumer_thread(
                                                consumer, chat_id, queue);
                                            consumer_thread.detach();
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
    }

    spdlog::get("logger")->info("🤖️ @{} stopped 😴", username);
}
