#include "../support/analysis.hpp"

TEST(LspCompletion, CompletesLocalsInsideTestsWithoutExposingGeneratedFunctions) {
    AnalysisHarness service;
    const auto locals = service.complete_cursor("test \"body\" { var local = 1; lo| }", "body.test.zep");

    EXPECT_NE(find_completion(locals, "local"), nullptr);

    const auto body = service.complete_cursor("test \"body\" { var local = 1; | }", "body.test.zep");

    EXPECT_NE(find_completion(body, "local"), nullptr);
    EXPECT_EQ(find_completion(body, "test"), nullptr);

    const auto module = service.complete_cursor("test \"named\" {}\n|", "module.test.zep");

    EXPECT_NE(find_completion(module, "test"), nullptr);
    EXPECT_EQ(find_completion(service.complete_cursor("|"), "test"), nullptr);

    for (const auto& item : module) {
        EXPECT_FALSE(item.label.starts_with('$')) << item.label;
    }
}

TEST(LspCompletion, CompletesCheckedErrorAttributesAndTypeLists) {
    AnalysisHarness service;
    const auto attribute = service.complete_cursor("@thr| fn work() -> void {}");

    EXPECT_NE(find_completion(attribute, "throws"), nullptr);
    EXPECT_EQ(find_completion(attribute, "throw"), nullptr);

    const auto types = service.complete_cursor(
        "struct Failure {}\nvar FailureValue = 1\nfn FailureHelper() -> void {}\n"
        "@throws(Fai|) fn work() -> void {}");

    EXPECT_NE(find_completion(types, "Failure"), nullptr);
    EXPECT_EQ(find_completion(types, "FailureValue"), nullptr);
    EXPECT_EQ(find_completion(types, "FailureHelper"), nullptr);

    const auto body = service.complete_cursor("fn work() -> void { | }");

    for (const auto* keyword : {"try", "catch", "throw"}) {
        EXPECT_NE(find_completion(body, keyword), nullptr);
    }
}

TEST(LspCompletion, CompletesCatchTypesBindingsAndMembers) {
    AnalysisHarness service(TestPaths::standard_library());
    const std::string source =
        "import std.core.error.Error\n"
        "struct Failure : Error { public: var code: i32 }\n"
        "struct Fake {}\n"
        "@throws(Failure) fn read() -> i32 { throw Failure { code: 7 } }\n";
    const auto types = service.complete_cursor(source +
        "fn main() -> i32 { return read() catch { error: Fa| -> 0, } }");

    EXPECT_NE(find_completion(types, "Failure"), nullptr);
    EXPECT_EQ(find_completion(types, "Fake"), nullptr);

    const auto bindings = service.complete_cursor(source +
        "fn main() -> i32 { return read() catch { error: Failure -> err|.code, } }");

    EXPECT_NE(find_completion(bindings, "error"), nullptr);

    const auto members = service.complete_cursor(source +
        "fn main() -> i32 { return read() catch { error: Failure -> error.co|, } }");

    EXPECT_NE(find_completion(members, "code"), nullptr);

    const auto outside = service.complete_cursor(source +
        "fn main() -> i32 { var value = read() catch { error: Failure -> 0, }; err|; return value }");

    EXPECT_EQ(find_completion(outside, "error"), nullptr);
}

TEST(LspCompletion, CompletesUnfinishedCallsWithoutRepeatingPositionalArguments) {
    AnalysisHarness service;
    const auto items = service.complete_cursor(
        "fn choose(first: i32, second: i32) -> i32 { return first + second }\n"
        "fn main() -> i32 { return choose(1, | ");

    EXPECT_NE(find_completion(items, "second"), nullptr);
    EXPECT_EQ(find_completion(items, "first"), nullptr);
    EXPECT_EQ(find_completion(items, "self"), nullptr);
}

TEST(LspCompletion, CompletesMethodsAndIndirectCallArguments) {
    AnalysisHarness service;
    const auto method = service.complete_cursor(
        "struct Worker { public: fn run(count: i32) -> i32 { return count } }\n"
        "fn main(worker: &Worker) -> i32 { return worker.run(co|) }");

    EXPECT_NE(find_completion(method, "count"), nullptr);
    EXPECT_EQ(find_completion(method, "self"), nullptr);

    const auto indirect = service.complete_cursor(
        "fn choose(first: i32, second: i32) -> i32 { return first + second }\n"
        "fn main() -> i32 { var callback = choose; return callback(1, se|) }");

    EXPECT_NE(find_completion(indirect, "second"), nullptr);
}

