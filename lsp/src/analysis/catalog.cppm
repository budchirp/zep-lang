module;

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module zep.lsp.analysis.catalog;

import zep.common.source.position;
import zep.common.source.span;
import zep.lsp.analysis.types;
import zep.workspace.package;
import zep.workspace.project;

class CatalogExport {
  public:
    std::string name;
    CompletionKind::Type kind;

    CatalogExport(std::string name, CompletionKind::Type kind)
        : name(std::move(name)), kind(kind) {}
};

class CatalogModule {
  public:
    std::string name;
    std::filesystem::path path;
    std::vector<CatalogExport> exports;

    CatalogModule(std::string name, std::filesystem::path path,
                  std::vector<CatalogExport> exports)
        : name(std::move(name)), path(std::move(path)), exports(std::move(exports)) {}
};

export class ModuleCatalog {
  private:
    std::vector<CatalogModule> modules;

    static std::string trim(std::string_view value) {
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
            value.remove_prefix(1);
        }
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
            value.remove_suffix(1);
        }
        return std::string(value);
    }

    static std::string identifier_after(std::string_view line, std::string_view prefix) {
        if (!line.starts_with(prefix)) {
            return {};
        }

        line.remove_prefix(prefix.length());
        while (!line.empty() && std::isspace(static_cast<unsigned char>(line.front())) != 0) {
            line.remove_prefix(1);
        }

        std::size_t length = 0;
        while (length < line.length() &&
               (std::isalnum(static_cast<unsigned char>(line[length])) != 0 ||
                line[length] == '_' || line[length] == '~')) {
            ++length;
        }
        return std::string(line.substr(0, length));
    }

    static std::vector<CatalogExport> exports(const std::filesystem::path& path) {
        std::ifstream input(path);
        std::vector<CatalogExport> result;
        if (!input.is_open()) {
            return result;
        }

        std::string line;
        while (std::getline(input, line)) {
            auto source = trim(line);
            const auto add = [&](std::string_view prefix, CompletionKind::Type kind) {
                auto name = identifier_after(source, prefix);
                if (!name.empty()) {
                    result.emplace_back(std::move(name), kind);
                    return true;
                }
                return false;
            };

            if (add("public fn ", CompletionKind::Type::Function) ||
                add("public struct ", CompletionKind::Type::Struct) ||
                add("public enum ", CompletionKind::Type::Enum) ||
                add("public interface ", CompletionKind::Type::Interface) ||
                add("public type ", CompletionKind::Type::TypeParameter) ||
                add("public const ", CompletionKind::Type::Constant) ||
                add("public var mut ", CompletionKind::Type::Variable) ||
                add("public var ", CompletionKind::Type::Variable)) {
                continue;
            }
        }

        return result;
    }

    void add_package(const Package& package, bool root_package, std::set<std::string>& visited) {
        std::error_code error_code;
        auto package_path = std::filesystem::weakly_canonical(package.root, error_code).string();
        if (!visited.insert(package_path).second ||
            !std::filesystem::is_directory(package.source_directory)) {
            return;
        }

        if (root_package && package.manifest.name == "standalone") {
            for (auto* dependency : package.dependencies) {
                if (dependency != nullptr) {
                    add_package(*dependency, false, visited);
                }
            }
            return;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                 package.source_directory, std::filesystem::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".zep") {
                continue;
            }

            auto relative = entry.path().lexically_relative(package.source_directory);
            relative.replace_extension();
            std::vector<std::string> segments;
            for (const auto& part : relative) {
                segments.push_back(part.string());
            }
            if (!segments.empty() && segments.back() == "index") {
                segments.pop_back();
            }
            if (!root_package) {
                segments.insert(segments.begin(), package.manifest.name);
            }
            if (segments.empty()) {
                segments.push_back(package.manifest.name);
            }

            std::string name;
            for (const auto& segment : segments) {
                if (!name.empty()) {
                    name += ".";
                }
                name += segment;
            }
            modules.emplace_back(std::move(name), entry.path(), exports(entry.path()));
        }

        for (auto* dependency : package.dependencies) {
            if (dependency != nullptr) {
                add_package(*dependency, false, visited);
            }
        }
    }

    static std::vector<std::string> split(std::string_view value) {
        std::vector<std::string> result;
        std::size_t begin = 0;
        while (begin <= value.length()) {
            auto end = value.find('.', begin);
            result.emplace_back(value.substr(begin, end == std::string_view::npos
                                                        ? value.length() - begin
                                                        : end - begin));
            if (end == std::string_view::npos) {
                break;
            }
            begin = end + 1;
        }
        return result;
    }

  public:
    explicit ModuleCatalog(const Project& project) {
        std::set<std::string> visited;
        add_package(*project.root_package, true, visited);
        std::ranges::sort(modules, {}, &CatalogModule::name);
    }

    std::optional<std::vector<Completion>> complete(std::string_view content, Position position,
                                                    const std::filesystem::path& current_path) const {
        std::size_t line = 1;
        std::size_t line_start = 0;
        for (std::size_t index = 0; index < content.length() && line < position.line; ++index) {
            if (content[index] == '\n') {
                ++line;
                line_start = index + 1;
            }
        }
        if (line != position.line) {
            return std::nullopt;
        }

        auto cursor = std::min(line_start + position.column - 1, content.length());
        auto line_prefix = content.substr(line_start, cursor - line_start);
        while (!line_prefix.empty() &&
               std::isspace(static_cast<unsigned char>(line_prefix.front())) != 0) {
            line_prefix.remove_prefix(1);
        }
        if (line_prefix != "import" && !line_prefix.starts_with("import ")) {
            return std::nullopt;
        }
        auto prefix = line_prefix == "import"
                          ? std::string()
                          : trim(std::string_view(line_prefix).substr(7));
        if (prefix.find(" as ") != std::string::npos) {
            return std::vector<Completion>();
        }

        auto parts = split(prefix);
        auto partial = parts.empty() ? std::string() : parts.back();
        if (!parts.empty()) {
            parts.pop_back();
        }

        std::string base;
        for (const auto& part : parts) {
            if (!base.empty()) {
                base += ".";
            }
            base += part;
        }

        auto replacement_start = position.column - partial.length();
        auto replacement = Span(Position(position.line, replacement_start),
                                Position(position.line, position.column));
        std::set<std::string> seen;
        std::set<std::string> imported;
        std::istringstream lines{std::string(content)};
        std::string source_line;
        while (std::getline(lines, source_line)) {
            auto value = trim(source_line);
            if (!value.starts_with("import ")) {
                continue;
            }
            value = trim(std::string_view(value).substr(7));
            if (auto alias = value.find(" as "); alias != std::string::npos) {
                value.erase(alias);
            }
            imported.insert(std::move(value));
        }
        std::vector<Completion> result;
        result.reserve(32);

        for (const auto& module : modules) {
            std::error_code error_code;
            if (std::filesystem::equivalent(module.path, current_path, error_code) && !error_code) {
                continue;
            }

            if (module.name == base) {
                for (const auto& symbol : module.exports) {
                    if (!imported.contains(module.name + "." + symbol.name) &&
                        symbol.name.starts_with(partial) &&
                        seen.insert("symbol:" + symbol.name).second) {
                        result.emplace_back(symbol.name, symbol.kind, module.name, replacement,
                                            symbol.name, "1_" + symbol.name);
                    }
                }
            }

            auto remainder = module.name;
            if (!base.empty()) {
                auto required = base + ".";
                if (!remainder.starts_with(required)) {
                    continue;
                }
                remainder.erase(0, required.length());
            }

            auto separator = remainder.find('.');
            auto segment = remainder.substr(0, separator);
            if (segment.starts_with(partial) && seen.insert("module:" + segment).second) {
                result.emplace_back(segment, CompletionKind::Type::Module, "module", replacement,
                                    segment, "0_" + segment);
            }
        }

        std::ranges::sort(result, [](const Completion& left, const Completion& right) {
            return left.sort_key < right.sort_key;
        });
        return result;
    }
};
