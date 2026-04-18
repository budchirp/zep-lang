module;

#include <memory>
#include <string>

export module zep.hir.sema.scope.symbol;

import zep.hir.sema.type;
import zep.frontend.sema.kinds;

export class HIRSymbol {
  public:
    class Kind {
      public:
        enum class Type : std::uint8_t { Var, Function, Type };
    };

  protected:
    HIRSymbol(Kind::Type kind, std::string name, Linkage::Type linkage,
                  Visibility::Type visibility, std::shared_ptr<HIRType> type)
        : kind(kind), name(std::move(name)), linkage(linkage), visibility(visibility),
          type(std::move(type)) {}

  public:
    Kind::Type kind;
    std::string name;
    Linkage::Type linkage;
    Visibility::Type visibility;
    std::shared_ptr<HIRType> type;

    virtual ~HIRSymbol() = default;

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

export class HIRTypeSymbol : public HIRSymbol {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Type;

    HIRTypeSymbol(std::string name, Linkage::Type linkage, Visibility::Type visibility,
                      std::shared_ptr<HIRType> type)
        : HIRSymbol(static_kind, std::move(name), linkage, visibility, std::move(type)) {}
};

export class HIRFunctionSymbol : public HIRSymbol {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Function;

    HIRFunctionSymbol(std::string name, Linkage::Type linkage, Visibility::Type visibility,
                          std::shared_ptr<HIRType> type)
        : HIRSymbol(static_kind, std::move(name), linkage, visibility, std::move(type)) {}
};

export class HIRVarSymbol : public HIRSymbol {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Var;

    HIRVarSymbol(std::string name, Linkage::Type linkage, Visibility::Type visibility,
                     std::shared_ptr<HIRType> type)
        : HIRSymbol(static_kind, std::move(name), linkage, visibility, std::move(type)) {}
};