TEST(LspCompletion, IgnoresLabelsAndCommasInsideStringArguments) {
    AnalysisHarness service;
    const auto items = service.complete_cursor(
        "fn send(message: cstr, count: i32) -> i32 { return count }\n"
        "fn main() -> i32 { return send(\"label: hello, world\", co|) }");

    EXPECT_NE(find_completion(items, "count"), nullptr);
    EXPECT_EQ(find_completion(items, "message"), nullptr);
}

TEST(LspAnalysis, CompletesNamedArguments) {
    AnalysisHarness service;
    const auto items = service.complete_cursor(
        "fn move(horizontal: i32, vertical: i32) -> i32 { return horizontal + vertical }\n"
        "fn main() -> i32 { return move(hor|) }");
    const auto* horizontal = find_completion(items, "horizontal");

    ASSERT_NE(horizontal, nullptr);
    EXPECT_EQ(horizontal->insertion, "horizontal: ");
    EXPECT_EQ(find_completion(items, "vertical"), nullptr);
}

TEST(LspAnalysis, OmitsAlreadySuppliedNamedArguments) {
    AnalysisHarness service;
    const auto items = service.complete_cursor(
        "fn move(horizontal: i32, vertical: i32) -> i32 { return horizontal + vertical }\n"
        "fn main() -> i32 { return move(horizontal: 1, ver|) }");
    const auto* vertical = find_completion(items, "vertical");

    ASSERT_NE(vertical, nullptr);
    EXPECT_EQ(vertical->insertion, "vertical: ");
    EXPECT_EQ(find_completion(items, "horizontal"), nullptr);
}

TEST(LspAnalysis, CompletesStaticMethods) {
    AnalysisHarness service;
    const auto items = service.complete_cursor(
        "struct Counter { public: var value: i32 }\n"
        "static fn Counter.zero() -> Counter { return Counter { value: 0 } }\n"
        "fn main() -> i32 { return Counter::ze|().value }");
    const auto* method = find_completion(items, "zero");

    ASSERT_NE(method, nullptr);
    EXPECT_EQ(method->kind, CompletionKind::Type::Method);
}

TEST(LspCompletion, SuppressesCommentsAndStrings) {
    AnalysisHarness service;

    EXPECT_TRUE(service.complete_cursor("fn main() -> void { // ret|\n}").empty());
    EXPECT_TRUE(service.complete_cursor("fn main() -> void { var value = \"ret|urn\" }").empty());
}

TEST(LspCompletion, RestrictsTypePositionsToTypes) {
    AnalysisHarness service;
    const auto items = service.complete_cursor(
        "struct Point { public: var x: i32 }\n"
        "fn main() -> void { var local = 1; var typed: | }");

    EXPECT_NE(find_completion(items, "Point"), nullptr);
    EXPECT_NE(find_completion(items, "i32"), nullptr);
    EXPECT_EQ(find_completion(items, "local"), nullptr);
    EXPECT_EQ(find_completion(items, "return"), nullptr);
    EXPECT_EQ(find_completion(items, "#sizeof"), nullptr);

    for (const auto* type : {"*mut |", "&mut |", "move |", "Container<|"}) {
        const auto qualified = service.complete_cursor(
            std::string("struct Point { public: var x: i32 }\n") +
            "struct Container<T> { public: var value: T }\n"
            "fn main() -> void { var local = 1; var typed: " + type + " }");

        EXPECT_NE(find_completion(qualified, "Point"), nullptr) << type;
        EXPECT_EQ(find_completion(qualified, "local"), nullptr) << type;
    }
}

TEST(LspCompletion, ReplacesTheWholeIdentifierAtAMiddleCursor) {
    AnalysisHarness service;
    const auto items = service.complete_cursor("fn main() -> void { var count = 1; co|unt }");
    const auto* item = find_completion(items, "count");

    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->replacement.end.column - item->replacement.start.column, 5U);
    EXPECT_EQ(item->insertion, "count");
}

