module;

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

export module zep.frontend.sema.scope:symbol;

import zep.common.source.span;
import zep.common.arena;
import zep.frontend.sema.kind;
import zep.frontend.sema.type;

export class Scope;

export class AttributeInfo {
  public:
    std::string name;
    std::vector<std::string> arguments;

    AttributeInfo(std::string name, std::vector<std::string> arguments)
        : name(std::move(name)), arguments(std::move(arguments)) {}
};

export class Symbol {
  public:
    class Kind {
      public:
        enum class Type : std::uint8_t { Type, Variable, Function, EnumVariant };
    };

  protected:
    Symbol(Kind::Type kind, std::string name, Span span, Visibility::Type visibility,
           const Type* type, std::vector<AttributeInfo> attributes = {})
        : kind(kind), span(span), visibility(visibility), name(std::move(name)), type(type),
          attributes(std::move(attributes)) {}

  public:
    const Kind::Type kind;
    const Span span;
    const Visibility::Type visibility;
    const std::string name;
    const Type* const type;
    std::vector<AttributeInfo> attributes;

    virtual ~Symbol() = default;

    template <typename T>
    T* as() {
        return kind == T::static_kind ? static_cast<T*>(this) : nullptr;
    }

    template <typename T>
    const T* as() const {
        return kind == T::static_kind ? static_cast<const T*>(this) : nullptr;
    }

    const AttributeInfo* get_attribute(const std::string& attribute_name) const {
        for (const auto& attribute : attributes) {
            if (attribute.name == attribute_name) {
                return &attribute;
            }
        }

        return nullptr;
    }

    bool has_attribute(const std::string& attribute_name) const {
        return get_attribute(attribute_name) != nullptr;
    }
};

export class TypeSymbol final : public Symbol {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Type;
    Scope* member_scope = nullptr;

    TypeSymbol(std::string name, Span span, Visibility::Type visibility, const Type* type,
               std::vector<AttributeInfo> attributes = {})
        : Symbol(static_kind, std::move(name), span, visibility, type, std::move(attributes)) {}
};

export class VariableSymbol final : public Symbol {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Variable;
    const StorageKind::Type storage_kind;

    VariableSymbol(std::string name, Span span, Visibility::Type visibility,
                   StorageKind::Type storage_kind, const Type* type,
                   std::vector<AttributeInfo> attributes = {})
        : Symbol(static_kind, std::move(name), span, visibility, type, std::move(attributes)),
          storage_kind(storage_kind) {}
};

export class FunctionSymbol final : public Symbol {
  public:
    class Kind {
      public:
        enum class Type : std::uint8_t {
            Function,
            InstanceMethod,
            StaticMethod,
            Constructor,
            Destructor,
        };

        static std::string to_string(Type type) {
            switch (type) {
            case Type::Function:
                return "function";
            case Type::InstanceMethod:
                return "instance_method";
            case Type::StaticMethod:
                return "static_method";
            case Type::Constructor:
                return "constructor";
            case Type::Destructor:
                return "destructor";
            }
            return "unknown";
        }

        static Type classify(const std::string& parent, bool is_static, const std::string& name) {
            if (parent.empty()) {
                return Type::Function;
            }
            if (name == parent) {
                return Type::Constructor;
            }
            if (name == "~" + parent) {
                return Type::Destructor;
            }
            return is_static ? Type::StaticMethod : Type::InstanceMethod;
        }
    };

    static constexpr Symbol::Kind::Type static_kind = Symbol::Kind::Type::Function;
    const FunctionType* function_type;
    const Linkage::Type linkage;
    const Kind::Type callable_kind;
    const std::string parent;
    const bool is_extension;
    const Abi::Type abi;
    FunctionSymbol(std::string name, Span span, Visibility::Type visibility, Linkage::Type linkage,
                   const Type* type, Kind::Type callable_kind = Kind::Type::Function,
                   std::string parent = {}, std::vector<AttributeInfo> attributes = {},
                   bool is_extension = false, Abi::Type abi = Abi::Type::Language)
        : Symbol(static_kind, std::move(name), span, visibility, type, std::move(attributes)),
          function_type(type != nullptr ? type->as<FunctionType>() : nullptr), linkage(linkage),
          callable_kind(callable_kind), parent(std::move(parent)), is_extension(is_extension),
          abi(abi) {}

    std::string base_name() const { return parent.empty() ? name : parent + "::" + name; }

    bool is_mangled() const {
        const auto* attribute = get_attribute("mangle");
        if (attribute == nullptr) {
            return linkage != Linkage::Type::External;
        }
        if (attribute->arguments.empty()) {
            return true;
        }
        return attribute->arguments[0] != "false";
    }
};

export class EnumVariantSymbol final : public Symbol {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::EnumVariant;
    const EnumType* const enum_type;
    const EnumVariantType* const variant_type;

    EnumVariantSymbol(std::string name, Span span, Visibility::Type visibility,
                      const EnumType* enum_type, const EnumVariantType* variant_type)
        : Symbol(static_kind, std::move(name), span, visibility, enum_type), enum_type(enum_type),
          variant_type(variant_type) {}
};

export class OverloadSet {
  public:
    std::vector<FunctionSymbol*> functions;
};

export using SymbolArena = Arena<Symbol>;
