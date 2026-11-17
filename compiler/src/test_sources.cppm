module;

#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

export module zep.compiler.test_sources;

import zep.compiler;
import zep.common.diagnostic.collection;
import zep.common.source.span;
import zep.workspace.module_path;
import zep.workspace.project;

export class TestSources {
  public:
    static bool load(Compiler& compiler, const Project& project,
                     const std::filesystem::path& input, Diagnostics& diagnostics) {
        std::vector<std::filesystem::path> sources;
        sources.reserve(32);

        std::error_code error;

        if (input.empty()) {
            for (const auto& directory : {project.root_package->source_directory,
                                         project.root_directory / "tests"}) {
                if (!std::filesystem::is_directory(directory, error)) {
                    if (error == std::errc::no_such_file_or_directory) {
                        error.clear();
                    }

                    if (error) {
                        break;
                    }

                    continue;
                }

                const auto external = directory != project.root_package->source_directory;

                for (std::filesystem::recursive_directory_iterator iterator(directory, error), end;
                     !error && iterator != end; iterator.increment(error)) {
                    if (external && iterator->is_directory(error) &&
                        iterator->path().filename() == "fixtures") {
                        iterator.disable_recursion_pending();
                        continue;
                    }

                    if (iterator->is_regular_file(error) &&
                        (external ? ModulePath::is_test(iterator->path())
                                  : iterator->path().extension() == ".zep")) {
                        sources.push_back(iterator->path());
                    }
                }

                if (error) {
                    break;
                }
            }
        } else {
            sources.push_back(input);
        }

        if (error) {
            diagnostics.add_error(Span(), "could not discover test sources: " + error.message());

            return false;
        }

        std::ranges::sort(sources);

        for (const auto& source : sources) {
            const auto module = project.module(source);

            if (!module.has_value()) {
                diagnostics.add_error(Span(), "could not resolve test source '" + source.string() + "'");

                return false;
            }

            if (compiler.load(*project.root_package, *module, diagnostics, source) == nullptr) {
                return false;
            }
        }

        return !diagnostics.has_errors();
    }
};
