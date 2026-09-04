module;

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

export module zep.build.link;

import zep.build.artifact;
import zep.common.target;
import zep.common.system.command;

export class LinkPlanner {
  public:
    static Command executable(const std::vector<ObjectArtifact>& objects,
                              const ExecutableArtifact& output, const TargetInfo& target,
                              const std::vector<std::string>& arguments,
                              std::filesystem::path workdir = {}) {
        std::vector<std::string> argv;
        argv.reserve(objects.size() + arguments.size() + 5);
        argv.push_back("clang");
        argv.push_back("-fuse-ld=lld");
        argv.push_back("--target=" + target.triple);

        for (const auto& object : objects) {
            argv.push_back(object.path.string());
        }

        argv.insert(argv.end(), arguments.begin(), arguments.end());
        argv.push_back("-o");
        argv.push_back(output.path.string());

        return Command(std::move(argv), std::move(workdir));
    }
};
