#include <fstream>
#include <shader.hpp>

using namespace common;

namespace shader {
result read_spirv(const std::string &path, std::vector<uint32_t> *code) {
  if (!code)
    return result::domain_error;

  std::ifstream file(path, std::ios::ate | std::ios::binary);
  if (!file.is_open())
    return result::access_error;

  const std::size_t text_size = file.tellg();
  const auto r = text_size % sizeof(uint32_t);
  const std::size_t padded_text{text_size + (r ? sizeof(uint32_t) - r : 0)};
  const std::size_t code_size = padded_text / sizeof(uint32_t);
  code->resize(code_size);

  std::vector<char> text{};
  text.resize(padded_text);

  file.seekg(0);
  file.read(text.data(), text_size);

  auto data = reinterpret_cast<const uint32_t *>(text.data());
  for (std::size_t i = 0; i < code->size(); ++i)
    (*code)[i] = *(data + i);

  return result::success;
}
} // namespace shader
