#include <cstddef>
#include <cstdint>
#include <string>

import zep.frontend.test.harness;

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    FrontendHarness harness(std::string(reinterpret_cast<const char*>(data), size));
    static_cast<void>(harness);

    return 0;
}
