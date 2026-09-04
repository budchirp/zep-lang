module;

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

export module zep.frontend.sema.context;

import zep.common.source;
import zep.common.logger;
import zep.common.context;
import zep.frontend.sema.env;
import zep.frontend.node;
import zep.frontend.sema.type;
import zep.frontend.sema.kind;
import zep.frontend.sema.scope;
import zep.frontend.sema.scope.definition;
import zep.frontend.sema.resolver.builtin;
import zep.common.target;
import zep.common.arena;

export class PrimitiveFacadeInfo {
  public:
    std::string name;
    const StructType* type = nullptr;
    const Type* backing_type = nullptr;
    std::string backing_parameter;
    const Type* backing_constraint = nullptr;

    PrimitiveFacadeInfo() = default;

    PrimitiveFacadeInfo(std::string name, const StructType* type, const Type* backing_type,
                        std::string backing_parameter, const Type* backing_constraint)
        : name(std::move(name)), type(type), backing_type(backing_type),
          backing_parameter(std::move(backing_parameter)), backing_constraint(backing_constraint) {}

    bool is_generic() const { return !backing_parameter.empty(); }
};

export class FacadeRegistry {
  private:
    std::unordered_map<std::string, PrimitiveFacadeInfo> facades;
    std::unordered_map<const Type*, std::string> facades_by_backing;

  public:
    void register_facade(std::string name, PrimitiveFacadeInfo info,
                         const Type* backing_type = nullptr) {
        if (backing_type != nullptr) {
            facades_by_backing[backing_type] = name;
        }
        facades.insert_or_assign(std::move(name), std::move(info));
    }

    const PrimitiveFacadeInfo* find_by_name(const std::string& name) const {
        auto iterator = facades.find(name);
        return iterator != facades.end() ? &iterator->second : nullptr;
    }

    const PrimitiveFacadeInfo* find_by_backing(const Type* type) const {
        auto iterator = facades_by_backing.find(type);
        if (iterator == facades_by_backing.end()) {
            return nullptr;
        }
        return find_by_name(iterator->second);
    }

    const std::unordered_map<std::string, PrimitiveFacadeInfo>& all() const { return facades; }
};

export class SemaContext {
  public:
    TypeArena types;

    NodeArena nodes;

    FacadeRegistry facades;

    TargetInfo target;

    BuiltinResolver builtin_resolver;

    Env env;

    DefinitionRegistry function_definitions;

    explicit SemaContext(TargetInfo target = TargetInfo())
        : target(std::move(target)), builtin_resolver(types), env(builtin_resolver.primitives) {}
};
