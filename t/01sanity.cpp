#include "../src/context.h"
#include "../src/lexer.h"
#include "../src/parser.h"
#include "../src/renderer.h"
#include "../src/dialect.h"
#include "../src/cppvariable.h"

#include <gtest/gtest.h>

using namespace std;
using namespace Liquid;

Context& getContext() {
    static bool setup = false;
    static Context context;
    if (!setup) {
        StandardDialect::implement(context);
        setup = true;
    }
    return context;
}
Parser& getParser() {
    static Parser parser(getContext());
    return parser;
}


TEST(sanity, literal) {
    auto ast = getParser().parse("asdf");
    CPPVariable variable;
    auto str = getContext().render(ast, variable);
    ASSERT_EQ(str, "asdf");
}

TEST(sanity, variable) {
    CPPVariable variable;
    variable["a"] = 3;
    auto ast = getParser().parse("{{ a }}");
    auto str = getContext().render(ast, variable);
    ASSERT_EQ(str, "3");
}

TEST(sanity, addition) {
    CPPVariable variable;
    variable["a"] = 3;
    auto ast = getParser().parse("asdbfsdf {{ a + 1 + 2 }} b");
    auto str = getContext().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 6 b");
}

TEST(sanity, subtraction) {
    CPPVariable variable;
    variable["a"] = 3;
    auto ast = getParser().parse("asdbfsdf {{ a - 1 + 2 }} b");
    auto str = getContext().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 4 b");
}

TEST(sanity, multiply) {
    CPPVariable variable;
    variable["a"] = 3;
    auto ast = getParser().parse("asdbfsdf {{ a * 2 }} b");
    auto str = getContext().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 6 b");
}


TEST(sanity, divide) {
    CPPVariable variable;
    variable["a"] = 3;
    auto ast = getParser().parse("asdbfsdf {{ a / 2 }} b");
    auto str = getContext().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 1 b");
}

TEST(sanity, orderOfOperations) {
    CPPVariable variable;
    Parser::Node ast;
    std::string str;
    variable["a"] = 3;
    ast = getParser().parse("asdbfsdf {{ a+3 * 6 }} b");
    str = getContext().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 21 b");
    ast = getParser().parse("asdbfsdf {{ a + 3 * 6 }} b");
    str = getContext().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 21 b");
}


TEST(sanity, dot) {
    CPPVariable variable, hash;
    hash["b"] = 2;
    variable["a"] = std::move(hash);
    auto ast = getParser().parse("asdbfsdf {{ a.b }} b");
    auto str = getContext().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 2 b");
}


TEST(sanity, parenthesis) {
    CPPVariable variable;
    variable["a"] = 3;
    auto ast = getParser().parse("asdbfsdf {{ a - (1 + 2) }} b");
    auto str = getContext().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 0 b");
}



TEST(sanity, dereference) {
    CPPVariable variable, hash;
    hash["b"] = 2;
    variable["a"] = std::move(hash);
    auto ast = getParser().parse("asdbfsdf {{ a[\"b\"] }} b");
    auto str = getContext().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 2 b");
}


