module;

#include <string>
#include <vector>

export module zep.compiler.checker;

import zep.common.context;
import zep.common.diagnostic.collection;
import zep.common.diagnostic.diagnostic;
import zep.compiler.graph;
import zep.compiler.unit;
import zep.frontend.node;
import zep.frontend.sema.context;
import zep.frontend.sema.checker;
import zep.frontend.sema.kind;
import zep.frontend.sema.scope;
import zep.frontend.sema.type;

export class CompilerChecker {
  private:
    SemaContext& sema;

    void import_builtin_types(Scope& destination, Scope& source) {
        for (const auto& [name, symbol] : source.local_types()) {
            if (source.find_exported_type(name) != nullptr && !destination.has_local_type(name)) {
                destination.define_type(name, symbol);
            }
        }
    }

    void import_symbol(Scope& destination, Scope& source, const ModuleImport& edge,
                       Context& context) {
        const auto name = edge.symbol();
        const auto local_name = edge.local_name();

        if (const auto* symbol = source.find_exported_type(name); symbol != nullptr) {
            if (!destination.define_type(local_name, source.local_types().at(name))) {
                context.diagnostics.add_error(edge.syntax->span,
                                              "duplicate imported type '" + local_name + "'");
            }
            return;
        }

        if (source.find_exported_var(name) != nullptr) {
            if (!destination.define_var(local_name, source.local_variables().at(name))) {
                context.diagnostics.add_error(edge.syntax->span,
                                              "duplicate imported value '" + local_name + "'");
            }
            return;
        }

        const auto functions = source.find_exported_function_overloads(name);
        if (!functions.empty()) {
            for (auto* function : functions) {
                destination.define_function(local_name, function,
                                            sema.env.overloads.create<OverloadSet>());
            }
            return;
        }

        context.diagnostics.add_error(edge.syntax->span, "module does not export '" + name + "'");
    }

    static bool report(Context& context) {
        if (!context.diagnostics.has_errors()) {
            return true;
        }
        context.diagnostics.print();
        return false;
    }

  public:
    explicit CompilerChecker(SemaContext& sema) : sema(sema) {}

    bool check(ModuleGraph& graph, Diagnostics* diagnostics = nullptr) {
        Module* builtins = nullptr;
        for (auto* module : graph.ordering()) {
            if (module->owner->manifest.name == "std" && module->path.string() == "builtins") {
                builtins = module;
                break;
            }
        }

        for (auto* module : graph.ordering()) {
            Context context(*module->source);
            if (builtins != nullptr && module != builtins) {
                import_builtin_types(*module->scope, *builtins->scope);
            }

            for (auto* edge : module->imports) {
                import_symbol(*module->scope, *edge->imported->scope, *edge, context);
            }
            ScopeGuard active_scope(sema.env.current_scope, module->scope);
            TypeChecker checker(context, sema);
            checker.check(module->syntax);
            if (diagnostics != nullptr) {
                for (const auto& entry : context.diagnostics.entries) {
                    diagnostics->entries.push_back(entry);
                }
            } else if (!report(context)) {
                return false;
            }
        }
        return diagnostics != nullptr ? !diagnostics->has_errors() : true;
    }
};
