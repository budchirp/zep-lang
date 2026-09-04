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
import zep.common.logger;
import zep.common.source.manager;
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
    CompilerChecker checker;

  public:
    explicit Compiler(TargetInfo target = TargetInfo())
        : sema(target), graph(sema), checker(sema) {}

    Module* load(Package& package, ModulePath entry, Diagnostics& diagnostics) {
        return graph.load(package, std::move(entry), diagnostics);
    }

    bool check(Diagnostics* diagnostics = nullptr) {
        CompilerChecker checker(sema);
        return checker.check(graph, diagnostics);
    }

    std::shared_ptr<HIRProgram> lower(Module& module) {
        std::vector<const Program*> programs;
        programs.reserve(graph.ordering().size());
        for (auto* item : graph.ordering()) {
            programs.push_back(&item->syntax);
        }
        Context context(*module.source);
        HIRLowerer lowerer(context, sema);
        auto program = lowerer.lower_module(module.syntax, module.scope, programs);
        if (lowerer.diagnostics.has_errors()) {
            lowerer.diagnostics.print();
            return nullptr;
        }

        return program;
    }

    const std::vector<Module*>& ordering() const { return graph.ordering(); }
    SourceManager& sources() { return graph.sources(); }
    void set_source_override(const std::filesystem::path& path, std::string content) {
        graph.sources().add_override(path, std::move(content));
    }
};
