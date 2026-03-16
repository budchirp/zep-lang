module;

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

export module zep.lowerer.sema.types;

import zep.common.logger;

export class LoweredType {
  public:
    enum class Kind : std::uint8_t {
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

  protected:
    explicit LoweredType(Kind kind) : kind(kind) {}

    LoweredType(const LoweredType&) = delete;
    LoweredType& operator=(const LoweredType&) = delete;
    LoweredType(LoweredType&&) = default;
    LoweredType& operator=(LoweredType&&) = default;

  public:
    Kind kind;

    virtual ~LoweredType() = default;

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

export class LoweredVoidType : public LoweredType {
  public:
    static constexpr Kind static_kind = Kind::Void;

    LoweredVoidType() : LoweredType(static_kind) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }
        std::cout << "LoweredVoidType()";
        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    std::string to_string() const override { return "void"; }
};

export class LoweredBooleanType : public LoweredType {
  public:
    static constexpr Kind static_kind = Kind::Boolean;

    LoweredBooleanType() : LoweredType(static_kind) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }
        std::cout << "LoweredBooleanType()";
        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    std::string to_string() const override { return "bool"; }
};

export class LoweredStringType : public LoweredType {
  public:
    static constexpr Kind static_kind = Kind::String;

    LoweredStringType() : LoweredType(static_kind) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }
        std::cout << "LoweredStringType()";
        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    std::string to_string() const override { return "string"; }
};

export class LoweredIntegerType : public LoweredType {
  public:
    static constexpr Kind static_kind = Kind::Integer;

    std::uint8_t bits;
    bool is_signed;

    LoweredIntegerType(std::uint8_t bits, bool is_signed)
        : LoweredType(static_kind), bits(bits), is_signed(is_signed) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }
        std::cout << "LoweredIntegerType(bits: " << static_cast<int>(bits)
                  << ", is_signed: " << (is_signed ? "true" : "false") << ")";
        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    std::string to_string() const override {
        return (is_signed ? "i" : "u") + std::to_string(bits);
    }
};

export class LoweredFloatType : public LoweredType {
  public:
    static constexpr Kind static_kind = Kind::Float;

    std::uint8_t bits;

    explicit LoweredFloatType(std::uint8_t bits = 64) : LoweredType(static_kind), bits(bits) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }
        std::cout << "LoweredFloatType(bits: " << static_cast<int>(bits) << ")";
        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    std::string to_string() const override { return "f" + std::to_string(bits); }
};

export class LoweredPointerType : public LoweredType {
  public:
    static constexpr Kind static_kind = Kind::Pointer;

    std::shared_ptr<LoweredType> base;

    explicit LoweredPointerType(std::shared_ptr<LoweredType> base)
        : LoweredType(static_kind), base(std::move(base)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }
        std::cout << "LoweredPointerType(base: " << (base != nullptr ? base->to_string() : "null")
                  << ")";
        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    std::string to_string() const override {
        return "*" + (base != nullptr ? base->to_string() : "void");
    }
};

export class LoweredArrayType : public LoweredType {
  public:
    static constexpr Kind static_kind = Kind::Array;

    std::shared_ptr<LoweredType> element;
    std::size_t size;

    LoweredArrayType(std::shared_ptr<LoweredType> element, std::size_t size)
        : LoweredType(static_kind), element(std::move(element)), size(size) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }
        std::cout << "LoweredArrayType(element: "
                  << (element != nullptr ? element->to_string() : "null") << ", size: " << size
                  << ")";
        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    std::string to_string() const override {
        return (element != nullptr ? element->to_string() : "void") + "[" + std::to_string(size) +
               "]";
    }
};

export class LoweredStructField {
  public:
    std::string name;
    std::shared_ptr<LoweredType> type;

    LoweredStructField(std::string name, std::shared_ptr<LoweredType> type)
        : name(std::move(name)), type(std::move(type)) {}
};

export class LoweredStructType : public LoweredType {
  public:
    static constexpr Kind static_kind = Kind::Struct;

    std::string name;
    std::vector<LoweredStructField> fields;

    LoweredStructType(std::string name, std::vector<LoweredStructField> fields = {})
        : LoweredType(static_kind), name(std::move(name)), fields(std::move(fields)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }
        std::cout << "LoweredStructType(name: \"" << name << "\", fields: " << fields.size() << ")";
        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    std::string to_string() const override { return name; }
};

export class LoweredFunctionType : public LoweredType {
  public:
    static constexpr Kind static_kind = Kind::Function;

    std::vector<std::shared_ptr<LoweredType>> parameters;
    std::shared_ptr<LoweredType> return_type;
    bool variadic;

    LoweredFunctionType(std::vector<std::shared_ptr<LoweredType>> parameters,
                        std::shared_ptr<LoweredType> return_type, bool variadic = false)
        : LoweredType(static_kind), parameters(std::move(parameters)),
          return_type(std::move(return_type)), variadic(variadic) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }
        std::cout << "LoweredFunctionType(return: "
                  << (return_type != nullptr ? return_type->to_string() : "null") << ", params: [";
        for (std::size_t i = 0; i < parameters.size(); ++i) {
            if (i > 0) {
                std::cout << ", ";
            }
            std::cout << (parameters[i] != nullptr ? parameters[i]->to_string() : "null");
        }
        std::cout << "], variadic: " << (variadic ? "true" : "false") << ")";
        if (trailing_newline) {
            std::cout << "\n";
        }
    }

    std::string to_string() const override {
        return "function( -> " + (return_type != nullptr ? return_type->to_string() : "void") + ")";
    }
};
