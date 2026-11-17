module;

#include <cstdint>
#include <string>
#include <utility>

export module zep.common.system.process;

import zep.common.system.command;

export class ProcessExit {
  public:
    class Kind {
      public:
        enum class Type : std::uint8_t { Exited, Signaled, FailedToStart };
    };

    Kind::Type kind;
    int status;

    ProcessExit(Kind::Type kind, int status) : kind(kind), status(status) {}
};

export class ProcessResult {
  public:
    ProcessExit exit;
    std::string stdout_text;
    std::string stderr_text;

    ProcessResult(ProcessExit exit, std::string stdout_text, std::string stderr_text)
        : exit(exit), stdout_text(std::move(stdout_text)), stderr_text(std::move(stderr_text)) {}

    bool succeeded() const {
        return exit.kind == ProcessExit::Kind::Type::Exited && exit.status == 0;
    }
};

export class ProcessRunner {
  public:
    virtual ~ProcessRunner() = default;

    virtual ProcessResult run(const Command& command) = 0;
};
