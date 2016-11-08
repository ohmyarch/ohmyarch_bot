//
// Copyright (C) Michael Yang. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
//

#pragma once

#include <experimental/optional>
#include <string>

namespace ohmyarch {
class quote {
  public:
    quote(quote &&other) noexcept
        : author_(std::move(other.author_)), text_(std::move(other.text_)) {}
    quote(const std::string &author, const std::string &text)
        : author_(author), text_(text) {}

    const std::string &author() const { return author_; }
    const std::string &text() const { return text_; }

    quote &operator=(const quote &other) {
        author_ = other.author_;
        text_ = other.text_;

        return *this;
    }

  private:
    std::string author_;
    std::string text_;
    ;
};

std::experimental::optional<quote> get_quote();
}
