#include "storage.h"

#include <mutex>

bool Storage::set(BulkString&& key, BulkString&& value,
                  std::optional<BulkString> const& px) {
  std::unique_lock guard(data_lock);
  auto expiry = std::nullopt;

  data[std::move(key)] = DataCell{.value = std::move(value), .expiry = expiry};

  return true;
}

std::optional<BulkString> Storage::get(BulkString const& key) {
  std::shared_lock guard(data_lock);

  if (data.contains(key)) {
    return std::move(BulkString(std::string(data[key].value.view())));
  } else {
    return std::nullopt;
  }
}