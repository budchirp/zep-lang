module;

#include <algorithm>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

export module zep.compiler.graph;

import zep.common.context;
import zep.common.diagnostic.collection;
import zep.common.source.manager;
import zep.common.source;
import zep.common.source.span;
import zep.common.source.position;
import zep.frontend.lexer;
import zep.frontend.node;
import zep.frontend.parser;
import zep.frontend.sema.context;
import zep.frontend.sema.scope;
import zep.workspace.module_path;
import zep.workspace.package;
import zep.compiler.unit;
import zep.compiler.resolver;

export class ModuleGraph {
  private:
    SourceManager source_manager;
    ModuleResolver resolver;
    std::vector<std::unique_ptr<Module>> module_storage;
    std::vector<std::unique_ptr<ModuleImport>> edges;
    SemaContext& sema;
    std::map<std::string, Module*> identities;
    std::map<std::string, Module*> physical_sources;
    std::vector<Module*> order;
    std::vector<Module*> active_modules;
    std::vector<ModuleImport*> active_edges;

    static std::string identity(const Package& package, const ModulePath& path) {
        return package.manifest.name + ":" + path.string();
    }

    static ModulePath import_path(const ImportStatement& statement) {
        std::vector<std::string> segments;
        segments.reserve(statement.path.size() - 1);
        for (std::size_t index = 0; index + 1 < statement.path.size(); ++index) {
            segments.push_back(statement.path[index]->name);
        }
        return ModulePath(std::move(segments));
    }

    Module* load_module(Package& package, ModulePath path, const std::filesystem::path& source_path,
                        Diagnostics& diagnostics) {
        auto module_identity = identity(package, path);
        if (auto existing = identities.find(module_identity); existing != identities.end()) {
            return existing->second;
        }

        std::error_code error_code;
        auto canonical_path = std::filesystem::weakly_canonical(source_path, error_code);
        if (error_code) {
            auto& source = source_manager.add(source_path.string(), "");
            diagnostics.add_error(source, Span::from_position(Position()),
                                  "could not canonicalize module source");
            return nullptr;
        }

        auto canonical = canonical_path.string();
        if (auto existing = physical_sources.find(canonical); existing != physical_sources.end()) {
            if (existing->second->owner != &package || existing->second->path != path) {
                Source& source = *existing->second->source;
                diagnostics.add_error(source, Span::from_position(Position()),
                                      "source is claimed by multiple module identities");
                return nullptr;
            }
            return existing->second;
        }

        auto loaded = source_manager.load(source_path);
        if (!loaded.has_value()) {
            auto& source = source_manager.add(source_path.string(), "");
            diagnostics.add_error(source, Span::from_position(Position()),
                                  "could not load module source");
            return nullptr;
        }

        Context context(**loaded);
        Parser parser(context, sema, Lexer(context.source.content));
        auto syntax = parser.parse();
        if (context.diagnostics.has_errors()) {
            for (auto& entry : context.diagnostics.entries) {
                diagnostics.entries.push_back(std::move(entry));
            }
        }

        auto* scope = sema.env.scopes.create<Scope>(Scope::Kind::Type::Module, path.string(),
                                                    sema.env.root_scope);
        auto module =
            std::make_unique<Module>(&package, std::move(path), *loaded, std::move(syntax), scope);
        auto* result = module.get();
        identities.emplace(std::move(module_identity), result);
        physical_sources.emplace(canonical, result);
        module_storage.push_back(std::move(module));
        return result;
    }

    Module* visit(Package& package, ModulePath path, const std::filesystem::path& source_path,
                  Diagnostics& diagnostics) {
        auto* module = load_module(package, std::move(path), source_path, diagnostics);
        if (module == nullptr) {
            return nullptr;
        }

        if (std::find(active_modules.begin(), active_modules.end(), module) !=
            active_modules.end()) {
            auto position = std::find(active_modules.begin(), active_modules.end(), module);
            auto offset = static_cast<std::size_t>(position - active_modules.begin());
            std::string chain;
            for (std::size_t index = offset; index < active_modules.size(); ++index) {
                if (!chain.empty()) {
                    chain += " -> ";
                }
                chain += active_modules[index]->path.string();
            }
            chain += " -> " + module->path.string();
            auto* edge = active_edges.back();
            diagnostics.add_error(*edge->importing->source, edge->syntax->span,
                                  "module import cycle: " + chain);
            return module;
        }

        if (std::find(order.begin(), order.end(), module) != order.end()) {
            return module;
        }

        active_modules.push_back(module);
        for (auto* statement : module->syntax.statements) {
            auto* import = statement->as<ImportStatement>();
            if (import == nullptr) {
                continue;
            }

            auto requested = import_path(*import);
            auto resolved = resolver.resolve(*module->owner, std::move(requested));
            if (!resolved.has_value()) {
                diagnostics.add_error(*module->source, import->span,
                                      "could not resolve module import");
                continue;
            }

            auto* imported =
                load_module(*resolved->owner, resolved->path, resolved->source_path, diagnostics);
            if (imported == nullptr) {
                continue;
            }

            auto edge = std::make_unique<ModuleImport>(module, imported, import);
            auto* edge_ptr = edge.get();
            edges.push_back(std::move(edge));
            module->imports.push_back(edge_ptr);
            active_edges.push_back(edge_ptr);
            if (visit(*resolved->owner, resolved->path, resolved->source_path, diagnostics) ==
                nullptr) {
                active_edges.pop_back();
                active_modules.pop_back();
                return nullptr;
            }
            active_edges.pop_back();
        }
        active_modules.pop_back();
        order.push_back(module);
        return module;
    }

  public:
    explicit ModuleGraph(SemaContext& sema) : sema(sema) {}

    const std::vector<std::unique_ptr<Module>>& modules() const { return module_storage; }
    const std::vector<Module*>& ordering() const { return order; }
    SourceManager& sources() { return source_manager; }

    Module* load(Package& package, ModulePath path, Diagnostics& diagnostics) {
        auto builtins = resolver.resolve(package, ModulePath::from_string("std.builtins"));
        if (builtins.has_value() && visit(*builtins->owner, builtins->path, builtins->source_path,
                                          diagnostics) == nullptr) {
            return nullptr;
        }

        auto resolved = resolver.resolve(package, std::move(path));
        if (!resolved.has_value()) {
            auto& source = source_manager.add((package.root / "src").string(), "");
            diagnostics.add_error(source, Span::from_position(Position()),
                                  "could not resolve requested module");
            return nullptr;
        }
        return visit(*resolved->owner, std::move(resolved->path), resolved->source_path,
                     diagnostics);
    }
};
