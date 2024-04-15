#pragma once

#include <string>

#include "shared.h"

void server_transact(buffer const &in, std::string &out);

std::string client_parse(buffer const &in);