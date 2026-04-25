module;

#include <cstdint>
#include <string>
#include <utility>

export module zep.frontend.sema.symbol;

import zep.common.span;
import zep.frontend.sema.kind;
import zep.frontend.sema.type;

export class Symbol {
  public:
    class Kind {
      public:
        enum class Type : std::uint8_t { Var, Function, Type };
    };

  protected:
    Symbol(Kind::Type kind, std::string name, Span span, Visibility::Type visibility,
           const Type* type)
        : kind(kind), span(span), visibility(visibility), name(std::move(name)), type(type) {}

  public:
    const Kind::Type kind;

    const Span span;

    const Visibility::Type visibility;

    const std::string name;
    const Type* const type;

    Symbol(const Symbol&) = delete;
    Symbol& operator=(const Symbol&) = delete;
    Symbol(Symbol&&) = delete;
    Symbol& operator=(Symbol&&) = delete;

    template <typename T>
    T* as() {
        if (kind == T::static_kind) {
            return static_cast<T*>(this);
        }

        return nullptr;
    }

    template <typename T>
    const T* as() const {
        if (kind == T::static_kind) {
            return static_cast<const T*>(this);
        }

        return nullptr;
    }

    template <typename T>
    bool is() const {
        return static_cast<bool>(kind == T::static_kind);
    }
};

export class VarSymbol : public Symbol {
  private:
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Var;

    const StorageKind::Type storage_kind;

    VarSymbol(std::string name, Span span, Visibility::Type visibility,
              StorageKind::Type storage_kind, const Type* type)
        : Symbol(static_kind, std::move(name), span, visibility, type), storage_kind(storage_kind) {
    }
};

export class FunctionSymbol : public Symbol {
  private:
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Function;

    FunctionSymbol(std::string name, Span span, Visibility::Type visibility, const Type* type)
        : Symbol(static_kind, std::move(name), span, visibility, type) {}
};

export class TypeSymbol : public Symbol {
  private:
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Type;

    TypeSymbol(std::string name, Span span, Visibility::Type visibility, const Type* type)
        : Symbol(static_kind, std::move(name), span, visibility, type) {}
};
