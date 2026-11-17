#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <gtest/gtest.h>
#include <memory>
#include <signal.h>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

import zep.common.arena;
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

class TrackedArenaValue : public ArenaBase {
  private:
    std::vector<int>& destroyed;

  public:
    int value;

    TrackedArenaValue(std::vector<int>& destroyed, int value)
        : destroyed(destroyed), value(value) {}

    ~TrackedArenaValue() override { destroyed.push_back(value); }
};

TEST(CommonArena, DestroysObjectsInCreationOrder) {
    std::vector<int> destroyed;
    destroyed.reserve(2);

    {
        Arena<ArenaBase> arena;
        const auto* first = arena.create<TrackedArenaValue>(destroyed, 1);
        const auto* second = arena.create<TrackedArenaValue>(destroyed, 2);

        ASSERT_NE(first, nullptr);
        ASSERT_NE(second, nullptr);
        EXPECT_EQ(first->value, 1);
        EXPECT_EQ(second->value, 2);
        EXPECT_TRUE(destroyed.empty());
    }

    EXPECT_EQ(destroyed, (std::vector<int>{1, 2}));
}

TEST(CommonArena, MovesOwnershipAndDestroysReplacedObjects) {
    std::vector<int> destroyed;
    destroyed.reserve(3);

    {
        Arena<ArenaBase> source;
        auto* first = source.create<TrackedArenaValue>(destroyed, 1);
        auto* second = source.create<TrackedArenaValue>(destroyed, 2);
        Arena<ArenaBase> moved(std::move(source));
        EXPECT_TRUE(destroyed.empty());

        Arena<ArenaBase> destination;
        static_cast<void>(destination.create<TrackedArenaValue>(destroyed, 3));
        destination = std::move(moved);

        EXPECT_EQ(destroyed, (std::vector<int>{3}));
        EXPECT_EQ(first->value, 1);
        EXPECT_EQ(second->value, 2);

        auto* same = &destination;
        destination = std::move(*same);
        EXPECT_EQ(destroyed, (std::vector<int>{3}));
        EXPECT_EQ(first->value, 1);
    }

    EXPECT_EQ(destroyed, (std::vector<int>{3, 1, 2}));
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

    EXPECT_EQ(position.column, 2U);

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

TEST(CommonDiagnostics, OwnsSyntheticSourcesThroughEntryCopiesAndMoves) {
    std::weak_ptr<const Source> lifetime;
    Diagnostics retained;

    {
        auto source = std::make_shared<const Source>("synthetic.json", "{}");
        lifetime = source;
        Diagnostics diagnostics;
        diagnostics.add_error(source, Span::from_position(Position()), "failure");
        diagnostics.add_error(source, Span::from_position(Position()), "failure");
        ASSERT_EQ(diagnostics.entries.size(), 1U);
        retained.entries.push_back(diagnostics.entries.front());
    }

    EXPECT_FALSE(lifetime.expired());
    Diagnostics moved(std::move(retained));
    EXPECT_EQ(moved.entries.front().location.source->name, "synthetic.json");
    EXPECT_EQ(moved.entries.front().location.source->content, "{}");
    moved.entries.clear();
    EXPECT_TRUE(lifetime.expired());
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

TEST(TestFixtures, ExcludesGeneratedBuildDirectories) {
    TestWorkspace source("fixture_copy_source");
    TestWorkspace destination("fixture_copy_destination");
    source.write("sample/src/main.zep", "fn main() -> i32 { return 0 }");
    source.write("sample/zep.json", "{}");
    source.write("sample/build/objs/main.o", "stale");
    source.write("sample/nested/build/executable", "stale");

    const auto* previous = std::getenv("ZEP_TEST_SOURCE_DIR");
    const auto previous_root = previous != nullptr ? std::string(previous) : std::string();
    ASSERT_EQ(setenv("ZEP_TEST_SOURCE_DIR", source.root().c_str(), 1), 0);

    destination.copy_fixture("sample");

    if (previous != nullptr) {
        EXPECT_EQ(setenv("ZEP_TEST_SOURCE_DIR", previous_root.c_str(), 1), 0);
    } else {
        EXPECT_EQ(unsetenv("ZEP_TEST_SOURCE_DIR"), 0);
    }

    EXPECT_EQ(TextFile::read(destination.file("sample/src/main.zep")),
              "fn main() -> i32 { return 0 }");
    EXPECT_TRUE(std::filesystem::exists(destination.file("sample/zep.json")));
    EXPECT_FALSE(std::filesystem::exists(destination.file("sample/build")));
    EXPECT_FALSE(std::filesystem::exists(destination.file("sample/nested/build")));
}
