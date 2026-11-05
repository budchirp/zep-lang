module;

#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include <variant>

export module zep.frontend.sema.symbols:function;

export import :base;
import zep.common.source.span;
import zep.frontend.kind;
import zep.frontend.sema.types;

export class CallableKind {
  public:
    enum class Type : std::uint8_t { Function, Destructor };
};

export class MethodKind {
  public:
    enum class Type : std::uint8_t { Instance, Static, InstanceExtension, StaticExtension };
};

export class ConstructorKind {
  public:
    enum class Type : std::uint8_t { Value, Copy, Move };
};

export class FunctionSymbol final : public Symbol {
  public:
    using Role = std::variant<CallableKind::Type, MethodKind::Type, ConstructorKind::Type>;

    static constexpr Symbol::Kind::Type static_kind = Symbol::Kind::Type::Function;

    const Linkage::Type linkage;
    const Role role;
    const std::string parent;
    const Abi::Type abi;
    const bool is_deleted;

    FunctionSymbol(std::string name, Span span, Visibility::Type visibility, Linkage::Type linkage,
                   const Type* type, Role role = Role(CallableKind::Type::Function),
                   std::string parent = {}, std::vector<AttributeInfo> attributes = {},
                   Abi::Type abi = Abi::Type::Language, bool is_deleted = false)
        : Symbol(static_kind, std::move(name), span, visibility, type, std::move(attributes)),
          linkage(linkage), role(role), parent(std::move(parent)), abi(abi), is_deleted(is_deleted) {}

    template <typename Kind>
    bool has_role(Kind kind) const {
        const auto* value = std::get_if<Kind>(&role);

        return value != nullptr && *value == kind;
    }

    bool extension() const {
        return has_role(MethodKind::Type::InstanceExtension) || has_role(MethodKind::Type::StaticExtension);
    }

    bool instance() const {
        return has_role(MethodKind::Type::Instance) || has_role(MethodKind::Type::InstanceExtension);
    }

    bool static_method() const {
        return has_role(MethodKind::Type::Static) || has_role(MethodKind::Type::StaticExtension);
    }

    bool constructor() const {
        return std::holds_alternative<ConstructorKind::Type>(role);
    }

    static std::string role_name(const Role& role) {
        if (const auto* callable = std::get_if<CallableKind::Type>(&role); callable != nullptr) {
            return *callable == CallableKind::Type::Function ? "function" : "destructor";
        }

        if (const auto* method = std::get_if<MethodKind::Type>(&role); method != nullptr) {
            return *method == MethodKind::Type::Instance || *method == MethodKind::Type::InstanceExtension
                ? "instance_method" : "static_method";
        }

        switch (std::get<ConstructorKind::Type>(role)) {
        case ConstructorKind::Type::Value:
            return "constructor";
        case ConstructorKind::Type::Copy:
            return "copy_constructor";
        case ConstructorKind::Type::Move:
            return "move_constructor";
        }

        return "unknown";
    }

    std::string base_name() const {
        return parent.empty() ? name : parent + "::" + name;
    }

    const FunctionType* signature() const {
        return type != nullptr ? type->as<FunctionType>() : nullptr;
    }
};
