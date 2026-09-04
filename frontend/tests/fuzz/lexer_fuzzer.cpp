#include <cstddef>
#include <cstdint>
#include <string_view>

import zep.frontend.test.harness;

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    std::string_view input(reinterpret_cast<const char*>(data), size);
    static_cast<void>(lex_all(std::string(input)));

    return 0;
}