TEST(LspCompletion, HidesLaterLocalsOnTheSameLine) {
    AnalysisHarness service;
    const auto items = service.complete_cursor(
        "fn main() -> void { var before = 1; |unknown; var later = 2; }");

    EXPECT_NE(find_completion(items, "before"), nullptr);
    EXPECT_EQ(find_completion(items, "later"), nullptr);
}

TEST(LspCompletion, ResolvesCallsIndicesAndArrowReceivers) {
    AnalysisHarness service;
    const auto declaration = std::string(
        "struct Point { public: var x: i32 }\n"
        "fn make() -> Point { return Point { x: 1 } }\n");

    for (const auto* expression : {"make().|", "values[0].|", "values->|"}) {
        const auto items = service.complete_cursor(declaration +
            "fn check(values: *Point) -> void { " + expression + " }");
        const auto* field = find_completion(items, "x");

        ASSERT_NE(field, nullptr) << expression;
        EXPECT_EQ(field->replacement.start.column, field->replacement.end.column) << expression;
    }
}

TEST(LspCompletion, AppliesVisibilityAndStaticReceiverRules) {
    AnalysisHarness service;
    const auto declaration = std::string(R"zep(
struct Record<T> {
    public:
        var value: T
        fn read() -> T { return value }
        static fn answer() -> i32 { return 42 }
    private:
        var hidden: i32
        fn secret() -> i32 { return hidden }
}
)zep");
    const auto members = service.complete_cursor(declaration +
        "fn check(value: &Record<i32>) -> void { value.| }");

    EXPECT_NE(find_completion(members, "value"), nullptr);
    EXPECT_NE(find_completion(members, "read"), nullptr);
    EXPECT_EQ(find_completion(members, "hidden"), nullptr);
    EXPECT_EQ(find_completion(members, "secret"), nullptr);
    EXPECT_EQ(find_completion(members, "answer"), nullptr);

    const auto statics = service.complete_cursor(declaration +
        "fn check() -> void { Record<i32>::| }");

    EXPECT_NE(find_completion(statics, "answer"), nullptr);
    EXPECT_EQ(find_completion(statics, "value"), nullptr);
    EXPECT_EQ(find_completion(statics, "read"), nullptr);
}

TEST(LspCompletion, DistinguishesEnumVariantsFromInstanceMethods) {
    AnalysisHarness service;
    const auto declaration = std::string(
        "enum Choice { None Some { value: i32 } public: fn code() -> i32 { return 1 } }\n");
    const auto members = service.complete_cursor(declaration +
        "fn check(value: &Choice) -> void { value.| }");

    EXPECT_NE(find_completion(members, "code"), nullptr);
    EXPECT_EQ(find_completion(members, "None"), nullptr);

    const auto variants = service.complete_cursor(declaration + "fn check() -> void { Choice::| }");

    EXPECT_NE(find_completion(variants, "None"), nullptr);
    EXPECT_NE(find_completion(variants, "Some"), nullptr);
    EXPECT_EQ(find_completion(variants, "code"), nullptr);
}

TEST(LspCompletion, CompletesInterfaceAndPrimitiveFacadeMethods) {
    AnalysisHarness service(TestPaths::standard_library());
    const auto methods = service.complete_cursor(
        "interface Reader { public: fn read() -> i32 }\n"
        "fn check(value: &Reader) -> void { value.| }");

    EXPECT_NE(find_completion(methods, "read"), nullptr);

    const auto facade = service.complete_cursor("fn check(value: cstr) -> void { value.| }");

    EXPECT_NE(find_completion(facade, "length"), nullptr);
    EXPECT_NE(find_completion(facade, "contains"), nullptr);
}

TEST(LspCompletion, CompletesOnlyRemainingBuiltinsWithHashPrefixes) {
    AnalysisHarness service;
    const auto items = service.complete_cursor("fn main() -> i32 { return #si|zeof(i32) as i32 }");

    EXPECT_NE(find_completion(items, "#sizeof"), nullptr);
    EXPECT_EQ(find_completion(items, "#move"), nullptr);
    EXPECT_EQ(find_completion(items, "#construct_at"), nullptr);
    EXPECT_EQ(find_completion(items, "#destroy_at"), nullptr);
}

