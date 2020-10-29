#include "../src/context.h"
#include "../src/lexer.h"
#include "../src/parser.h"
#include "../src/renderer.h"
#include "../src/dialect.h"

#include <gtest/gtest.h>

using namespace std;

Liquid::Context& getContext() {
    static bool setup = false;
    static Liquid::Context context;
    if (!setup) {
        Liquid::StandardDialect::implement(context);
        setup = true;
    }
    return context;
}
Liquid::Parser& getParser() {
    static Liquid::Parser parser(getContext());
    return parser;
}


TEST(sanity, literal) {
    auto ast = getParser().parse("asdf");
    Liquid::Context::CPPVariable variable;
    auto str = getContext().render(ast, variable);
    ASSERT_EQ(str, "asdf");
}

TEST(sanity, variable) {
    Liquid::Context::CPPVariable variable;
    variable["a"] = 3;
    auto ast = getParser().parse("{{ a }}");
    auto str = getContext().render(ast, variable);
    ASSERT_EQ(str, "3");
}

TEST(sanity, addition) {
    Liquid::Context::CPPVariable variable;
    variable["a"] = 3;
    auto ast = getParser().parse("asdbfsdf {{ a + 1 + 2 }} b");
    auto str = getContext().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 6 b");
}

TEST(sanity, subtraction) {
    Liquid::Context::CPPVariable variable;
    variable["a"] = 3;
    auto ast = getParser().parse("asdbfsdf {{ a - 1 + 2 }} b");
    auto str = getContext().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 4 b");
}

TEST(sanity, multiply) {
    Liquid::Context::CPPVariable variable;
    variable["a"] = 3;
    auto ast = getParser().parse("asdbfsdf {{ a * 2 }} b");
    auto str = getContext().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 6 b");
}


TEST(sanity, divide) {
    Liquid::Context::CPPVariable variable;
    variable["a"] = 3;
    auto ast = getParser().parse("asdbfsdf {{ a / 2 }} b");
    auto str = getContext().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 1 b");
}

TEST(sanity, orderOfOperations) {
    Liquid::Context::CPPVariable variable;
    variable["a"] = 3;
    auto ast = getParser().parse("asdbfsdf {{ a + 3 * 6 }} b");
    auto str = getContext().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 21 b");
}

TEST(sanity, parenthesis) {
    /*Liquid::Context::CPPVariable variable;
    variable["a"] = 3;
    auto ast = getParser().parse("asdbfsdf {{ a - (1 + 2) }} b");
    auto str = getContext().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 0 b");*/
}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
};
