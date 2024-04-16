#pragma once

#include <optional>
#include <ostream>
#include <variant>
#include <vector>

// somehow, this method was easier for me than inheritance...
#define STRING_NEWTYPE(name)                                                  \
  struct name {                                                               \
    std::string inner;                                                        \
    name() = default;                                                         \
    name(name const &other) = default;                                        \
    name(name &&other) = default;                                             \
    name(std::string &&string) : inner(std::move(string)) {}                  \
    name &operator=(name &&other) {                                           \
      inner = std::move(other.inner);                                         \
      return *this;                                                           \
    }                                                                         \
    bool operator==(name const &other) const { return inner == other.inner; } \
    std::string_view view() const { return inner; }                           \
  }

STRING_NEWTYPE(SimpleString);

static std::ostream &operator<<(std::ostream &os,
                                SimpleString const &simple_string) {
  return os << '+' << simple_string.view() << "\r\n";
}

STRING_NEWTYPE(SimpleError);

static std::ostream &operator<<(std::ostream &os,
                                SimpleError const &simple_error) {
  return os << '-' << simple_error.view() << "\r\n";
}

STRING_NEWTYPE(BulkString);

static std::ostream &operator<<(std::ostream &os,
                                BulkString const &bulk_string) {
  return os << '$' << bulk_string.view().length() << "\r\n"
            << bulk_string.view() << "\r\n";
}

static std::ostream &operator<<(
    std::ostream &os, std::optional<BulkString> const &maybe_bulk_string) {
  if (maybe_bulk_string) {
    return os << *maybe_bulk_string;
  }
  return os << "$-1\r\n";
}

template <>
struct std::hash<BulkString> {
  size_t operator()(BulkString const &bulk_string) const noexcept {
    return std::hash<std::string_view>{}(bulk_string.view());
  }
};

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