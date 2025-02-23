#pragma once
#include <result.hpp>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

namespace shader {
common::result read_spirv(const std::string &path, std::vector<uint32_t> *code);
}
