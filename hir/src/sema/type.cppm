module;

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

export module zep.hir.sema.type;

import zep.common.logger;

export class HIRType {
  public:
    class Kind {
      public:
        enum class Type : std::uint8_t {
            Void,
            Boolean,
            String,
            Integer,
            Float,
            Pointer,
            Array,
            Struct,
            Function
        };
    };

  protected:
    explicit HIRType(Kind::Type kind) : kind(kind) {}

    HIRType(const HIRType&) = delete;
    HIRType& operator=(const HIRType&) = delete;
    HIRType(HIRType&&) = default;
    HIRType& operator=(HIRType&&) = default;

  public:
    Kind::Type kind;

    virtual ~HIRType() = default;

    virtual void dump(int depth, bool with_indent = true, bool trailing_newline = true) const = 0;
    virtual std::string to_string() const = 0;

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

export class HIRVoidType : public HIRType {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Void;

    HIRVoidType() : HIRType(static_kind) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }
        Logger::print("HIRVoidType()");
        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    std::string to_string() const override { return "void"; }
};

export class HIRBooleanType : public HIRType {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Boolean;

    HIRBooleanType() : HIRType(static_kind) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }
        Logger::print("HIRBooleanType()");
        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    std::string to_string() const override { return "bool"; }
};

export class HIRStringType : public HIRType {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::String;

    HIRStringType() : HIRType(static_kind) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }
        Logger::print("HIRStringType()");
        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    std::string to_string() const override { return "string"; }
};

export class HIRIntegerType : public HIRType {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Integer;

    std::uint8_t bits;
    bool is_signed;

    HIRIntegerType(std::uint8_t bits, bool is_signed)
        : HIRType(static_kind), bits(bits), is_signed(is_signed) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }
        Logger::print("HIRIntegerType(bits: ", static_cast<int>(bits),
                      ", is_signed: ", (is_signed ? "true" : "false"), ")");
        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    std::string to_string() const override {
        return (is_signed ? "i" : "u") + std::to_string(bits);
    }
};

export class HIRFloatType : public HIRType {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Float;

    std::uint8_t bits;

    explicit HIRFloatType(std::uint8_t bits = 64) : HIRType(static_kind), bits(bits) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }
        Logger::print("HIRFloatType(bits: ", static_cast<int>(bits), ")");
        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    std::string to_string() const override { return "f" + std::to_string(bits); }
};

export class HIRPointerType : public HIRType {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Pointer;

    std::shared_ptr<HIRType> base;

    explicit HIRPointerType(std::shared_ptr<HIRType> base)
        : HIRType(static_kind), base(std::move(base)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }
        Logger::print("HIRPointerType(base: ", (base != nullptr ? base->to_string() : "null"), ")");
        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    std::string to_string() const override {
        return "*" + (base != nullptr ? base->to_string() : "void");
    }
};

export class HIRArrayType : public HIRType {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Array;

    std::shared_ptr<HIRType> element;
    std::size_t size;

    HIRArrayType(std::shared_ptr<HIRType> element, std::size_t size)
        : HIRType(static_kind), element(std::move(element)), size(size) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }
        Logger::print("HIRArrayType(element: ",
                      (element != nullptr ? element->to_string() : "null"), ", size: ", size, ")");
        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    std::string to_string() const override {
        return (element != nullptr ? element->to_string() : "void") + "[" + std::to_string(size) +
               "]";
    }
};

export class HIRStructField {
  public:
    std::string name;
    std::shared_ptr<HIRType> type;

    HIRStructField(std::string name, std::shared_ptr<HIRType> type)
        : name(std::move(name)), type(std::move(type)) {}
};

export class HIRStructType : public HIRType {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Struct;

    std::string name;
    std::vector<HIRStructField> fields;

    HIRStructType(std::string name, std::vector<HIRStructField> fields = {})
        : HIRType(static_kind), name(std::move(name)), fields(std::move(fields)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }
        Logger::print("HIRStructType(name: \"", name, "\", fields: ", fields.size(), ")");
        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    std::string to_string() const override { return name; }
};

export class HIRFunctionType : public HIRType {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Function;

    std::vector<std::shared_ptr<HIRType>> parameters;
    std::shared_ptr<HIRType> return_type;
    bool variadic;

    HIRFunctionType(std::vector<std::shared_ptr<HIRType>> parameters,
                    std::shared_ptr<HIRType> return_type, bool variadic = false)
        : HIRType(static_kind), parameters(std::move(parameters)),
          return_type(std::move(return_type)), variadic(variadic) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            Logger::print_indent(depth);
        }
        Logger::print("HIRFunctionType(return: ",
                      (return_type != nullptr ? return_type->to_string() : "null"), ", params: [");
        for (std::size_t i = 0; i < parameters.size(); ++i) {
            if (i > 0) {
                Logger::print(", ");
            }
            Logger::print((parameters[i] != nullptr ? parameters[i]->to_string() : "null"));
        }
        Logger::print("], variadic: ", (variadic ? "true" : "false"), ")");
        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    std::string to_string() const override {
        return "function( -> " + (return_type != nullptr ? return_type->to_string() : "void") + ")";
    }
};
