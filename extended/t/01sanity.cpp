#include "../src/context.h"
#include "../src/lexer.h"
#include "../src/parser.h"
#include "../src/renderer.h"
#include "../src/dialect.h"
#include "../src/cppvariable.h"

#include <gtest/gtest.h>
#include <sys/time.h>

using namespace std;
using namespace Liquid;

Context& getContext() {
    static bool setup = false;
    static Context context;
    if (!setup) {
        StandardDialect::implementPermissive(context);
        #ifdef LIQUID_INCLUDE_WEB_DIALECT
            WebDialect::implement(context);
        #endif
        setup = true;
    }
    return context;
}

Renderer& getRenderer() {
    static Renderer renderer(getContext(), CPPVariableResolver());
    return renderer;
}
Parser& getParser() {
    static Parser parser(getContext());
    return parser;
}

TEST(sanity, extended) {

    CPPVariable hash = { };
    hash["a"] = 1;
    Node ast;
    std::string str;
	
    ast = getParser().parse("{% scss %}.a { .b { margin-left: 2px; } }{% endscss %}");
    str = getRenderer().render(ast, hash);
    ASSERT_EQ(str, ".a .b {\n  margin-left: 2px; }\n");

    ast = getParser().parse("{% minify_css %}.a .b { margin-left: 2px; }{% endminify_css %}");
    str = getRenderer().render(ast, hash);
    ASSERT_EQ(str, ".a .b{margin-left:2px}");

    ast = getParser().parse("{% minify_js %}var a = 1 + 2 + 3 + 4; console.log(a);{% endminify_js %}");
    str = getRenderer().render(ast, hash);
    ASSERT_EQ(str, "var a=1+2+3+4;console.log(a);");

}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
};