TEST(LspCompletion, CompletesStorageKeywordsAndMemoryMethods) {
    AnalysisHarness service(TestPaths::standard_library());
    const auto keywords = service.complete_cursor("fn check() -> void { | }");

    EXPECT_NE(find_completion(keywords, "new"), nullptr);
    EXPECT_EQ(find_completion(keywords, "delete"), nullptr);
    EXPECT_EQ(find_completion(keywords, "drop"), nullptr);
    EXPECT_EQ(find_completion(keywords, "place"), nullptr);

    const auto implementation = service.complete_cursor(
        "struct Value { public: fn Value(source: &Value) -> Value = de| }");

    EXPECT_NE(find_completion(implementation, "delete"), nullptr);
    EXPECT_NE(find_completion(implementation, "default"), nullptr);

    const auto methods = service.complete_cursor(
        "import std.memory.Memory\nfn check() -> void { Memory::| }");

    EXPECT_NE(find_completion(methods, "move"), nullptr);
    EXPECT_NE(find_completion(methods, "allocate"), nullptr);
    EXPECT_NE(find_completion(methods, "free"), nullptr);
    EXPECT_EQ(find_completion(methods, "construct_at"), nullptr);
    EXPECT_EQ(find_completion(methods, "destroy_at"), nullptr);
}

TEST(LspCompletion, CompletesSharedEnumFields) {
    AnalysisHarness service;
    const auto fields = service.complete_cursor(
        "enum Failure(code: i32) { Open }\n"
        "fn check() -> void { var failure = Failure::Open { code: 7 }; failure.co| }");

    EXPECT_NE(find_completion(fields, "code"), nullptr);

    const auto construction = service.complete_cursor(
        "enum Failure(code: i32) { Open }\n"
        "fn check() -> void { var failure = Failure::Open { co| } }");

    EXPECT_NE(find_completion(construction, "code"), nullptr);
}

TEST(LspCompletion, CompletesStructAndEnumPayloadFieldNames) {
    AnalysisHarness service;
    const auto fields = service.complete_cursor(
        "struct Point { public: var first: i32 var second: i32 }\n"
        "fn main() -> void { var point = Point { first: 1, se|cond: 2 } }");
    const auto* second = find_completion(fields, "second");

    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->insertion, "second: ");
    EXPECT_EQ(find_completion(fields, "first"), nullptr);

    const auto payload = service.complete_cursor(
        "enum Choice { Item { value: i32 } }\n"
        "fn main() -> void { var choice = Choice::Item { va|lue: 1 } }");

    EXPECT_NE(find_completion(payload, "value"), nullptr);

    const auto unfinished = service.complete_cursor(
        "struct point { public: var first: i32 var second: i32 }\n"
        "fn main() -> void { var value = point { first: 1, se|");
    const auto* completion = find_completion(unfinished, "second");

    ASSERT_NE(completion, nullptr);
    EXPECT_EQ(find_completion(unfinished, "first"), nullptr);
    EXPECT_EQ(completion->replacement.end.column - completion->replacement.start.column, 2U);
}

TEST(LspCompletion, FiltersMutableAndConsumingMethodsForBorrowedReceivers) {
    AnalysisHarness service;
    const auto items = service.complete_cursor(
        "struct Record { public: fn read() -> i32 { return 1 } "
        "fn change() mut -> i32 { return 2 } fn consume() move -> i32 { return 3 } }\n"
        "fn check(value: &Record) -> void { value.| }");

    EXPECT_NE(find_completion(items, "read"), nullptr);
    EXPECT_EQ(find_completion(items, "change"), nullptr);
    EXPECT_EQ(find_completion(items, "consume"), nullptr);

    const auto pointer = service.complete_cursor(
        "struct Record { public: fn change() mut -> i32 { return 2 } }\n"
        "fn check(value: *mut Record) -> void { value->| }");

    EXPECT_NE(find_completion(pointer, "change"), nullptr);
}

TEST(LspAnalysis, ReturnsCompletionsIncludingKeywordsPrimitivesAndSymbols) {
    AnalysisHarness service;
    const auto items = service.complete_cursor(
        "struct MyStruct { public: var value: i32 }\n"
        "fn my_function() -> void {}\nvar my_variable: i32 = 0;\n|");

    for (const auto* label : {"fn", "var", "struct", "i32", "cstr", "#sizeof",
                              "MyStruct", "my_function", "my_variable"}) {
        EXPECT_NE(find_completion(items, label), nullptr) << label;
    }
}
