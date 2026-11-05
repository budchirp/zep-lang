module;

#include <utility>

export module zep.frontend.sema.context;

import zep.frontend.sema.scopes.environment;
import zep.frontend.node;
import zep.frontend.sema.types;
import zep.frontend.sema.types.facades;
import zep.frontend.sema.lifecycle;
import zep.frontend.sema.expression.binding;
import zep.common.target;

export class SemaContext {
  public:
    TypeArena types;

    NodeArena nodes;

    FacadeRegistry facades;

    TargetInfo target;

    PrimitiveTypes primitives;

    SymbolEnvironment env;

    Lifecycle lifecycle;
    Binding binding;

    explicit SemaContext(TargetInfo target = TargetInfo())
        : target(std::move(target)), primitives(types), env(primitives.entries),
          binding(nodes, lifecycle) {}
};
