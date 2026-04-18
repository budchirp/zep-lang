module;

#include <memory>
#include <string>

export module zep.frontend.sema.symbol;

import zep.common.position;
import zep.common.span;
import zep.frontend.sema.kinds;
import zep.frontend.sema.type;

export class Symbol {
  public:
    class Kind {
      public:
        enum class Type : std::uint8_t {
            Var,
            Function,
            Type,
        };
    };

  protected:
    Symbol(Kind::Type kind, std::string name, Span span, Visibility::Type visibility,
           std::shared_ptr<Type> type)
        : kind(kind), span(span), name(std::move(name)), visibility(visibility),
          type(std::move(type)) {}

  private:
    Symbol(const Symbol&) = delete;
    Symbol& operator=(const Symbol&) = delete;
    Symbol(Symbol&&) = default;
    Symbol& operator=(Symbol&&) = default;

  public:
    Kind::Type kind;

    Span span;

    std::string name;

    Visibility::Type visibility;

    std::shared_ptr<Type> type;

    virtual ~Symbol() = default;

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
};

export class VarSymbol : public Symbol {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Var;

    StorageKind::Type storage_kind;

    VarSymbol(std::string name, Span span, Visibility::Type visibility,
              StorageKind::Type storage_kind, std::shared_ptr<Type> type)
        : Symbol(static_kind, std::move(name), span, visibility, std::move(type)),
          storage_kind(storage_kind) {}
};

export class FunctionSymbol : public Symbol {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Function;

    FunctionSymbol(std::string name, Span span, Visibility::Type visibility,
                   std::shared_ptr<Type> type)
        : Symbol(static_kind, std::move(name), span, visibility, std::move(type)) {}
};

export class TypeSymbol : public Symbol {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Type;

    TypeSymbol(std::string name, Span span, Visibility::Type visibility,
               std::shared_ptr<Type> type)
        : Symbol(static_kind, std::move(name), span, visibility, std::move(type)) {}
};
