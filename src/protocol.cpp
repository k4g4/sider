#include "protocol.h"

#include <algorithm>
#include <iostream>
#include <numeric>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#define STRING_NEWTYPE(name)                                 \
  struct name {                                              \
    std::string inner;                                       \
    /* remove ability to copy */                             \
    name() = delete;                                         \
    name(name &&other) = default;                            \
    name(std::string &&string) : inner(std::move(string)) {} \
    std::string_view view() const { return inner; }          \
  }

STRING_NEWTYPE(SimpleString);

std::ostream &operator<<(std::ostream &os, SimpleString const &simple_string) {
  return os << '+' << simple_string.view() << "\r\n";
}

STRING_NEWTYPE(SimpleError);

std::ostream &operator<<(std::ostream &os, SimpleError const &simple_error) {
  return os << '-' << simple_error.view() << "\r\n";
}

STRING_NEWTYPE(BulkString);

std::ostream &operator<<(std::ostream &os, BulkString const &bulk_string) {
  return os << '$' << bulk_string.view().length() << "\r\n"
            << bulk_string.view() << "\r\n";
}

class Element {
  using ElemVars = std::variant<SimpleString, SimpleError, int64_t,
                                std::optional<BulkString>, std::vector<Element>,
                                std::nullptr_t>;
  ElemVars inner;

  Element(ElemVars &&inner) : inner(std::move(inner)) {}

  // only possible to construct element from factories that move values out
 public:
  static Element simple_string(std::string &&string) {
    return Element(ElemVars(SimpleString(std::move(string))));
  }

  static Element simple_error(std::string &&string) {
    return Element(ElemVars(SimpleError(std::move(string))));
  }

  static Element null_bulk_string() { return Element(ElemVars(std::nullopt)); }

  static Element bulk_string(std::string &&string) {
    return Element(ElemVars(BulkString(std::move(string))));
  }

  static Element array(std::vector<Element> &&array) {
    return Element(ElemVars(std::move(array)));
  }

  static Element integer(int64_t integer) { return Element(ElemVars(integer)); }

  static Element null() { return Element(ElemVars(nullptr)); }

  auto &&get_simple_string() {
    return std::get<SimpleString>(std::move(inner));
  }

  auto &&get_simple_error() { return std::get<SimpleError>(std::move(inner)); }

  auto &&get_bulk_string() {
    return std::get<std::optional<BulkString>>(std::move(inner));
  }

  auto &&get_array() {
    return std::get<std::vector<Element>>(std::move(inner));
  }

  auto get_integer() const { return std::get<int64_t>(inner); }

  operator bool() const { return std::holds_alternative<nullptr_t>(inner); }
};

namespace commands {

struct Echo {
  BulkString msg;

  Echo(BulkString &&msg) : msg(std::move(msg)) {}

  void visit(std::ostream &os) const { os << msg; }
};

struct Ping {
  std::optional<BulkString> msg;

  Ping() : msg(std::nullopt) {}
  Ping(BulkString &&msg) : msg(std::move(msg)) {}

  void visit(std::ostream &os) const {
    if (msg) {
      os << *msg;
    } else {
      os << SimpleString("PONG");
    }
  }
};

using Command = std::variant<Ping, Echo>;

struct Visitor {
  std::ostream &os;

  Visitor(std::ostream &os) : os(os) {}

  template <typename Command>
  void operator()(Command const &command) {
    command.visit(os);
  }
};

}  // namespace commands

