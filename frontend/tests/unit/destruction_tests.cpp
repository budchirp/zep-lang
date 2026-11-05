#include <gtest/gtest.h>

import zep.frontend.test.harness;

TEST(FrontendDestruction, AcceptsGenericImplicitAndScalarDestructors) {
    EXPECT_TRUE(assert_type_check_ok(R"zep(
struct Owner {
    public:
        var value: i32
        fn ~Owner() -> void {}
}
struct Record { public: var owner: Owner }
type Alias = Record

fn destroy<T>(ptr: *mut T) -> void { ptr->~T() }
fn check(ptr: *mut Record, number: *mut i32) -> void {
    ptr->~Alias()
    destroy(number)
}
fn main() -> i32 {
    var mut record = Record { owner: Owner { value: 42 } }
    record.~Record()
    record = Record { owner: Owner { value: 7 } }
    var mut number = 1
    number.~i32()
    number = 2
    return number
}
)zep"));
}

TEST(FrontendDestruction, AllowsDestructorCallsAsVoidExpressions) {
    EXPECT_TRUE(assert_type_check_ok(R"zep(
struct Record { public: var value: i32 }
fn clear(ptr: *mut Record) -> void { return ptr->~Record() }
fn drop(value: i32) -> i32 { return value }
)zep"));
}

TEST(FrontendDestruction, RejectsDeletedAndInaccessibleDestructors) {
    EXPECT_TRUE(assert_type_check_error(R"zep(
struct Owner {
    public:
        var value: i32
        fn ~Owner() -> void = delete
}
fn clear(ptr: *mut Owner) -> void { ptr->~Owner() }
)zep"));

    EXPECT_TRUE(assert_type_check_error(R"zep(
struct Owner {
    private:
        fn ~Owner() -> void {}
    public:
        var value: i32
}
fn clear(ptr: *mut Owner) -> void { ptr->~Owner() }
)zep"));
}

TEST(FrontendDestruction, RejectsRepeatedLocalDestructionAndObsoleteSyntax) {
    EXPECT_TRUE(assert_type_check_error(R"zep(
fn main() -> void {
    var mut value = 42
    value.~i32()
    value.~i32()
}
)zep"));

    EXPECT_TRUE(assert_type_check_error(R"zep(
fn main() -> void {
    var mut value = 42
    drop value
}
)zep"));
}
