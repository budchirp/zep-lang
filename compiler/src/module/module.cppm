module;

#include <string>
#include <utility>
#include <vector>

export module zep.compiler.unit;

import zep.frontend.node;
import zep.frontend.node.program;
import zep.common.source;
import zep.workspace.module_path;
import zep.workspace.package;
import zep.frontend.sema.scope;

export class Module;

export class ModuleImport {
  public:
    Module* importing;
    Module* imported;
    ImportStatement* syntax;
    std::string symbol;
    std::string local_name;

    ModuleImport(Module* importing, Module* imported, ImportStatement* syntax)
        : importing(importing), imported(imported), syntax(syntax),
          symbol(syntax->path.back()->name),
          local_name(syntax->alias.empty() ? symbol : syntax->alias) {}
};

export class Module {
  public:
    Package* owner;
    ModulePath path;
    Source* source;
    Program syntax;
    Scope* scope;
    std::vector<ModuleImport*> imports;

    Module(Package* owner, ModulePath path, Source* source, Program syntax, Scope* scope)
        : owner(owner), path(std::move(path)), source(source), syntax(std::move(syntax)),
          scope(scope) {}
};
