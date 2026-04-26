module;

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

export module zep.driver;

import zep.common.source;
import zep.common.logger;
import zep.common.context;
import zep.frontend.lexer;
import zep.frontend.parser;
import zep.frontend.sema.context;
import zep.frontend.sema.type.checker;
import zep.frontend.debug.ast_dumper;
import zep.hir.builder;
import zep.hir.node.program;
import zep.hir.debug.dumper;
import zep.codegen;
import zep.codegen.driver;

export class CompileOptions {
  public:
    std::string input;

    std::vector<std::string> libs;
    std::vector<std::string> objs;

    std::string out;
};

export class Driver {
  private:
    std::string read_file(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("could not open file '" + filename + "'");
        }
        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

  public:
    Driver() = default;

    void run(const CompileOptions& options) {
        std::filesystem::create_directory("build");

        std::vector<std::string> files_to_compile;
        for (const auto& lib : options.libs) {
            files_to_compile.push_back(lib);
        }
        files_to_compile.push_back(options.input);

        SemaContext sema;
        std::vector<std::string> generated_objects;

        for (const auto& filename : files_to_compile) {
            auto content = read_file(filename);
            Source source(filename, content);
            Context context(source);

            Parser parser(context, sema, Lexer(context.source.content));
            auto program = parser.parse();

            TypeChecker type_checker(context, sema);
            type_checker.check(program);

            if (context.diagnostics.has_errors()) {
                context.diagnostics.print(context.logger);
                std::exit(1);
            }

            HIRBuilder hir_builder(sema);
            auto hir_program = hir_builder.lower(program);

            std::filesystem::path path(filename);
            auto obj_path = "build/" + path.stem().string() + ".o";

            Codegen codegen(Backend::Type::LLVM);
            codegen.generate(hir_program, obj_path);

            generated_objects.push_back(obj_path);
        }

        std::string link_command = "cc ";
        for (const auto& obj : generated_objects) {
            link_command += obj + " ";
        }
        for (const auto& obj : options.objs) {
            link_command += obj + " ";
        }
        link_command += "-o " + options.out;

        int link_status = std::system(link_command.c_str());
        if (link_status != 0) {
            throw std::runtime_error("linking failed with status " + std::to_string(link_status));
        }
    }
};