namespace parsers {

template <typename T>
using Result = std::optional<std::pair<std::string_view, T>>;

template <typename T>
Result<T> result(std::string_view data, T &&t) {
  return std::make_optional(std::make_pair(data, std::move(t)));
}

auto tag(std::string_view tag) {
  return [=](std::string_view data) -> Result<std::string_view> {
    return data.starts_with(tag)
               ? result(data.substr(tag.length()), data.substr(0, tag.length()))
               : std::nullopt;
  };
}

Result<std::string_view> crlf(std::string_view data) {
  return tag("\r\n")(data);
}

auto one_of(std::string_view one_of) {
  return [=](std::string_view data) -> Result<char> {
    if (!data.empty()) {
      auto iter = std::find_if(one_of.begin(), one_of.end(),
                               [=](char c) { return data.front() == c; });

      return iter != one_of.end() ? result(data.substr(1), *iter)
                                  : std::nullopt;
    }

    return std::nullopt;
  };
}

template <typename... Parsers>
auto alt(Parsers... parsers) {
  return [=](std::string_view data) -> decltype(std::get<0>(
                                        make_tuple(parsers...))("")) {
    for (auto parser : {parsers...}) {
      auto res = parser(data);
      if (res) {
        return res;
      }
    }

    return std::nullopt;
  };
}

template <typename Parser1, typename Parser2>
auto then(Parser1 parser1, Parser2 parser2) {
  return [=](std::string_view data) -> decltype(parser2("")) {
    auto res = parser1(data);
    if (res) {
      auto [data2, _] = *res;

      return parser2(data2);
    }

    return std::nullopt;
  };
}

Result<size_t> number(std::string_view data) {
  auto end = std::find_if_not(data.begin(), data.end(),
                              [](char c) { return c >= '0' && c <= '9'; });
  if (end != data.begin()) {
    size_t size = std::accumulate(data.begin(), end, 0, [](size_t acc, char c) {
      return acc * 10 + (c - '0');
    });

    return result(data.substr(end - data.begin()), std::move(size));
  }

  return std::nullopt;
}

auto take_until(std::string_view until) {
  return [=](std::string_view data) {
    auto at = data.find(until);

    return at != data.npos ? result(data.substr(at), data.substr(0, at))
                           : std::nullopt;
  };
}

Result<Element> simple_string(std::string_view data) {
  auto res = tag("+")(data);

  if (res) {
    auto [data, _] = *res;

    if (res = take_until("\r\n")(data)) {
      auto [data, string] = *res;

      if (res = crlf(data)) {
        auto [data, __] = *res;

        return result(data, Element::simple_string(std::string(string)));
      }
    }
  }

  return std::nullopt;
}

Result<Element> simple_error(std::string_view data) {
  auto res = tag("-")(data);

  if (res) {
    auto [data, _] = *res;

    if (res = take_until("\r\n")(data)) {
      auto [data, string] = *res;

      if (res = crlf(data)) {
        auto [data, _] = *res;

        return result(data, Element::simple_error(std::string(string)));
      }
    }
  }

  return std::nullopt;
}

Result<Element> integer(std::string_view data) {
  auto res = tag(":")(data);

  if (res) {
    auto [data, _] = *res;
    auto res = one_of("+-")(data);

    if (res) {
      auto [data, sign] = *res;
      auto res = number(data);

      if (res) {
        auto [data, number] = *res;

        auto res = crlf(data);
        if (res) {
          auto [data, _] = *res;
          int64_t integer = sign == '+' ? number : -(int64_t)number;

          return result(data, Element::integer(integer));
        }
      }
    }
  }

  return std::nullopt;
}

Result<Element> bulk_string(std::string_view data) {
  auto res = then(tag("$"), number)(data);

  if (res) {
    auto [data, size] = *res;
    auto res = then(tag("-1"), crlf)(data);

    if (res) {
      auto [data, _] = *res;

      return result(data, Element::null_bulk_string());
    }

    if (res = crlf(data)) {
      auto [data, _] = *res;

      if (data.length() >= size) {
        auto string = data.substr(0, size);
        data = data.substr(size);

        if (res = crlf(data)) {
          auto [data, __] = *res;

          return result(data, Element::bulk_string(std::string(string)));
        }
      }
    }
  }

  return std::nullopt;
}

Result<Element> element(std::string_view data);

Result<Element> array(std::string_view data) {
  auto res = then(tag("*"), number)(data);

  if (res) {
    auto [data2, size] = *res;

    auto res = then(tag("-1"), crlf)(data);
    if (res) {
      auto [data, _] = *res;

      return result(data, Element::null_bulk_string());
    }

    if (res = crlf(data2)) {
      auto [data, _] = *res;

      std::vector<Element> array;
      array.reserve(size);

      for (size_t i = 0; i < size; i++) {
        auto res = element(data);

        if (res) {
          auto [new_data, element] = std::move(*res);

          array.push_back(std::move(element));
          data = new_data;

        } else {
          return std::nullopt;
        }
      }

      return result(data, Element::array(std::move(array)));
    }
  }

  return std::nullopt;
}

Result<Element> null(std::string_view data) {
  auto res = then(tag("_"), crlf)(data);

  if (res) {
    auto [data, _] = *res;

    return result(data, Element::null());
  }

  return std::nullopt;
}

Result<Element> element(std::string_view data) {
  return alt(simple_string, simple_error, integer, bulk_string, array,
             null)(data);
}

Result<commands::Command> parse_command(std::string_view data) {
  auto res = array(data);

  if (res) {
    auto [data, element] = std::move(*res);

    try {
      auto array = element.get_array();

      if (!array.empty()) {
        auto command_name = array[0].get_bulk_string();

        if (command_name) {
          auto command_name_is = [&](std::string_view rhs) {
            auto lhs = command_name->view();
            auto right = rhs.begin();

            return lhs.length() == rhs.length() &&
                   std::all_of(lhs.begin(), lhs.end(), [&](char left) {
                     return std::toupper(left) == *(right++);
                   });
          };

          if (command_name_is("PING")) {
            if (array.size() > 1) {
              auto arg = array[1].get_bulk_string();

              return arg ? result(data, commands::Ping(std::move(*arg)))
                         : result(data, commands::Ping());
            } else {
              return result(data, commands::Ping());
            }

          } else if (command_name_is("ECHO")) {
            if (array.size() > 1) {
              auto arg = array[1].get_bulk_string();

              if (arg) {
                return result(data, commands::Echo(std::move(*arg)));
              }
            }
          }
        }
      }
    } catch (std::bad_variant_access) {
    }
  }

  return std::nullopt;
}

};  // namespace parsers

void server_transact(buffer const &in, std::string &out) {
  auto res = parsers::parse_command(in.begin());
  std::ostringstream oss(std::move(out), std::ios_base::trunc);

  if (res) {
    auto [_, command] = std::move(*res);
    commands::Visitor visitor(oss);

    std::visit(visitor, command);

  } else {
    oss << SimpleError("server error");
  }

  out = std::move(oss.str());
}

std::string client_parse(buffer const &in) { return ""; }