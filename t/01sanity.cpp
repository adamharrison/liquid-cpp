#include "../src/context.h"
#include "../src/lexer.h"
#include "../src/parser.h"
#include "../src/renderer.h"
#include "../src/dialect.h"

#include <gtest/gtest.h>

using namespace std;



TEST(sanity, rendering) {
    Liquid::Context context;
    Liquid::Parser parser(context);
    Liquid::StandardDialect::implement(context);

    auto ast = parser.parse("asdf");
    Liquid::Context::CPPVariable variable;
    auto str = context.render(ast, variable);

    ASSERT_EQ(str, "asdf");

    variable["a"] = 3;
    ast = parser.parse("asdbfsdf {{ a + 1 + 2 }} b");
    str = context.render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 6 b");

}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
};
