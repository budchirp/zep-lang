module;

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

export module zep.compiler;

import zep.compiler.checker;
import zep.compiler.graph;
import zep.compiler.unit;
import zep.common.context;
import zep.common.diagnostic.collection;
import zep.common.target;
import zep.frontend.sema.context;
import zep.frontend.node.program;
import zep.compiler.lowering;
import zep.hir.program;
import zep.workspace.module_path;
import zep.workspace.package;

export class Compiler {
  private:
    SemaContext sema;
    ModuleGraph graph;

  public:
    explicit Compiler(TargetInfo target = TargetInfo()) : sema(target), graph(sema) {}

    Module* analyze(Package& package, ModulePath entry, Diagnostics& diagnostics,
                    std::filesystem::path source_path = {}) {
        auto* module = graph.load(package, std::move(entry), diagnostics, std::move(source_path));
        if (module == nullptr || diagnostics.has_errors()) {
            return module;
        }

        CompilerChecker checker(sema);
        checker.check(graph, diagnostics);
        return module;
    }

    std::shared_ptr<HIRProgram> lower(Module& module, Diagnostics& diagnostics) {
        std::vector<const Program*> programs;
        programs.reserve(graph.modules().size());
        for (auto* item : graph.modules()) {
            programs.push_back(&item->syntax);
        }
        Context context(*module.source);
        HIRLowerer lowerer(context, sema);
        auto program = lowerer.lower_module(module.syntax, module.scope, programs);
        if (lowerer.diagnostics.has_errors()) {
            for (const auto& entry : lowerer.diagnostics.entries) {
                diagnostics.entries.push_back(entry);
            }
            return nullptr;
        }

        return program;
    }

    const std::vector<Module*>& modules() const { return graph.modules(); }

    void overlay(const std::filesystem::path& path, std::string content) {
        graph.overlay(path, std::move(content));
    }
};
