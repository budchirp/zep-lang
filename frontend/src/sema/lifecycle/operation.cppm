module;

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

export module zep.frontend.sema.lifecycle.operation;

import zep.frontend.sema.types;
import zep.frontend.sema.symbols;

export class Operation {
  public:
    enum class Type : std::uint8_t { Copy, Move, Assign, MoveAssign, Destroy };
};

export class LifecycleMode {
  public:
    enum class Type : std::uint8_t { Selected, Defaulted };
};

export class LifecyclePlan;

export class LifecyclePart {
  public:
    class Kind {
      public:
        enum class Type : std::uint8_t { Base, Field, Element, Payload };
    };

    const Kind::Type kind;
    const std::string name;
    const std::size_t variant;
    const std::size_t count;
    const std::shared_ptr<const LifecyclePlan> plan;

    LifecyclePart(Kind::Type kind, std::string name, std::size_t variant, std::size_t count,
                  std::shared_ptr<const LifecyclePlan> plan)
        : kind(kind), name(std::move(name)), variant(variant), count(count), plan(std::move(plan)) {}
};

export class LifecyclePlan {
  public:
    class Action {
      public:
        enum class Type : std::uint8_t { Trivial, Function, Aggregate, Deleted, Deferred };
    };

    const Type* const type;
    const Operation::Type operation;
    const Action::Type action;
    const FunctionSymbol* const function;
    const std::vector<LifecyclePart> parts;

    LifecyclePlan(const Type* type, Operation::Type operation, Action::Type action,
                  const FunctionSymbol* function, std::vector<LifecyclePart> parts)
        : type(type), operation(operation), action(action), function(function),
          parts(std::move(parts)) {}
};
