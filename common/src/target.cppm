module;

#include <cctype>
#include <cstdint>
#include <string>
#include <utility>

export module zep.common.target;

namespace {

std::string lower(std::string value) {
    for (auto& character : value) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }

    return value;
}

} // namespace

export class TargetArch {
  public:
    class Kind {
      public:
        enum class Type : std::uint8_t {
            Unknown,
            Amd64,
            Aarch64,
        };
    };

    static Kind::Type from(std::string triple) {
        auto separator = triple.find('-');

        std::string value;
        if (separator == std::string::npos) {
            value = std::move(triple);
        } else {
            value = triple.substr(0, separator);
        }

        auto normalized = lower(std::move(value));

        if (normalized == "x86_64") {
            return Kind::Type::Amd64;
        }

        if (normalized == "aarch64" || normalized == "arm64") {
            return Kind::Type::Aarch64;
        }

        return Kind::Type::Unknown;
    }

    static std::string to_string(Kind::Type type) {
        switch (type) {
        case Kind::Type::Amd64:
            return std::string("x86_64");
        case Kind::Type::Aarch64:
            return std::string("aarch64");
        case Kind::Type::Unknown:
            return std::string("unknown");
        }

        return std::string("unknown");
    }
};

export class TargetOS {
  public:
    class Kind {
      public:
        enum class Type : std::uint8_t {
            Unknown,
            Linux,
            Macos,
        };
    };

    static Kind::Type from(std::string triple) {
        auto normalized = lower(std::move(triple));

        if (normalized.contains("darwin") || normalized == "macos") {
            return Kind::Type::Macos;
        }

        if (normalized.contains("linux")) {
            return Kind::Type::Linux;
        }

        if (normalized.contains("unknown")) {
            return Kind::Type::Unknown;
        }

        return Kind::Type::Unknown;
    }

    static std::string to_string(Kind::Type type) {
        switch (type) {
        case Kind::Type::Linux:
            return std::string("linux");
        case Kind::Type::Macos:
            return std::string("macos");
        case Kind::Type::Unknown:
            return std::string("unknown");
        }

        return std::string("unknown");
    }
};

export class TargetInfo {
  private:
    static TargetArch::Kind::Type host_arch() {
#if defined(__aarch64__) || defined(_M_ARM64)
        return TargetArch::Kind::Type::Aarch64;
#else
        return TargetArch::Kind::Type::Amd64;
#endif
    }

    static TargetOS::Kind::Type host_os() {
#if defined(__APPLE__)
        return TargetOS::Kind::Type::Macos;
#else
        return TargetOS::Kind::Type::Linux;
#endif
    }

  public:
    std::string triple;

    TargetArch::Kind::Type arch;
    TargetOS::Kind::Type os;

    TargetInfo() : TargetInfo(host_triple()) {}

    explicit TargetInfo(std::string triple)
        : triple(std::move(triple)), arch(TargetArch::from(this->triple)),
          os(TargetOS::from(this->triple)) {}

    static std::string triple_from(TargetArch::Kind::Type arch, TargetOS::Kind::Type os) {
        if (os == TargetOS::Kind::Type::Macos) {
            if (arch == TargetArch::Kind::Type::Aarch64) {
                return "aarch64-apple-darwin";
            }
        }

        if (os == TargetOS::Kind::Type::Linux) {
            if (arch == TargetArch::Kind::Type::Aarch64) {
                return "aarch64-unknown-linux-gnu";
            }

            if (arch == TargetArch::Kind::Type::Amd64) {
                return "x86_64-unknown-linux-gnu";
            }
        }

        if (os == TargetOS::Kind::Type::Unknown) {
            if (arch == TargetArch::Kind::Type::Aarch64) {
                return "aarch64-unknown-none";
            }

            if (arch == TargetArch::Kind::Type::Amd64) {
                return "x86_64-unknown-none";
            }
        }

        return "";
    }

    static std::string host_triple() {
        auto arch = host_arch();
        auto os = host_os();

        return triple_from(arch, os);
    }

    static std::string supported_triples_text() {
        return "x86_64-unknown-linux-gnu, aarch64-unknown-linux-gnu, "
               "aarch64-apple-darwin";
    }

    static std::string target_option_description() {
        return "Target triple. Valid: " + supported_triples_text();
    }

    bool is_supported() const { return arch != TargetArch::Kind::Type::Unknown; }

    std::string path_name() const {
        std::string result;
        result.reserve(triple.size());

        for (auto character : triple) {
            if (std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '-' ||
                character == '_' || character == '.') {
                result.push_back(character);
            } else {
                result.push_back('_');
            }
        }

        return result;
    }
};
