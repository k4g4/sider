#include "storage.h"

#include <charconv>
#include <chrono>
#include <mutex>

bool Storage::set(BulkString&& key, BulkString&& value,
                  std::optional<BulkString> const& px) {
  std::unique_lock guard(data_lock);
  decltype(DataCell::expiry) expiry;

  if (px) {
    size_t ms;
    auto res = std::from_chars(px->view().begin(), px->view().end(), ms);
    if (res.ec == std::errc::invalid_argument) {
      return false;
    }
    expiry = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
  }

  data[std::move(key)] = DataCell{.value = std::move(value), .expiry = expiry};

  return true;
}

std::optional<BulkString> Storage::get(BulkString const& key) {
  std::shared_lock guard(data_lock);

  if (data.contains(key)) {
    const auto& data_cell = data[key];

    if (!data_cell.expiry) {
      return std::move(BulkString(std::string(data_cell.value.view())));
    }

    if (std::chrono::steady_clock::now() < *data_cell.expiry) {
      return std::move(BulkString(std::string(data_cell.value.view())));
    } else {
      data.erase(key);
    }
  }
  return std::nullopt;
}