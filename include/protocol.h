#pragma once

#include <string>

#include "shared.h"
#include "storage.h"

void server_transact(Storage &storage, buffer const &in, std::string &out);

std::string client_parse(buffer const &in);