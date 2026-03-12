module;

#include <string_view>

export module zep.common.logger.colors;

export namespace colors {

constexpr std::string_view reset = "\033[0m";
constexpr std::string_view bold = "\033[1m";
constexpr std::string_view dim = "\033[2m";

constexpr std::string_view red = "\033[31m";
constexpr std::string_view green = "\033[32m";
constexpr std::string_view yellow = "\033[33m";
constexpr std::string_view blue = "\033[34m";
constexpr std::string_view magenta = "\033[35m";
constexpr std::string_view cyan = "\033[36m";
constexpr std::string_view white = "\033[37m";
constexpr std::string_view gray = "\033[90m";

constexpr std::string_view bold_red = "\033[1;31m";
constexpr std::string_view bold_yellow = "\033[1;33m";
constexpr std::string_view bold_blue = "\033[1;34m";
constexpr std::string_view bold_cyan = "\033[1;36m";
constexpr std::string_view bold_white = "\033[1;37m";

} // namespace colors
