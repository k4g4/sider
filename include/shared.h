#pragma once

#include <stddef.h>

#include <array>
#include <cstdint>

const uint16_t PORT = 6379;
const size_t BUF_LEN = 2048;

using buffer = std::array<char, BUF_LEN>;