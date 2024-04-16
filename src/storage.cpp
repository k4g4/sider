#include "storage.h"

#include <mutex>

bool Storage::set(BulkString&& key, BulkString&& value) {
  std::unique_lock guard(data_lock);

  data[std::move(key)] = std::move(value);

  return true;
}

std::optional<BulkString> Storage::get(BulkString const& key) {
  std::shared_lock guard(data_lock);

  if (data.contains(key)) {
    return std::move(BulkString(std::string(data[key].view())));
  } else {
    return std::nullopt;
  }
}