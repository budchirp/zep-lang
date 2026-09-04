module;

#include <expected>
#include <filesystem>
#include <string>

export module zep.codegen.api;

export import zep.codegen.format;
export import zep.codegen.options;
export import zep.hir.program;

export class CodegenBackend {
  public:
    virtual ~CodegenBackend() = default;
    virtual CodegenFormat::Type format() const = 0;
    virtual std::expected<void, std::string> generate(const HIRProgram& program,
                                                      const std::filesystem::path& output,
                                                      const CodegenOptions& options) = 0;
};
