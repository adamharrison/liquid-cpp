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
        StandardDialect::implement(context);
        context.registerType<CPPVariableNode>();
        setup = true;
    }
    return context;
}

Renderer& getRenderer() {
    static Renderer renderer(getContext());
    return renderer;
}
Parser& getParser() {
    static Parser parser(getContext());
    return parser;
}


TEST(sanity, literal) {
    auto ast = getParser().parse("asdf");
    CPPVariable variable;
    auto str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "asdf");
}

TEST(sanity, variable) {
    CPPVariable variable;
    variable["a"] = 3;
    auto ast = getParser().parse("{{ a }}");
    auto str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "3");
}

TEST(sanity, addition) {
    CPPVariable variable;
    variable["a"] = 3;
    auto ast = getParser().parse("asdbfsdf {{ a + 1 + 2 }} b");
    auto str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 6 b");
}

TEST(sanity, subtraction) {
    CPPVariable variable;
    variable["a"] = 3;
    Node ast;
    std::string str;

    ast = getParser().parse("asdbfsdf {{ -a }} b");
    str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf -3 b");

    ast = getParser().parse("asdbfsdf {{ a - 1 + 2 }} b");
    str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 4 b");
}

TEST(sanity, multiply) {
    CPPVariable variable;
    variable["a"] = 3;
    auto ast = getParser().parse("asdbfsdf {{ a * 2 }} b");
    auto str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 6 b");
}


TEST(sanity, divide) {
    CPPVariable variable;
    variable["a"] = 3;
    auto ast = getParser().parse("asdbfsdf {{ a / 2 }} b");
    auto str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 1 b");
}

TEST(sanity, orderOfOperations) {
    CPPVariable variable;
    Node ast;
    std::string str;
    variable["a"] = 3;
    ast = getParser().parse("asdbfsdf {{ a+3 * 6 }} b");
    str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 21 b");
    ast = getParser().parse("asdbfsdf {{ a + 3 * 6 }} b");
    str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 21 b");
}


TEST(sanity, dot) {
    CPPVariable variable, hash;
    hash["b"] = 2;
    variable["a"] = std::move(hash);
    auto ast = getParser().parse("asdbfsdf {{ a.b }} b");
    auto str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 2 b");
}


TEST(sanity, parenthesis) {
    CPPVariable variable;
    variable["a"] = 3;
    auto ast = getParser().parse("asdbfsdf {{ a - (1 + 2) }} b");
    auto str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 0 b");
}

TEST(sanity, raw) {
    CPPVariable variable;
    variable["a"] = 3;
    auto ast = getParser().parse("asdbfsdf {% raw %}{{ a - (1 + 2) }}{% endraw %} b");
    auto str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf {{ a - (1 + 2) }} b");
}



TEST(sanity, dereference) {
    CPPVariable variable, hash;
    hash["b"] = 2;
    variable["a"] = std::move(hash);
    auto ast = getParser().parse("asdbfsdf {{ a[\"b\"] }} b");
    auto str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 2 b");
}



TEST(sanity, whitespace) {
    CPPVariable variable, hash;
    Node ast;
    std::string str;
    hash["b"] = 2;
    variable["a"] = std::move(hash);
    ast = getParser().parse("asdbfsdf        {{ 1 }} b");
    str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf        1 b");

    ast = getParser().parse("asdbfsdf        {{- 1 }} b");
    str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf1 b");

    ast = getParser().parse("asdbfsdf        {{- 1 -}} b");
    str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf1b");
    ast = getParser().parse("asdbfsdf        {{- 1 -}}b");
    str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "asdbfsdf1b");
}


TEST(sanity, ifstatement) {
    CPPVariable variable, hash;
    Node ast;
    std::string str;
    hash["b"] = 2;

    variable["a"] = "test";
    ast = getParser().parse("a{% if a == \"test2\" %}1{% else %}5{% endif %} b");
    str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "a5 b");



    variable["a"] = std::move(hash);
    ast = getParser().parse("asdasdasd{% if a %}1{% endif %} dfgsdfg");
    str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "asdasdasd1 dfgsdfg");
    variable["a"].clear();
    str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "asdasdasd dfgsdfg");

    variable["a"] = 2;
    ast = getParser().parse("a{% if a > 1 %}1{% endif %} b");
    str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "a1 b");

    ast = getParser().parse("a{% if a > 2 %}1{% endif %} b");
    str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "a b");


    ast = getParser().parse("a{% if a == 2 %}1{% endif %} b");
    str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "a1 b");

    variable["a"] = "test";
    ast = getParser().parse("a{% if a == \"test\" %}1{% endif %} b");
    str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "a1 b");

    ast = getParser().parse("a{% if a == \"test2\" %}1{% endif %} b");
    str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "a b");


    ast = getParser().parse("a{% if a == \"test2\" %}1{% elsif a == \"test\" %}2{% endif %} b");
    str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "a2 b");

    ast = getParser().parse("a{% unless a == \"test2\" %}1{% endunless %} b");
    str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "a1 b");

    ast = getParser().parse("a{% unless a == \"test\" %}1{% endunless %} b");
    str = getRenderer().render(ast, variable);
    ASSERT_EQ(str, "a b");
}

TEST(sanity, casestatement) {
    CPPVariable hash;
    Node ast;
    std::string str;
    hash["b"] = 2;

    ast = getParser().parse("{% case b %}{% when 1 %}3{% when 2 %}7{% else %}8{% endcase %}");
    str = getRenderer().render(ast, hash);
    ASSERT_EQ(str, "7");
}

