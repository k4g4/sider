#include "protocol.h"

#include <algorithm>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

struct BulkString {
  std::string inner;

  // remove ability to copy
  BulkString() = delete;
  BulkString(BulkString &&other) = default;
  BulkString(std::string &&string) : inner(std::move(string)) {}
};

class Element {
  using ElemVars = std::variant<std::string, BulkString, std::vector<Element>>;
  ElemVars inner;

  Element(ElemVars &&inner) : inner(std::move(inner)) {}

  // only possible to construct element from factories that move values out
 public:
  static Element simple_string(std::string &&string) {
    return Element(ElemVars(std::move(string)));
  }

  static Element bulk_string(std::string &&string) {
    return Element(ElemVars(BulkString(std::move(string))));
  }

  static Element array(std::vector<Element> &&array) {
    return Element(ElemVars(std::move(array)));
  }
};

// Commands

struct Echo {
  BulkString msg;

  Echo(BulkString &&msg) : msg(std::move(msg)) {}
};

struct Ping {
  std::optional<BulkString> msg;

  Ping() : msg(std::nullopt) {}
  Ping(BulkString &&msg) : msg(std::move(msg)) {}
};

using Command = std::variant<Ping, Echo>;

// parser combinators use std::optional as monad
namespace parsers {

template <typename T>
using Result = std::optional<std::pair<std::string_view, T>>;

auto tag(std::string_view tag) {
  return [=](std::string_view data) -> Result<std::string_view> {
    return data.starts_with(tag)
               ? std::make_optional(std::make_pair(
                     data.substr(tag.length()), data.substr(0, tag.length())))
               : std::nullopt;
  };
}

Result<std::string_view> crlf(std::string_view data) {
  return tag("\r\n")(data);
}

auto one_of(std::string_view one_of) {
  return [=](std::string_view data) -> Result<char> {
    if (data.empty()) {
      return std::nullopt;
    }
    auto result = std::find_if(one_of.begin(), one_of.end(),
                               [=](char c) { return data.front() == c; });
    return result != one_of.end()
               ? std::make_optional(std::make_pair(data.substr(1), *result))
               : std::nullopt;
  };
}

template <typename... Parsers>
auto alt(Parsers... parsers) {
  return [=](std::string_view data) -> decltype(std::get<0>(
                                        make_tuple(parsers...))("")) {
    for (auto parser : {parsers...}) {
      auto result = parser(data);
      if (result) {
        return result;
      }
    }
    return std::nullopt;
  };
}

template <typename Parser1, typename Parser2>
auto then(Parser1 parser1, Parser2 parser2) {
  return [=](std::string_view data) -> decltype(parser2("")) {
    auto result = parser1(data);
    if (result) {
      auto [data, _] = result.value();
      return parser2(data);
    }
    return std::nullopt;
  };
}

Result<size_t> size(std::string_view data) {
  auto end = std::find_if_not(data.begin(), data.end(),
                              [](char c) { return c >= '0' && c <= '9'; });
  if (end == data.begin()) {
    return std::nullopt;
  }
  size_t size = std::accumulate(data.begin(), end, 0, [](size_t acc, char c) {
    return acc * 10 + (c - '0');
  });
  return std::make_optional(
      std::make_pair(data.substr(end - data.begin()), size));
}

auto take_until(std::string_view until) {
  return [=](std::string_view data) {
    auto at = data.find(until);
    return at != data.npos ? std::make_optional(std::make_pair(
                                 data.substr(at), data.substr(0, at)))
                           : std::nullopt;
  };
}

Result<Element> simple_string(std::string_view data) {
  auto result = tag("+")(data);
  if (!result) {
    return std::nullopt;
  }
  auto result2 = take_until("\r\n")(data);
  if (!result2) {
    return std::nullopt;
  }
  auto [data2, string] = result.value();
  auto result3 = crlf(data2);
  if (!result3) {
    return std::nullopt;
  }
  auto [data3, _] = result3.value();

  return std::make_optional(
      std::make_pair(data3, Element::simple_string(std::string(string))));
}

Result<Element> element(std::string_view data);

Result<Element> array(std::string_view data) {
  auto result = then(tag("*"), size)(data);
  if (!result) {
    return std::nullopt;
  }

  auto [data2, size] = result.value();
  auto result2 = crlf(data2);
  if (!result2) {
    return std::nullopt;
  }

  std::vector<Element> array;
  std::string_view data_out = data2;
  array.reserve(size);

  for (size_t i = 0; i < size; i++) {
    auto result3 = element(data_out);
    if (!result3) {
      return std::nullopt;
    }
    auto [data3, element] = std::move(result3.value());
    array.push_back(std::move(element));
    data_out = data3;
  }

  return std::make_optional(
      std::make_pair(data_out, Element::array(std::move(array))));
}

Result<Element> element(std::string_view data) { return alt(array)(data); }

Result<Command> parse_command(std::string_view data) { return std::nullopt; }

};  // namespace parsers

void transact(buffer const &in, buffer &out, int &out_len) {
  auto result = parsers::parse_command(in.begin());
  if (!result) {
    //
  }
  auto [_, command] = std::move(result.value());
}