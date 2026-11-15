module;

#include <algorithm>
#include <filesystem>
#include <string>
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

        try {
            if (input.empty()) {
                for (const auto& directory : {project.root_package->source_directory,
                                             project.root_directory / "tests"}) {
                    if (!std::filesystem::is_directory(directory)) {
                        continue;
                    }

                    const auto external = directory != project.root_package->source_directory;

                    for (std::filesystem::recursive_directory_iterator iterator(directory), end;
                         iterator != end; ++iterator) {
                        if (external && iterator->is_directory() &&
                            iterator->path().filename() == "fixtures") {
                            iterator.disable_recursion_pending();
                            continue;
                        }

                        if (iterator->is_regular_file() &&
                            (external ? ModulePath::is_test(iterator->path())
                                      : iterator->path().extension() == ".zep")) {
                            sources.push_back(iterator->path());
                        }
                    }
                }
            } else {
                sources.push_back(input);
            }

            std::ranges::sort(sources);

            for (const auto& source : sources) {
                const auto module = project.module(source);

                if (!module.has_value()) {
                    diagnostics.add_error(Span(), "could not resolve test source '" +
                        source.string() + "'");

                    return false;
                }

                if (compiler.load(*project.root_package, *module, diagnostics, source) == nullptr) {
                    return false;
                }
            }
        } catch (const std::filesystem::filesystem_error& error) {
            diagnostics.add_error(Span(), "could not discover test sources: " + std::string(error.what()));

            return false;
        }

        return !diagnostics.has_errors();
    }
};