TEST(sanity, assignments) {
    CPPVariable variable, hash;
    hash["b"] = 2;
    variable["a"] = std::move(hash);
    Node ast;
    std::string str;

    ast = getParser().parse("{% assign a = 1 %} {{ a }}");
    CPPVariable copyVariable(variable);
    str = getRenderer().render(ast, copyVariable);
    ASSERT_EQ(str, " 1");

    ast = getParser().parse("{% assign a.b = 3 %} {{ a.b }}");
    copyVariable = CPPVariable(variable);
    str = getRenderer().render(ast, copyVariable);
    ASSERT_EQ(str, " 3");


    ast = getParser().parse("{% capture d %}{{ 1 + 3 }}sdfsdfsdf{% endcapture %}dddd{{ d }}ggggg");
    str = getRenderer().render(ast, copyVariable);
    ASSERT_EQ(str, "dddd4sdfsdfsdfggggg");
}

TEST(sanity, forloop) {
    CPPVariable array = { 1, 5, 10, 20 };
    CPPVariable hash = { };
    hash["list"] = std::move(array);
    Node ast;
    std::string str;

    ast = getParser().parse("{% for i in (3..5) %}{{ i }}{% endfor %}");
    str = getRenderer().render(ast, hash);
    ASSERT_EQ(str, "345");


    ast = getParser().parse("{% for i in (0..2) %}{{ i }}{% endfor %}");
    str = getRenderer().render(ast, hash);
    ASSERT_EQ(str, "012");

    ast = getParser().parse("{% for i in list reversed %}{{ i }}{% else %}fdsfdf{% endfor %}");
    str = getRenderer().render(ast, hash);
    ASSERT_EQ(str, "201051");

    ast = getParser().parse("{% for i in list %}{{ i }}{% endfor %}");
    str = getRenderer().render(ast, hash);
    ASSERT_EQ(str, "151020");

    ast = getParser().parse("{% for i in list %}{{ forloop.index0 }}{% endfor %}");
    str = getRenderer().render(ast, hash);
    ASSERT_EQ(str, "0123");

    ast = getParser().parse("{% for i in list %}{% cycle \"A\", \"B\" %}{% endfor %}");
    str = getRenderer().render(ast, hash);
    ASSERT_EQ(str, "ABAB");


    ast = getParser().parse("{% for i in list %}{{ i }}{% break %}3{% endfor %}");
    str = getRenderer().render(ast, hash);
    ASSERT_EQ(str, "1");


    ast = getParser().parse("{% for i in list %}{{ i }}{% continue %}2{% endfor %}");
    str = getRenderer().render(ast, hash);
    ASSERT_EQ(str, "151020");


    ast = getParser().parse("{% for i in list %}{{ i }}{% endfor %}");
    str = getRenderer().render(ast, hash);
    ASSERT_EQ(str, "151020");


    ast = getParser().parse("{% for i in lissdfsdft %}{{ i }}{% else %}fdsfdf{% endfor %}");
    str = getRenderer().render(ast, hash);
    ASSERT_EQ(str, "fdsfdf");

}


TEST(sanity, filters) {
    CPPVariable hash = { };
    hash["a"] = 1;
    Node ast;
    std::string str;

    ast = getParser().parse("{% assign a = a | plus: 5 %}{{ a }}");
    str = getRenderer().render(ast, hash);
    ASSERT_EQ(str, "6");

    hash["a"] = 1;
    ast = getParser().parse("{% assign a = a | plus: 1 | plus: 6 %}{{ a }}");
    str = getRenderer().render(ast, hash);
    ASSERT_EQ(str, "8");


    hash["a"] = 1;
    ast = getParser().parse("{% assign a = a | plus: 1 | plus: 6 | minus: 3 %}{{ a }}");
    str = getRenderer().render(ast, hash);
    ASSERT_EQ(str, "5");

    hash["a"] = "A B C";
    ast = getParser().parse("{% assign a = a | split: \" \" %}{{ a | size }}");
    str = getRenderer().render(ast, hash);
    ASSERT_EQ(str, "3");


    hash["a"] = CPPVariable({ 1, 2, 3, 4 });
    ast = getParser().parse("{{ a.size }}");
    str = getRenderer().render(ast, hash);
    ASSERT_EQ(str, "4");
}

TEST(sanity, composite) {
    CPPVariable hash, order, transaction;
    Node ast;
    std::string str;

    CPPVariable line_item, product, product_option1, product_option2;
    product_option1["name"] = "Color";
    product_option2["name"] = "Size";
    product["options"] = CPPVariable({ product_option1, product_option2 });
    line_item["variant_title"] = "Red / adsfds";
    line_item["product"] = move(product);
    hash["line_item"] = move(line_item);

    ast = getParser().parse("{% for i in (0..2) %}{% if line_item.product.options[i].name == \"Color\" %}{% assign groups = line_item.variant_title | split: \" / \" %}{{ groups[i] }}{% endif %}{% endfor %}");
    str = getRenderer().render(ast, hash);
    ASSERT_EQ(str, "Red");


    transaction["id"] = 12445324;
    transaction["kind"] = "refund";
    transaction["status"] = "success";
    order["transactions"] = CPPVariable({ transaction });

    hash["order"] = order;


    ast = getParser().parse("{% for trans in order.transactions %}{% if trans.status == 'success' and trans.kind == 'refund' %}{% unless forloop.first %}, {% endunless %}{{ trans.id }}{% endif %}{% endfor %}");
    str = getRenderer().render(ast, hash);
    ASSERT_EQ(str, "12445324");


}

TEST(sanity, error) {
    CPPVariable hash = { };
    hash["a"] = 1;
    Node ast;
    std::string str;

    ASSERT_THROW({
        ast = getParser().parse("{% assign a = a | plus: 5");
    }, Parser::Exception);

    ASSERT_THROW({
        ast = getParser().parse("{% assign a a | plus: 5 %}");
    }, Parser::Exception);

}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
};
