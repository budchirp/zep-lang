module;

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

export module zep.compiler.analysis;

import zep.common.context;
import zep.common.diagnostic.collection;
import zep.common.diagnostic.diagnostic;
import zep.common.source;
import zep.common.source.position;
import zep.common.source.span;
import zep.common.target;
import zep.compiler;
import zep.compiler.unit;
import zep.frontend.lexer;
import zep.frontend.node;
import zep.frontend.node.program;
import zep.frontend.parser;
import zep.frontend.sema.checker;
import zep.frontend.sema.context;
import zep.frontend.sema.scope;
import zep.workspace.project;

export class AnalysisResult {
  private:
    std::unique_ptr<Compiler> compiler_instance;
    Module* project_module = nullptr;

    std::unique_ptr<Source> standalone_source;
    std::unique_ptr<Context> standalone_context;
    TargetInfo standalone_target;
    std::unique_ptr<SemaContext> standalone_sema;
    std::optional<Program> standalone_program;

    std::vector<Diagnostic> diagnostics_list;

  public:
    AnalysisResult(std::unique_ptr<Compiler> compiler, Module* module,
                   std::vector<Diagnostic> diagnostics)
        : compiler_instance(std::move(compiler)), project_module(module),
          diagnostics_list(std::move(diagnostics)) {}

    AnalysisResult(std::unique_ptr<Source> source, std::unique_ptr<Context> context,
                   TargetInfo target, std::unique_ptr<SemaContext> sema, Program program,
                   std::vector<Diagnostic> diagnostics)
        : standalone_source(std::move(source)), standalone_context(std::move(context)),
          standalone_target(target), standalone_sema(std::move(sema)),
          standalone_program(std::move(program)), diagnostics_list(std::move(diagnostics)) {}

    explicit AnalysisResult(std::vector<Diagnostic> diagnostics)
        : diagnostics_list(std::move(diagnostics)) {}

    const Program* program() const {
        if (project_module != nullptr) {
            return &project_module->syntax;
        }

        if (standalone_program.has_value()) {
            return &*standalone_program;
        }

        return nullptr;
    }

    const Scope* scope() const {
        if (project_module != nullptr) {
            return project_module->scope;
        }

        if (standalone_sema != nullptr) {
            return standalone_sema->env.root_scope;
        }

        return nullptr;
    }

    const Source* source() const {
        if (project_module != nullptr) {
            return project_module->source;
        }

        if (standalone_source != nullptr) {
            return standalone_source.get();
        }

        return nullptr;
    }

    const std::vector<Diagnostic>& diagnostics() const { return diagnostics_list; }

    bool is_valid() const { return program() != nullptr; }
};

export class CompilerAnalysis {
  private:
    std::unordered_map<std::string, std::string> source_overrides;

    static std::string canonical_key(const std::filesystem::path& path) {
        std::error_code error_code;
        return std::filesystem::weakly_canonical(path, error_code).string();
    }

  public:
    CompilerAnalysis() = default;

    void set_source_override(const std::filesystem::path& path, std::string content) {
        source_overrides.insert_or_assign(canonical_key(path), std::move(content));
    }

    void remove_source_override(const std::filesystem::path& path) {
        source_overrides.erase(canonical_key(path));
    }

    AnalysisResult analyze(const std::filesystem::path& path,
                           std::optional<std::string_view> content = std::nullopt) {
        try {
            auto canonical = canonical_key(path);
            if (content.has_value()) {
                source_overrides.insert_or_assign(canonical, std::string(*content));
            }

            Diagnostics project_diagnostics;
            auto project = Project::open(path, project_diagnostics);
            if (project.has_value()) {
                auto module_path = Project::module_path(project->root_directory, path);
                if (module_path.has_value()) {
                    auto compiler = std::make_unique<Compiler>(TargetInfo());

                    for (const auto& [override_path, override_content] : source_overrides) {
                        compiler->set_source_override(override_path, override_content);
                    }

                    Diagnostics load_diagnostics;
                    auto* module = compiler->load(*project->root_package, std::move(*module_path),
                                                  load_diagnostics);
                    if (module != nullptr) {
                        compiler->check(&load_diagnostics);
                    }

                    std::vector<Diagnostic> file_diagnostics;
                    file_diagnostics.reserve(load_diagnostics.entries.size());
                    for (const auto& entry : load_diagnostics.entries) {
                        if (entry.location.source != nullptr) {
                            auto entry_source_canonical =
                                canonical_key(entry.location.source->name);
                            if (entry_source_canonical == canonical) {
                                file_diagnostics.push_back(entry);
                            }
                        }
                    }

                    return AnalysisResult(std::move(compiler), module, std::move(file_diagnostics));
                }
            }

            std::string source_text;
            if (auto iterator = source_overrides.find(canonical);
                iterator != source_overrides.end()) {
                source_text = iterator->second;
            } else if (std::filesystem::is_regular_file(path)) {
                std::ifstream file(path);
                std::ostringstream buffer;
                buffer << file.rdbuf();
                source_text = buffer.str();
            }

            auto source = std::make_unique<Source>(canonical, std::move(source_text));
            auto context = std::make_unique<Context>(*source);
            TargetInfo target_info;
            auto sema = std::make_unique<SemaContext>(target_info);

            Parser parser(*context, *sema, Lexer(context->source.content));
            auto program = parser.parse();
            if (!context->diagnostics.has_errors()) {
                TypeChecker checker(*context, *sema);
                checker.check(program);
            }

            auto file_diagnostics = context->diagnostics.entries;
            return AnalysisResult(std::move(source), std::move(context), target_info,
                                  std::move(sema), std::move(program), std::move(file_diagnostics));
        } catch (const std::exception& exception) {
            Diagnostics fallback_diagnostics;
            fallback_diagnostics.add_error(Span(::Position(1, 1), ::Position(1, 1)),
                                           exception.what());
            return AnalysisResult(fallback_diagnostics.entries);
        }
    }
};
