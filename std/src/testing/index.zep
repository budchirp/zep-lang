import std.io.panic.panic

public struct Test {
    public:
        static fn expect(condition: boolean) -> void {
            Test::expect(condition, "test expectation failed")
        }

        static fn expect(condition: boolean, message: cstr) -> void {
            if (!condition) {
                Test::fail(message)
            }
        }

        static fn fail(message: cstr) -> never {
            panic(message)
        }
}
