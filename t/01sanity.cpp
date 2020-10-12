#include "../src/context.h"
#include "../src/lexer.h"
#include "../src/parser.h"
#include "../src/renderer.h"

#include <gtest/gtest.h>



TEST(sanity, rendering) {
    Liquid::Context context;
    Liquid::Parser<Liquid::Context> parser(context);
    Liquid::Renderer<Liquid::Context> renderer(context);

    auto ast = parser.parse("asdf");
    auto str = renderer.render(2, ast);

    ASSERT_EQ(str, "asdf");

}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
};
