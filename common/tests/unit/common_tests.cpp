#include <cstddef>
#include <filesystem>
#include <gtest/gtest.h>
#include <map>
#include <nlohmann/json.hpp>
#include <signal.h>
#include <stdexcept>
#include <string>
#include <vector>

import zep.common.arena;
import zep.common.context;
import zep.common.diagnostic.collection;
import zep.common.logger;
import zep.common.diagnostic.diagnostic;
import zep.common.source.position;
import zep.common.source;
import zep.common.source.manager;
import zep.common.source.span;
import zep.common.system.command;
import zep.common.system.posix;
import zep.common.system.process;
import zep.common.target;
import zep.test.support;

class ArenaBase {
  public:
    virtual ~ArenaBase() = default;
};

class ArenaValue : public ArenaBase {
  public:
    int value;

    explicit ArenaValue(int value) : value(value) {}
};

TEST(CommonArena, StoresCreatedObjects) {
    Arena<ArenaBase> arena;

    auto* first = arena.create<ArenaValue>(1);
    auto* second = arena.create<ArenaValue>(2);

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(first->value, 1);
    EXPECT_EQ(second->value, 2);
}

TEST(CommonProcess, RejectsEmptyCommand) {
    PosixProcessRunner process_runner;

    auto result = process_runner.run(Command({}));

    EXPECT_EQ(result.exit.kind, ProcessExit::Kind::Type::FailedToStart);
    EXPECT_NE(result.stderr_text.find("empty"), std::string::npos);
}

TEST(CommonProcess, CapturesSuccessfulOutput) {
    PosixProcessRunner process_runner;

    auto result = process_runner.run(Command({"/bin/sh", "-c", "printf output"}));

    EXPECT_TRUE(result.succeeded());
    EXPECT_EQ(result.exit.kind, ProcessExit::Kind::Type::Exited);
    EXPECT_EQ(result.exit.status, 0);
    EXPECT_EQ(result.stdout_text, "output");
}

TEST(CommonProcess, ReportsNonzeroExit) {
    PosixProcessRunner process_runner;

    auto result = process_runner.run(Command({"/bin/sh", "-c", "exit 7"}));

    EXPECT_FALSE(result.succeeded());
    EXPECT_EQ(result.exit.kind, ProcessExit::Kind::Type::Exited);
    EXPECT_EQ(result.exit.status, 7);
}

TEST(CommonProcess, ReportsSignalTermination) {
    PosixProcessRunner process_runner;

    auto result = process_runner.run(Command({"/bin/sh", "-c", "kill -TERM $$"}));

    EXPECT_EQ(result.exit.kind, ProcessExit::Kind::Type::Signaled);
    EXPECT_EQ(result.exit.status, SIGTERM);
}

TEST(CommonProcess, ReportsStartupErrors) {
    PosixProcessRunner process_runner;

    auto missing_executable = process_runner.run(Command({"/does/not/exist"}));
    auto missing_directory =
        process_runner.run(Command({"/bin/true"}, std::filesystem::path("/does/not/exist")));

    EXPECT_EQ(missing_executable.exit.kind, ProcessExit::Kind::Type::FailedToStart);
    EXPECT_NE(missing_executable.stderr_text.find("exec"), std::string::npos);
    EXPECT_EQ(missing_directory.exit.kind, ProcessExit::Kind::Type::FailedToStart);
    EXPECT_NE(missing_directory.stderr_text.find("chdir"), std::string::npos);
}

TEST(CommonPosition, TracksLinesAndColumns) {
    Position position(1, 0);

    position.increment_column();
    position.increment_column();
    position.next_line();

    EXPECT_EQ(position.line, 2U);
    EXPECT_EQ(position.column, 0U);
}

TEST(CommonSources, KeepsOwnedSourcesStable) {
    SourceManager sources;

    auto& first = sources.add("first.zep", "fn first() -> i32 { return 1 }");
    auto* first_ptr = &first;
    auto& second = sources.add("second.zep", "fn second() -> i32 { return 2 }");

    EXPECT_EQ(first_ptr->name, "first.zep");
    EXPECT_EQ(first_ptr->content, "fn first() -> i32 { return 1 }");
    EXPECT_EQ(second.name, "second.zep");
}

TEST(CommonDiagnostics, RetainsSourceForEveryLocation) {
    SourceManager sources;
    auto& first = sources.add("first.zep", "first");
    auto& second = sources.add("second.zep", "second");

    Diagnostics diagnostics;
    diagnostics.add_error(first, Span(Position(1, 1), Position(1, 2)), "first error");
    diagnostics.add_warning(second, Span(Position(1, 2), Position(1, 3)), "second warning");

    ASSERT_EQ(diagnostics.entries.size(), 2U);
    EXPECT_EQ(diagnostics.entries[0].location.source, &first);
    EXPECT_EQ(diagnostics.entries[1].location.source, &second);
    EXPECT_TRUE(diagnostics.has_errors());
}

TEST(CommonJson, ParsesNestedObjectsAndArrays) {
    auto value = nlohmann::json::parse(
        "{\"name\":\"zep\",\"enabled\":true,\"items\":[1,2,{\"kind\":\"compiler\"}]}");

    ASSERT_TRUE(value.is_object());
    EXPECT_EQ(value["name"].get<std::string>(), "zep");
    EXPECT_TRUE(value["enabled"].get<bool>());
    ASSERT_EQ(value["items"].size(), 3U);
    EXPECT_EQ(value["items"][2]["kind"].get<std::string>(), "compiler");
}

TEST(CommonJson, RejectsInvalidDocuments) {
    EXPECT_THROW(static_cast<void>(nlohmann::json::parse("{\"name\":}")),
                 nlohmann::json::parse_error);
    EXPECT_THROW(static_cast<void>(nlohmann::json::parse("[true false]")),
                 nlohmann::json::parse_error);
}

TEST(CommonDiagnostics, DetectsErrorsAndDeduplicates) {
    Diagnostics diagnostics;
    auto span = Span(Position(1, 1), Position(1, 2));

    diagnostics.add_error(span, "first");
    EXPECT_TRUE(diagnostics.has_errors());

    diagnostics.add_error(span, "first");
    EXPECT_TRUE(diagnostics.has_errors());

    diagnostics.add_warning(span, "second");
    EXPECT_TRUE(diagnostics.has_errors());
}

TEST(CommonTarget, ParsesSupportedTriples) {
    TargetInfo target("x86_64-unknown-linux-gnu");

    EXPECT_TRUE(target.is_supported());
    EXPECT_EQ(target.arch, TargetArch::Kind::Type::Amd64);
    EXPECT_EQ(target.os, TargetOS::Kind::Type::Linux);

    EXPECT_EQ(TargetInfo::triple_from(TargetArch::Kind::Type::Aarch64, TargetOS::Kind::Type::Macos),
              "aarch64-apple-darwin");
}

TEST(CommonLogger, PrintsToStdoutThroughLogger) {
    auto output = capture_stdout([] { Logger::print("zep", " ", 1); });

    EXPECT_EQ(output, "zep 1");
}
