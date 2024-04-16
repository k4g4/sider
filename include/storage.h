#pragma once

#include <chrono>
#include <optional>
#include <shared_mutex>
#include <unordered_map>

#include "elements.h"

struct DataCell {
  BulkString value;
  std::optional<std::chrono::time_point<std::chrono::steady_clock>> expiry;
};

class Storage {
  std::unordered_map<BulkString, DataCell> data;
  std::shared_mutex data_lock;

 public:
  bool set(BulkString&& key, BulkString&& value,
           std::optional<BulkString> const& px);

  std::optional<BulkString> get(BulkString const& key);
};