TEST(sanity, ifstatement) {
    CPPVariable variable, hash;
    Parser::Node ast;
    std::string str;
    hash["b"] = 2;

    variable["a"] = "test";
    ast = getParser().parse("a{% if a == \"test2\" %}1{% else %}5{% endif %} b");
    str = getContext().render(ast, variable);
    ASSERT_EQ(str, "a5 b");



    variable["a"] = std::move(hash);
    ast = getParser().parse("asdasdasd{% if a %}1{% endif %} dfgsdfg");
    str = getContext().render(ast, variable);
    ASSERT_EQ(str, "asdasdasd1 dfgsdfg");
    variable["a"].clear();
    str = getContext().render(ast, variable);
    ASSERT_EQ(str, "asdasdasd dfgsdfg");

    variable["a"] = 2;
    ast = getParser().parse("a{% if a > 1 %}1{% endif %} b");
    str = getContext().render(ast, variable);
    ASSERT_EQ(str, "a1 b");

    ast = getParser().parse("a{% if a > 2 %}1{% endif %} b");
    str = getContext().render(ast, variable);
    ASSERT_EQ(str, "a b");


    ast = getParser().parse("a{% if a == 2 %}1{% endif %} b");
    str = getContext().render(ast, variable);
    ASSERT_EQ(str, "a1 b");

    variable["a"] = "test";
    ast = getParser().parse("a{% if a == \"test\" %}1{% endif %} b");
    str = getContext().render(ast, variable);
    ASSERT_EQ(str, "a1 b");

    ast = getParser().parse("a{% if a == \"test2\" %}1{% endif %} b");
    str = getContext().render(ast, variable);
    ASSERT_EQ(str, "a b");


    ast = getParser().parse("a{% if a == \"test2\" %}1{% elsif a == \"test\" %}2{% endif %} b");
    str = getContext().render(ast, variable);
    ASSERT_EQ(str, "a2 b");

    ast = getParser().parse("a{% unless a == \"test2\" %}1{% endunless %} b");
    str = getContext().render(ast, variable);
    ASSERT_EQ(str, "a1 b");

    ast = getParser().parse("a{% unless a == \"test\" %}1{% endunless %} b");
    str = getContext().render(ast, variable);
    ASSERT_EQ(str, "a b");
}

TEST(sanity, casestatement) {
    CPPVariable hash;
    Parser::Node ast;
    std::string str;
    hash["b"] = 2;

    ast = getParser().parse("{% case b %}{% when 1 %}3{% when 2 %}7{% else %}8{% endcase %}");
    str = getContext().render(ast, hash);
    ASSERT_EQ(str, "7");
}

TEST(sanity, assignments) {
    CPPVariable variable, hash;
    hash["b"] = 2;
    variable["a"] = std::move(hash);
    Parser::Node ast;
    std::string str;

    ast = getParser().parse("{% assign a = 1 %} {{ a }}");
    CPPVariable copyVariable(variable);
    str = getContext().render(ast, copyVariable);
    ASSERT_EQ(str, " 1");

    ast = getParser().parse("{% assign a.b = 3 %} {{ a.b }}");
    copyVariable = CPPVariable(variable);
    str = getContext().render(ast, copyVariable);
    ASSERT_EQ(str, " 3");


    ast = getParser().parse("{% capture d %}{{ 1 + 3 }}sdfsdfsdf{% endcapture %}dddd{{ d }}ggggg");
    str = getContext().render(ast, copyVariable);
    ASSERT_EQ(str, "dddd4sdfsdfsdfggggg");
}

TEST(sanity, forloop) {
    CPPVariable array = { 1, 5, 10, 20 };
    CPPVariable hash = { };
    hash["list"] = std::move(array);
    Parser::Node ast;
    std::string str;

    ast = getParser().parse("{% for i in list %}{{ i }}{% endfor %}");
    str = getContext().render(ast, hash);
    ASSERT_EQ(str, "151020");

    ast = getParser().parse("{% for i in list %}{{ forloop.index0 }}{% endfor %}");
    str = getContext().render(ast, hash);
    ASSERT_EQ(str, "0123");

    ast = getParser().parse("{% for i in list %}{% cycle \"A\", \"B\" %}{% endfor %}");
    str = getContext().render(ast, hash);
    ASSERT_EQ(str, "ABAB");
}


TEST(sanity, filters) {
    CPPVariable hash = { };
    hash["a"] = 1;
    Parser::Node ast;
    std::string str;

    ast = getParser().parse("{% assign a = a | plus: 5 %}{{ a }}");
    str = getContext().render(ast, hash);
    ASSERT_EQ(str, "6");

    hash["a"] = 1;
    ast = getParser().parse("{% assign a = a | plus: 1 | plus: 6 %}{{ a }}");
    str = getContext().render(ast, hash);
    ASSERT_EQ(str, "8");


    hash["a"] = 1;
    ast = getParser().parse("{% assign a = a | plus: 1 | plus: 6 | minus: 3 %}{{ a }}");
    str = getContext().render(ast, hash);
    ASSERT_EQ(str, "5");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
};
