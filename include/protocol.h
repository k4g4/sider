#pragma once

#include <string>

#include "shared.h"

void server_transact(buffer const &in, buffer &out, size_t &out_size);

std::string client_parse(buffer const &in);