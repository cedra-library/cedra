#include <gtest/gtest.h>
#include <cdr/types/expect.h>

enum class ErrorSet {
    kExample
};

cdr::Expect<int, ErrorSet> MustSuccess(int value, ErrorSet errs, bool success) {
    if (success) {
        return cdr::Ok<int>(std::in_place, value);
    }
    return cdr::Failure(ErrorSet::kExample);
}

cdr::Expect<void, ErrorSet> VoidOrError(bool success) {
    if (success) {
        return cdr::Ok();
    }
    return cdr::Failure(ErrorSet::kExample);
}

TEST(UmiExpect, Construction) {
    int val = MustSuccess(12, ErrorSet::kExample, true).OrCrashProgram()
        << __FILE__ << " condition mismatch" << std::endl;
    ASSERT_EQ(val, 12);
    ASSERT_TRUE(MustSuccess(21, ErrorSet::kExample, true));
    ASSERT_FALSE(MustSuccess(21, ErrorSet::kExample, false));

    {
        auto res = VoidOrError(true);
        ASSERT_TRUE(res.Succeed());
        ASSERT_TRUE(!res.Failed());
    }

    if (auto res = VoidOrError(false); res.Failed()) {
        ASSERT_TRUE(!res.Succeed());
        ASSERT_TRUE(res.Failed());
        const ErrorSet err = res.GetFailure();
        ASSERT_EQ(err, ErrorSet::kExample);
    }
}

TEST(CdrExpect, Termination) {
    EXPECT_DEATH(MustSuccess(12, ErrorSet::kExample, false).OrCrashProgram() << 3232423, "3232423");
    EXPECT_DEATH(VoidOrError(false).OrCrashProgram() << 46546733, "46546733");
}

struct Struct {} structure;

cdr::Expect<Struct*, ErrorSet> Func() {
    return cdr::Ok(&structure);
}

TEST(UmiExpect, CorrectCompilation) {
    Struct* s = Func().Value();
}
