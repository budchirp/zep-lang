module;

#include <expected>
#include <filesystem>
#include <string>

export module zep.codegen.api;

export import zep.codegen.options;
export import zep.hir.program;

export class Backend {
  public:
    virtual ~Backend() = default;
    virtual std::expected<void, std::string> generate(const HIRProgram& program,
                                                      const std::filesystem::path& output,
                                                      const EmitOptions& options) = 0;
};
