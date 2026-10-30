module;

#include <utility>

export module zep.frontend.sema.context;

import zep.frontend.sema.env;
import zep.frontend.node;
import zep.frontend.sema.type;
import zep.frontend.sema.facade;
import zep.frontend.sema.lifetime;
import zep.common.target;

export class SemaContext {
  public:
    TypeArena types;

    NodeArena nodes;

    FacadeRegistry facades;

    TargetInfo target;

    PrimitiveTypes primitives;

    Env env;

    Lifetime lifetime;

    explicit SemaContext(TargetInfo target = TargetInfo())
        : target(std::move(target)), primitives(types), env(primitives.entries),
          lifetime(types, nodes) {}
};
