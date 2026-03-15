module;

#include <memory>
#include <string>

export module zep.sema.symbol;

import zep.common.position;
import zep.sema.kinds;
import zep.sema.type;

export class Symbol {
  public:
    enum class Kind : std::uint8_t {
        Var,
        Function,
        Type,
    };

  protected:
    Symbol(Kind kind, std::string name, Position position, Visibility visibility,
           std::shared_ptr<Type> type)
        : kind(kind), position(position), name(std::move(name)), visibility(visibility),
          type(std::move(type)) {}

  private:

    Symbol(const Symbol&) = delete;
    Symbol& operator=(const Symbol&) = delete;
    Symbol(Symbol&&) = default;
    Symbol& operator=(Symbol&&) = default;

  public:
    Kind kind;

    Position position;

    std::string name;
    
    Visibility visibility;
    
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
    static constexpr Kind static_kind = Kind::Var;

    StorageKind storage_kind;

    VarSymbol(std::string name, Position position, Visibility visibility, StorageKind storage_kind,
              std::shared_ptr<Type> type)
        : Symbol(static_kind, std::move(name), position, visibility, std::move(type)),
          storage_kind(storage_kind) {}
};

export class FunctionSymbol : public Symbol {
  public:
    static constexpr Kind static_kind = Kind::Function;

    FunctionSymbol(std::string name, Position position, Visibility visibility,
                   std::shared_ptr<Type> type)
        : Symbol(static_kind, std::move(name), position, visibility, std::move(type)) {}
};

export class TypeSymbol : public Symbol {
  public:
    static constexpr Kind static_kind = Kind::Type;

    TypeSymbol(std::string name, Position position, Visibility visibility,
               std::shared_ptr<Type> type)
        : Symbol(static_kind, std::move(name), position, visibility, std::move(type)) {}
};
