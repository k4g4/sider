#pragma once

#include <shared_mutex>
#include <unordered_map>

#include "elements.h"

class Storage {
  std::unordered_map<BulkString, BulkString> data;
  std::shared_mutex data_lock;

 public:
  bool set(BulkString&& key, BulkString&& value);

  std::optional<BulkString> get(BulkString const& key);
};