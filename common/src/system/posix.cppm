module;

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

export module zep.common.system.posix;

import zep.common.system.command;
import zep.common.system.process;

namespace {

void append_error(std::string& output, const char* operation) {
    if (!output.empty()) {
        output += ": ";
    }
    output += operation;
    output += ": ";
    output += std::strerror(errno);
}

void child_error(int descriptor, const char* operation) {
    auto error = std::string(operation) + ": " + std::strerror(errno);
    static_cast<void>(write(descriptor, error.data(), error.size()));
    _exit(127);
}

bool read_pipe(int& descriptor, std::string& output, bool& failed) {
    char buffer[4096];
    auto count = read(descriptor, buffer, sizeof(buffer));
    if (count > 0) {
        output.append(buffer, static_cast<std::size_t>(count));
        return true;
    }
    if (count < 0 && errno == EINTR) {
        return true;
    }
    if (count < 0) {
        failed = true;
    }
    if (close(descriptor) != 0) {
        failed = true;
    }
    descriptor = -1;
    return true;
}

} // namespace

export class PosixProcessRunner final : public ProcessRunner {
  public:
    ProcessResult run(const Command& command) override;
};

ProcessResult PosixProcessRunner::run(const Command& command) {
    if (command.arguments.empty()) {
        return ProcessResult(ProcessExit(ProcessExit::Kind::Type::FailedToStart, -1), "",
                             "empty process invocation");
    }

    int stdout_pipe[2];
    int stderr_pipe[2];
    int error_pipe[2];
    if (pipe(stdout_pipe) != 0) {
        auto error = std::strerror(errno);
        return ProcessResult(ProcessExit(ProcessExit::Kind::Type::FailedToStart, -1), "", error);
    }
    if (pipe(stderr_pipe) != 0) {
        auto error = std::strerror(errno);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return ProcessResult(ProcessExit(ProcessExit::Kind::Type::FailedToStart, -1), "", error);
    }
    if (pipe(error_pipe) != 0) {
        auto error = std::strerror(errno);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return ProcessResult(ProcessExit(ProcessExit::Kind::Type::FailedToStart, -1), "", error);
    }

    auto flags = fcntl(error_pipe[1], F_GETFD);
    if (flags < 0 || fcntl(error_pipe[1], F_SETFD, flags | FD_CLOEXEC) != 0) {
        auto error = std::strerror(errno);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        close(error_pipe[0]);
        close(error_pipe[1]);
        return ProcessResult(ProcessExit(ProcessExit::Kind::Type::FailedToStart, -1), "", error);
    }

    auto process_id = fork();
    auto process_error = process_id < 0 ? std::string(std::strerror(errno)) : std::string();
    if (process_id == 0) {
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        close(error_pipe[0]);
        if (dup2(stdout_pipe[1], STDOUT_FILENO) < 0) {
            child_error(error_pipe[1], "dup2 stdout");
        }
        if (dup2(stderr_pipe[1], STDERR_FILENO) < 0) {
            child_error(error_pipe[1], "dup2 stderr");
        }
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);
        if (!command.working_directory.empty() && chdir(command.working_directory.c_str()) != 0) {
            child_error(error_pipe[1], "chdir");
        }

        std::vector<std::vector<char>> argument_buffers;
        argument_buffers.reserve(command.arguments.size());
        for (const auto& argument : command.arguments) {
            argument_buffers.emplace_back(argument.begin(), argument.end());
            argument_buffers.back().push_back('\0');
        }
        std::vector<char*> arguments;
        arguments.reserve(argument_buffers.size() + 1);
        for (auto& argument : argument_buffers) {
            arguments.push_back(argument.data());
        }
        arguments.push_back(nullptr);
        execvp(arguments.front(), arguments.data());
        child_error(error_pipe[1], "exec");
    }

    close(stdout_pipe[1]);
    close(stderr_pipe[1]);
    close(error_pipe[1]);
    if (process_id < 0) {
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        close(error_pipe[0]);
        return ProcessResult(ProcessExit(ProcessExit::Kind::Type::FailedToStart, -1), "",
                             process_error);
    }

    int descriptors[3] = {stdout_pipe[0], stderr_pipe[0], error_pipe[0]};
    pollfd polls[3] = {
        {descriptors[0], POLLIN, 0}, {descriptors[1], POLLIN, 0}, {descriptors[2], POLLIN, 0}};
    std::string stdout_text;
    std::string stderr_text;
    bool failed = false;
    while (polls[0].fd >= 0 || polls[1].fd >= 0 || polls[2].fd >= 0) {
        auto poll_result = poll(polls, 3, -1);
        if (poll_result < 0 && errno == EINTR) {
            continue;
        }
        if (poll_result < 0) {
            append_error(stderr_text, "poll");
            failed = true;
            if (kill(process_id, SIGTERM) != 0 && errno != ESRCH) {
                append_error(stderr_text, "kill");
            }
            break;
        }
        if (polls[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            read_pipe(descriptors[0], stdout_text, failed);
        }
        if (polls[1].revents & (POLLIN | POLLHUP | POLLERR)) {
            read_pipe(descriptors[1], stderr_text, failed);
        }
        if (polls[2].revents & (POLLIN | POLLHUP | POLLERR)) {
            auto before = stderr_text.size();
            read_pipe(descriptors[2], stderr_text, failed);
            if (stderr_text.size() != before) {
                failed = true;
            }
        }
        polls[0].fd = descriptors[0];
        polls[1].fd = descriptors[1];
        polls[2].fd = descriptors[2];
        polls[0].revents = polls[1].revents = polls[2].revents = 0;
    }
    for (auto& descriptor : descriptors) {
        if (descriptor >= 0) {
            close(descriptor);
        }
    }

    int status = 0;
    auto waited = waitpid(process_id, &status, 0);
    while (waited < 0 && errno == EINTR) {
        waited = waitpid(process_id, &status, 0);
    }
    if (waited < 0) {
        append_error(stderr_text, "waitpid");
        failed = true;
    }
    if (failed) {
        return ProcessResult(ProcessExit(ProcessExit::Kind::Type::FailedToStart, -1),
                             std::move(stdout_text), std::move(stderr_text));
    }
    if (WIFEXITED(status)) {
        return ProcessResult(ProcessExit(ProcessExit::Kind::Type::Exited, WEXITSTATUS(status)),
                             std::move(stdout_text), std::move(stderr_text));
    }
    if (WIFSIGNALED(status)) {
        return ProcessResult(ProcessExit(ProcessExit::Kind::Type::Signaled, WTERMSIG(status)),
                             std::move(stdout_text), std::move(stderr_text));
    }
    return ProcessResult(ProcessExit(ProcessExit::Kind::Type::FailedToStart, -1),
                         std::move(stdout_text), std::move(stderr_text));
}
