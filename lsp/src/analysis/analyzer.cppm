module;

#include <algorithm>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

export module zep.lsp.analysis.analyzer;

import zep.common.diagnostic.collection;
import zep.common.diagnostic.diagnostic;
import zep.common.source.position;
import zep.common.source.span;
import zep.common.target;
import zep.compiler;
import zep.compiler.unit;
import zep.lsp.analysis.snapshot;
import zep.lsp.analysis.types;
import zep.workspace.manifest;
import zep.workspace.project;
import zep.workspace.toolchain;

export class Analyzer {
  private:
    Toolchain toolchain;
    std::unordered_map<std::string, std::string> overlays;

    static std::string canonical(const std::filesystem::path& path) {
        std::error_code error_code;
        auto result = std::filesystem::weakly_canonical(path, error_code);
        return error_code ? std::filesystem::absolute(path).lexically_normal().string()
                          : result.string();
    }

    static std::size_t offset(std::string_view content, Position position) {
        std::size_t current_line = 1;
        std::size_t current_column = 1;
        for (std::size_t index = 0; index < content.size(); ++index) {
            if (current_line == position.line && current_column == position.column) {
                return index;
            }

            if (content[index] == '\n') {
                ++current_line;
                current_column = 1;
            } else {
                ++current_column;
            }
        }

        return content.size();
    }

  public:
    explicit Analyzer(Toolchain toolchain = Toolchain::discover())
        : toolchain(std::move(toolchain)) {}

    explicit Analyzer(std::filesystem::path standard_library) : toolchain(Toolchain::discover()) {
        toolchain.standard_library = std::move(standard_library);
    }

    void overlay(const std::filesystem::path& path, std::string content) {
        overlays.insert_or_assign(canonical(path), std::move(content));
    }

    void remove_overlay(const std::filesystem::path& path) {
        overlays.erase(canonical(path));
    }

    std::vector<DocumentSymbol> workspace_symbols(const std::filesystem::path& path,
                                                  const std::string& query) {
        Diagnostics diagnostics;
        auto project = Project::open(path, diagnostics, toolchain);
        if (!project.has_value()) {
            return {};
        }

        std::vector<DocumentSymbol> result;
        result.reserve(200);
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(project->root_package->source_directory)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".zep") {
                continue;
            }

            auto analysis = analyze(entry.path());
            for (auto& symbol : analysis.document_symbols()) {
                if (!query.empty() && !symbol.name.starts_with(query) &&
                    symbol.name.find(query) == std::string::npos) {
                    continue;
                }

                result.push_back(std::move(symbol));
                if (result.size() == 200) {
                    return result;
                }
            }
        }

        std::ranges::sort(result, [&query](const DocumentSymbol& left,
                                          const DocumentSymbol& right) {
            auto left_prefix = left.name.starts_with(query);
            auto right_prefix = right.name.starts_with(query);
            return left_prefix != right_prefix ? left_prefix : left.name < right.name;
        });

        return result;
    }

    std::vector<Completion> complete(const std::filesystem::path& path, Position position) {
        auto canonical_path = canonical(path);
        if (!overlays.contains(canonical_path)) {
            return analyze(path).complete(position);
        }

        auto original_content = overlays.at(canonical_path);
        auto recovered_content = original_content;
        auto insertion_offset = offset(recovered_content, position);
        if (insertion_offset > 0 && recovered_content[insertion_offset - 1] == '.') {
            recovered_content.insert(insertion_offset, "__zep_member_dummy;");
        }

        auto analysis = analyze(path, recovered_content);
        overlays.insert_or_assign(canonical_path, std::move(original_content));
        return analysis.complete(position);
    }

    Analysis analyze(const std::filesystem::path& path,
                     std::optional<std::string_view> content = std::nullopt) {
        try {
            auto canonical_path = canonical(path);
            if (content.has_value()) {
                overlays.insert_or_assign(canonical_path, std::string(*content));
            }

            Diagnostics diagnostics;
            auto project = Project::open(path, diagnostics, toolchain);
            if (!project.has_value()) {
                project = Project::single_file(path, toolchain);
            }

            if (project->packages.find("std") == nullptr && !toolchain.standard_library.empty()) {
                ManifestReader reader;
                auto manifest = reader.read(toolchain.standard_library / "zep.json");
                if (manifest.has_value()) {
                    auto& standard_library =
                        project->packages.add(std::move(*manifest), toolchain.standard_library,
                                              toolchain.standard_library / "src");
                    project->root_package->dependencies.push_back(&standard_library);
                }
            }

            auto module_path = project->module(path);
            if (!module_path.has_value()) {
                diagnostics.add_error(Span::from_position(Position(1, 1)),
                                      "source is outside the project source directory");
                return Analysis(std::move(diagnostics.entries));
            }

            auto compiler = std::make_unique<Compiler>(TargetInfo());
            for (const auto& [overlay_path, overlay_content] : overlays) {
                compiler->overlay(overlay_path, overlay_content);
            }

            auto* module = compiler->analyze(*project->root_package, std::move(*module_path),
                                             diagnostics, path);

            std::vector<Diagnostic> file_diagnostics;
            file_diagnostics.reserve(diagnostics.entries.size());
            for (const auto& diagnostic : diagnostics.entries) {
                if (diagnostic.location.source == nullptr ||
                    canonical(diagnostic.location.source->name) == canonical_path) {
                    file_diagnostics.push_back(diagnostic);
                }
            }

            auto source_content = overlays.contains(canonical_path) ? overlays.at(canonical_path)
                                  : module != nullptr               ? module->source->content
                                                                    : std::string();
            return Analysis(std::move(*project), std::move(compiler), module,
                            std::move(source_content), std::move(file_diagnostics));
        } catch (const std::exception& exception) {
            Diagnostics diagnostics;
            diagnostics.add_error(Span::from_position(Position(1, 1)), exception.what());
            return Analysis(std::move(diagnostics.entries));
        }
    }
};
