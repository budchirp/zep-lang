module;

#include <cstdint>
#include <string>

export module zep.sema.kinds;

export class Linkage {
  private:
  public:
    enum class Type : std::uint8_t { External, Internal };

    static std::string to_string(Type linkage) {
        return linkage == Type::External ? "external" : "internal";
    }
};

export class Visibility {
  private:
  public:
    enum class Type : std::uint8_t { Public, Private };

    static std::string to_string(Type visibility) {
        return visibility == Type::Public ? "public" : "private";
    }
};

export class StorageKind {
  private:
  public:
    enum class Type : std::uint8_t { Const, Var, VarMut };

    static std::string to_string(Type kind) {
        switch (kind) {
        case Type::Const:
            return "const";
        case Type::Var:
            return "var";
        case Type::VarMut:
            return "var mut";
        }
    }
};
