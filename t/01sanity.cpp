#include "../src/context.h"
#include "../src/lexer.h"
#include "../src/parser.h"
#include "../src/renderer.h"
#include "../src/compiler.h"
#include "../src/optimizer.h"
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
Compiler& getCompiler() {
    static Compiler compiler(getContext());
    return compiler;
}
Interpreter& getInterpreter() {
    static Interpreter interpreter(getContext(), CPPVariableResolver());
    return interpreter;
}
Parser& getParser() {
    static Parser parser(getContext());
    return parser;
}

Optimizer& getOptimizer() {
    static Optimizer optimizer(getRenderer());
    return optimizer;
}

string renderTemplate(const Node& ast, Variable variable) {
    // Whether these tests run on the interpreter or on the renderer.
    return getRenderer().render(ast, variable);
    //fprintf(stderr, "DISASSEMBLY: \n%s\n", getCompiler().disassemble(getCompiler().compile(ast)).data());
    //return getInterpreter().renderTemplate(getCompiler().compile(ast), variable);
}

TEST(sanity, literal) {
    auto ast = getParser().parse("asdf");
    CPPVariable variable;
    auto str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "asdf");
}

TEST(sanity, variable) {
    CPPVariable variable;
    variable["a"] = 3;
    auto ast = getParser().parse("{{ a }}");
    auto str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "3");
}

TEST(sanity, addition) {
    CPPVariable variable;
    variable["a"] = 3;
    auto ast = getParser().parse("asdbfsdf {{ a + 1 + 2 }} b");
    auto str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 6 b");
}

TEST(sanity, subtraction) {
    CPPVariable variable;
    variable["a"] = 3;
    Node ast;
    std::string str;

    ast = getParser().parse("asdbfsdf {{ -a }} b");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "asdbfsdf -3 b");

    ast = getParser().parse("asdbfsdf {{ a - 1 + 2 }} b");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 4 b");



}

TEST(sanity, multiply) {
    CPPVariable variable;
    variable["a"] = 3;
    auto ast = getParser().parse("asdbfsdf {{ a * 2 }} b");
    auto str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 6 b");
}


TEST(sanity, divide) {
    CPPVariable variable;
    variable["a"] = 3;
    auto ast = getParser().parse("asdbfsdf {{ a / 2 }} b");
    auto str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 1 b");
}

TEST(sanity, orderOfOperations) {
    CPPVariable variable;
    Node ast;
    std::string str;
    variable["a"] = 3;
    ast = getParser().parse("asdbfsdf {{ a+3 * 6 }} b");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 21 b");
    ast = getParser().parse("asdbfsdf {{ a + 3 * 6 }} b");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 21 b");
}


TEST(sanity, dot) {
    CPPVariable variable, hash;
    hash["b"] = 2;
    variable["a"] = std::move(hash);
    auto ast = getParser().parse("asdbfsdf {{ a.b }} b");
    auto str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 2 b");
}


TEST(sanity, parenthesis) {
    CPPVariable variable;
    variable["a"] = 3;
    auto ast = getParser().parse("asdbfsdf {{ a - (1 + 2) }} b");
    auto str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 0 b");
}

TEST(sanity, lexingHalts) {
    CPPVariable variable;
    variable["a"] = 3;
    auto ast = getParser().parse("asdbfsdf {% raw %}{{ a - (1 + 2) }}{% endraw %} b");
    auto str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "asdbfsdf {{ a - (1 + 2) }} b");

    ast = getParser().parse("asdbfsdf {% comment %}{{ a - (1 + 2) }}{% endcomment %} b");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(getParser().errors.size(), 0);
    ASSERT_EQ(str, "asdbfsdf  b");

    ast = getParser().parse("asdbfsdf {% comment %}{{ a - (1 + 2) dfghfhdfgh}}{% endcomment %} b");
    ASSERT_EQ(getParser().errors.size(), 0);
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "asdbfsdf  b");
}



TEST(sanity, dereference) {
    CPPVariable variable, hash;
    hash["b"] = 2;
    variable["a"] = std::move(hash);
    auto ast = getParser().parse("asdbfsdf {{ a[\"b\"] }} b");
    auto str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 2 b");
}



TEST(sanity, whitespace) {
    CPPVariable variable, hash;
    Node ast;
    std::string str;
    hash["b"] = 2;
    variable["a"] = std::move(hash);



    // UTF-8 whitespace.
    ast = getParser().parse("asdbfsdf             {{- 1 -}}b");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "asdbfsdf1b");

    ast = getParser().parse("asdbfsdf        {{ 1 }} b");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "asdbfsdf        1 b");

    ast = getParser().parse("asdbfsdf        {{- 1 }} b");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "asdbfsdf1 b");

    ast = getParser().parse("asdbfsdf        {{- 1 -}} b");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "asdbfsdf1b");
    ast = getParser().parse("asdbfsdf        {{- 1 -}}b");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "asdbfsdf1b");

}


TEST(sanity, ifstatement) {
    CPPVariable variable, hash;
    Node ast;
    std::string str;
    hash["b"] = 2;


    variable["a"] = "test";
    ast = getParser().parse("a{% if a == \"test2\" %}1{% elsif a == \"test\" %}2{% endif %} b");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "a2 b");

    variable["a"] = "test";
    ast = getParser().parse("a{% if a == \"test2\" %}1{% else %}5{% endif %} b");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "a5 b");



    variable["a"] = std::move(hash);
    ast = getParser().parse("asdasdasd{% if a %}1{% endif %} dfgsdfg");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "asdasdasd1 dfgsdfg");
    variable["a"].clear();
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "asdasdasd dfgsdfg");

    variable["a"] = 2;
    ast = getParser().parse("a{% if a > 1 %}1{% endif %} b");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "a1 b");

    ast = getParser().parse("a{% if a > 2 %}1{% endif %} b");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "a b");


    ast = getParser().parse("a{% if a == 2 %}1{% endif %} b");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "a1 b");

    variable["a"] = "test";
    ast = getParser().parse("a{% if a == \"test\" %}1{% endif %} b");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "a1 b");

    ast = getParser().parse("a{% if a == \"test2\" %}1{% endif %} b");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "a b");


    ast = getParser().parse("a{% if a == \"test2\" %}1{% elsif a == \"test\" %}2{% endif %} b");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "a2 b");

    ast = getParser().parse("a{% unless a == \"test2\" %}1{% endunless %} b");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "a1 b");

    ast = getParser().parse("a{% unless a == \"test\" %}1{% endunless %} b");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "a b");
}

TEST(sanity, casestatement) {
    CPPVariable hash;
    Node ast;
    std::string str;
    hash["b"] = 2;

    ast = getParser().parse("{% case b %}{% when 1 %}3{% when 2 %}7{% else %}8{% endcase %}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "7");
}

TEST(sanity, liquidtag) {
    CPPVariable hash;
    Node ast;
    std::string str;
    hash["b"] = 2;

    // I cannot believe that this is how they do it. Literally they treat newlines as special whitespace, rather than
    // actually doing a proper parse and disambiguating based on keywords. I rolled my eyes so hard; they probably actually used a regex.
    ast = getParser().parse("{% liquid\n\
    case b\n\
        when 1\n\
            echo '3'\n\
        when 2\n\
            echo '7'\n\
        else\n\
            echo '8'\n\
    endcase %}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "7");
}

TEST(sanity, assignments) {
    CPPVariable variable, hash;
    hash["b"] = 2;
    variable["a"] = std::move(hash);
    Node ast;
    std::string str;


    ast = getParser().parse("{% capture d %}{{ 1 + 3 }}sdfsdfsdf{% endcapture %}dddd{{ d }}ggggg");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "dddd4sdfsdfsdfggggg");

    ast = getParser().parse("{% assign a = 1 %} {{ a }}");
    CPPVariable copyVariable(variable);
    str = renderTemplate(ast, copyVariable);
    ASSERT_EQ(str, " 1");

    ast = getParser().parse("{% assign a.b = 3 %} {{ a.b }}");
    copyVariable = CPPVariable(variable);
    str = renderTemplate(ast, copyVariable);
    ASSERT_EQ(str, " 3");



}

TEST(sanity, forloop) {
    CPPVariable array = { 1, 5, 10, 20 };
    CPPVariable hash = { };
    hash["list"] = std::move(array);
    Node ast;
    std::string str;

    ast = getParser().parse("{% for i in list %}{% unless forloop.first %},{% endunless%}{{ forloop.index0 }}{% endfor %}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "0,1,2,3");


    ast = getParser().parse("{% for i in (3..5) %}{{ i }}{% endfor %}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "345");


    ast = getParser().parse("{% for i in (0..2) %}{{ i }}{% endfor %}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "012");

    ast = getParser().parse("{% for i in list reversed %}{{ i }}{% else %}fdsfdf{% endfor %}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "201051");


    ast = getParser().parse("{% for i in list limit: 2 %}{{ i }}{% else %}fdsfdf{% endfor %}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "15");

    ast = getParser().parse("{% for i in list limit: 2 offset: 2 %}{{ i }}{% else %}fdsfdf{% endfor %}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "1020");

    ast = getParser().parse("{% for i in list %}{{ i }}{% endfor %}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "151020");

    ast = getParser().parse("{% for i in list %}{{ forloop.index0 }}{% endfor %}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "0123");

    ast = getParser().parse("{% for i in list %}{% cycle \"A\", \"B\" %}{% endfor %}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "ABAB");


    ast = getParser().parse("{% for i in list %}{{ i }}{% break %}3{% endfor %}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "1");


    ast = getParser().parse("{% for i in list %}{{ i }}{% continue %}2{% endfor %}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "151020");


    ast = getParser().parse("{% for i in list %}{{ i }}{% endfor %}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "151020");


    ast = getParser().parse("{% for i in lissdfsdft %}{{ i }}{% else %}fdsfdf{% endfor %}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "fdsfdf");



}


TEST(sanity, negation) {
    CPPVariable hash, internal;

    internal["b"] = 0;
    internal["c"] = 1;
    hash["a"] = move(internal);
    Node ast;
    std::string str;


    ast = getParser().parse("{{ !a.b }} {{ !a.c }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "true false");
}

TEST(sanity, specialLiterals) {
    CPPVariable hash, internal;
    Node ast;
    std::string str;

    ast = getParser().parse("{% assign a = true %}{{ a }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "true");
}


TEST(sanity, argumentContext) {
    CPPVariable hash, internal;
    Node ast;
    std::string str;

    hash["a"] = 1;
    internal["title"] = "The Greatest Thing Ever";
    hash["product"] = move(internal);

    ast = getParser().parseArgument("a + 2");
    Variant result = getRenderer().renderArgument(ast, hash);
    ASSERT_EQ(result.type, Variant::Type::INT);
    ASSERT_EQ(result.i, 3);

    ast = getParser().parseAppropriate("{% if product.title contains 'a' %}1{% else %}0{% endif %}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "1");

}

TEST(sanity, arrayLiterals) {
    CPPVariable hash, internal;
    Node ast;
    std::string str;

    ast = getParser().parse("{% assign a = [1,2] %}A{{ a.last }}B");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "A2B");
}

TEST(sanity, filters) {
    CPPVariable hash = { };
    Node ast;
    std::string str;

    // This should probably be implciit.
    /*hash["now"] = (long long)time(NULL);
    ast = getParser().parse("{% assign wat1 = now | date: '%s' %}{% assign wat2 = '2021-09-23T13:00:00-05:00' | date: '%s' %}{{ wat1 | minus: wat2 }}");
    str = renderTemplate(ast, hash);*/
    /*fprintf(stderr, "WAT: %s\n", Parser::Error::english(getParser().errors[0]).data());
    ASSERT_EQ(getParser().errors.size(), 0);*/

    ASSERT_EQ(str, "1632402000");

    struct TestingFilter : FilterNodeType {
        TestingFilter() : FilterNodeType("testing", -1, -1, true) { }
    };
    getContext().registerType<TestingFilter>();

    hash["a"] = 1;
    ast = getParser().parse("{% assign a = a | testing: a: 2 %}{{ a }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "");



    hash["a"] = "A B C";
    ast = getParser().parse("{% assign a = a | split: \" \" %}{{ a | size }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "3");

    hash["a"] = CPPVariable({ 1, 2, 3, 4, 10 });
    ast = getParser().parse("{{ a.first.size }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "1");
    ast = getParser().parse("{{ a.last.size }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "2");

    hash["a"] = 1;
    ast = getParser().parse("{% assign a = a | plus: 5 %}{{ a }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "6");

    hash["a"] = 1;
    ast = getParser().parse("{% assign a = a | plus: 1 | plus: 6 %}{{ a }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "8");

    ast = getParser().parse("{{ a | plus: 5 | plus: 3 }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "16");


    hash["a"] = 1;
    ast = getParser().parse("{% assign a = a | plus: 1 | plus: 6 | minus: 3 %}{{ a }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "5");



    hash["a"] = CPPVariable({ 1, 2, 3, 4 });
    ast = getParser().parse("{{ a.size }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "4");


    hash["a"] = CPPVariable({ 1, 2, 3, 4 });
    ast = getParser().parse("{{ a.first }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "1");


    ast = getParser().parse("{{ \"2021-09-23T13:00:00-05:00\" | date: \"%s\" }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "1632402000");

}





struct SchemaNode : TagNodeType {
    SchemaNode() : TagNodeType(Composition::ENCLOSED, "schema", 1, 1, LIQUID_OPTIMIZATION_SCHEME_FULL) { }

    Node render(Renderer& renderer, const Node& node, Variable store) const override {
        auto str = renderer.retrieveRenderedNode(*node.children[1].get(), store).getString();
        //fprintf(stderr, "CONTENTS: %s\n", str.c_str());
        return Node();
    }
};


TEST(sanity, optimizer) {
    CPPVariable hash, internal, settings;
    Node ast;
    std::string str;


    Context& ctx = getContext();
    ctx.registerType<SchemaNode>();
    ast =  getParser().parse("AAA{% if section.settings.enabled %}dfasfsdf{% endif %}{% schema %}{\"settings\":[{\"id\":\"enabled\",\"type\":\"checkbox\",\"default\":true}]}{% endschema %}");
    internal["enabled"] = true;
    settings["settings"] = move(internal);
    hash["section"] = move(settings);
    getOptimizer().optimize(ast, hash);
    str = renderTemplate(ast, CPPVariable());
    ASSERT_STREQ(str.data(), "AAAdfasfsdf");


    const NodeType* ifNode = getContext().getTagType("if");
    bool atLeastOne = false;


    hash["a"] = 0;

    ast = getParser().parse("{% if a %}{{ a.b }}{% elsif b == 1 %}d{% else %}c{% endif %}");

    ast.walk([ifNode, &atLeastOne](const Node& node) {
        if (node.type == ifNode)
            atLeastOne = true;
    });
    ASSERT_TRUE(atLeastOne);

    atLeastOne = false;
    getOptimizer().optimize(ast, hash);
    ast.walk([ifNode, &atLeastOne](const Node& node) {
        if (node.type == ifNode)
            atLeastOne = true;
    });

    ASSERT_TRUE(atLeastOne);





    internal["c"] = "D";
    hash["a"] = internal;

    ast = getParser().parse("{% if a %}{{ a.b }}{% endif %}");

    ast.walk([ifNode, &atLeastOne](const Node& node) {
        if (node.type == ifNode)
            atLeastOne = true;
    });
    ASSERT_TRUE(atLeastOne);

    atLeastOne = false;
    getOptimizer().optimize(ast, hash);
    ast.walk([ifNode, &atLeastOne](const Node& node) {
        if (node.type == ifNode)
            atLeastOne = true;
    });

    ASSERT_TRUE(!atLeastOne);

    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "");

    internal["b"] = "C";
    hash["a"] = move(internal);

    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "C");



    hash["a"] = nullptr;
}

TEST(sanity, composite) {
    CPPVariable hash, order, transaction, event;
    Node ast;
    std::string str;


    hash["B"] = "D";
    ast = getParser().parse("A {{ B }} C", sizeof("A {{ B }} C")-1, "testfile");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "A D C");

    CPPVariable lip1, lip2, lip3, lip4;
    CPPVariable line_item, line_item1, line_item2, product, product_option1, product_option2, product_option3;

    lip1["name"] = "Gift Email";
    lip1["value"] = "test1@gmail.com";
    lip2["name"] = "Gift Image";
    lip2["value"] = 1;
    line_item1["id"] = 1;
    line_item1["properties"] = { lip1, lip2 };
    lip3["name"] = "Gift Email";
    lip3["value"] = "test2@gmail.com";
    lip4["name"] = "Gift Image";
    lip4["value"] = 2;
    line_item2["id"] = 1;
    line_item2["properties"] = { lip3, lip4 };
    order["line_items"] = { line_item1, line_item2 };
    hash["order"] = order;
    hash["email"] = "test1@gmail.com";

    ast = getParser().parse("{% for line_item in order.line_items %}{% for property in line_item.properties %}{% if property.name == \"Gift Email\" and property.value == email %}{% assign li = line_item %}{% endif %}{% endfor %}{% endfor %}{{ li.id }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "1");


    ast = getParser().parse("{{ params.state.order-by.test }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "");

    hash["image"] = "ASD";
    ast = getParser().parse("{{ image | default: '/static/img/empty.png' | img_tag }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "<img src='ASD'/>");

    hash.clear();

    ast = getParser().parse("{{ image | default: '/static/img/empty.png' | img_tag }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "<img src='/static/img/empty.png'/>");

    ast = getParser().parse("{{ (series.lowest_review.content | newline_to_br) + \"<br> -- \" + series.lowest_review.user }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "<br> -- ");
    ast = getParser().parse("{% comment %}Test Comment{% endcomment %}DoesntRender");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "DoesntRender");

    event["category"] = "tv";
    hash["event"] = move(event);
    ast = getParser().parse("{{ event.category | capitalize | replace: \"Tv\", \"TV\" }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "TV");


    ast = getParser().parse("{% if a %}asdfghj {{ a }}{% else %}asdfjlsjkhgsjlkhglsdfjkgdfhs{% for i in (1..10) %}{{ i }}fasdfsdf{% endfor %}{% endif %}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "asdfjlsjkhgsjlkhglsdfjkgdfhs1fasdfsdf2fasdfsdf3fasdfsdf4fasdfsdf5fasdfsdf6fasdfsdf7fasdfsdf8fasdfsdf9fasdfsdf10fasdfsdf");


    order["id"] = 2;
    hash["order"] = order;

    ast = getParser().parse("{% assign b = order %}{{ b.id }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "2");





    line_item1["sku"] = "ASDTOPS";
    line_item2["sku"] = "ASDBOTS";
    order["line_items"] = { line_item1, line_item2 };
    hash["order"] = move(order);
    hash["line_item"] = line_item1;


    ast = getParser().parse("{% if line_item.sku contains ' --- ' %}{{ line_item.sku | split: ' --- ' | last }}{% else %}{% assign base = \"ASD\" %}BASE{{ base }}|{% for line in order.line_items %}{% if line.sku != line_item.sku and line.sku contains base %}{{ line.sku }}{% endif %}{% endfor %}{% endif %}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "BASEASD|ASDBOTS");

    product_option1["name"] = "Color";
    product_option2["name"] = "Size";
    product["options"] = CPPVariable({ product_option1, product_option2 });
    line_item["variant_title"] = "Red / adsfds";
    line_item["product"] = move(product);
    hash["line_item"] = move(line_item);

    ast = getParser().parse("{% for i in (0..2) %}{% if line_item.product.options[i].name == \"Color\" %}{% assign groups = line_item.variant_title | split: \" / \" %}{{ groups[i] }}{% endif %}{% endfor %}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "Red");


    CPPVariable variant;
    variant["id"] = 1;
    variant["option1"] = "Red";
    variant["option2"] = "Large";
    variant["option3"] = "Silk";
    product = CPPVariable();
    product["variants"] = { variant };
    product_option1["name"] = "Material";
    product_option1["position"] = 3;
    product_option2["name"] = "Color";
    product_option2["position"] = 1;
    product_option3["name"] = "Size";
    product_option3["position"] = 2;
    product["options"] = { product_option1, product_option2, product_option3 };
    hash["variant"] = move(variant);
    hash["product"] = move(product);

    ast = getParser().parse("{% assign color = 1 %}{% if color %}{{ variant['option1'] }}{{ variant['option' + color] }}{% else %}B{% endif %}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "RedRed");





    transaction["id"] = 12445324;
    transaction["kind"] = "refund";
    transaction["status"] = "success";
    order["transactions"] = CPPVariable({ transaction });

    hash["order"] = order;


    ast = getParser().parse("{% for trans in order.transactions %}{% if trans.status == 'success' and trans.kind == 'refund' %}{% unless forloop.first %}, {% endunless %}{{ trans.id }}{% endif %}{% endfor %}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "12445324");

    hash.clear();
    Liquid::CPPVariable search;
    Liquid::CPPVariable results({ });
    std::unique_ptr<Liquid::CPPVariable> cppResult = std::make_unique<Liquid::CPPVariable>();
    (*cppResult.get())["url"] = string("https://test.com");
    (*cppResult.get())["title"] = string("My Test Thing");
    (*cppResult.get())["description"] = string("A description!");
    results.a.push_back(move(cppResult));
    search["results"] = move(results);
    hash["search"] = move(search);

    ast = getParser().parse("<ul class='search-results'>{% for result in search.results %}<li><a href='{{ result.url }}'><div class='title'>{{ result.title | escape }}</div><div class='description'>{{ result.description | escape }}</div></a></li>{% endfor %}</ul>");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "<ul class='search-results'><li><a href='https://test.com'><div class='title'>My Test Thing</div><div class='description'>A description!</div></a></li></ul>");




}


TEST(sanity, malicious) {
    CPPVariable hash, order, transaction;
    Node ast;
    std::string str;

    /*ast = getParser().parse("{{ nil | default: a: 2 | sort | json }}");
    str = renderTemplate(ast, hash);*/


    ASSERT_ANY_THROW(ast = getParser().parse("{% assign a = (((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((( %}"));
}

TEST(sanity, strict) {
    CPPVariable hash;
    Context context;
    StandardDialect::implementStrict(context);
    WebDialect::implement(context);
    Parser parser(context);
    Renderer renderer(context, CPPVariableResolver());
    Node ast;
    string result;

    hash["a"] = -1;

    ast = parser.parse("{% if a + 2 > 1 %}a{% endif %}");
    result = renderer.render(ast, hash);
    ASSERT_TRUE(parser.errors.size() > 0);
}
TEST(sanity, clang) {
    CPPVariable hash = { };
    LiquidContext context = liquidCreateContext();
    liquidImplementStrictStandardDialect(context);
    LiquidParser parser = liquidCreateParser(context);
    LiquidRenderer renderer = liquidCreateRenderer(context);
    LiquidOptimizer optimizer = liquidCreateOptimizer(renderer);
    liquidRegisterVariableResolver(renderer, CPPVariableResolver());
    LiquidCompiler compiler = liquidCreateCompiler(context);
    char buffer[] = "{% if a %}asdfghj {{ a }}{% else %}asdfjlsjkhgsjlkhglsdfjkgdfhs{% for i in (1..10) %}{{ i }}fasdfsdf{% endfor %}{% endif %}";
    LiquidTemplate tmpl = liquidParserParseTemplate(parser, buffer, strlen(buffer), nullptr, nullptr, nullptr);
    liquidOptimizeTemplate(optimizer, tmpl, &hash);

    LiquidTemplateRender result = liquidRendererRenderTemplate(renderer, &hash, tmpl, nullptr);
    ASSERT_STREQ(liquidTemplateRenderGetBuffer(result), "asdfjlsjkhgsjlkhglsdfjkgdfhs1fasdfsdf2fasdfsdf3fasdfsdf4fasdfsdf5fasdfsdf6fasdfsdf7fasdfsdf8fasdfsdf9fasdfsdf10fasdfsdf");
    liquidFreeTemplateRender(result);


    /*LiquidProgram program = liquidCompilerCompileTemplate(compiler, tmpl);

    char target[10*1024];
    liquidCompilerDisassembleProgram(compiler, program, target, 10*1024);

    for (int i = 0; i < 100000; ++i) {
        LiquidProgramRender result = liquidRendererRunProgram(renderer, &hash, program, nullptr);
        ASSERT_STREQ(result.str, "asdfjlsjkhgsjlkhglsdfjkgdfhs1fasdfsdf2fasdfsdf3fasdfsdf4fasdfsdf5fasdfsdf6fasdfsdf7fasdfsdf8fasdfsdf9fasdfsdf10fasdfsdf");
    }

    liquidFreeProgram(program);*/

    liquidFreeTemplate(tmpl);
    liquidFreeCompiler(compiler);
    liquidFreeOptimizer(optimizer);
    liquidFreeRenderer(renderer);
    liquidFreeParser(parser);
    liquidFreeContext(context);
}

TEST(sanity, vm) {
    /*CPPVariable hash = { };
    Node ast;
    Program program;
    std::string str;
    std::string result;

    hash[0] = 1;
    hash[1] = 3;
    hash[2] = 5;
    hash[3] = 7;


        ast = getParser().parse("{% if a %}asdfghj {{ a }}{% else %}asdfjlsjkhgsjlkhglsdfjkgdfhs{% for i in b %}{{ i }}fasdfsdf{% endfor %}{% endif %}");
        getOptimizer().optimize(ast, hash);
        program = getCompiler().compile(ast);
        result =  getInterpreter().renderTemplate(program, hash);
        ASSERT_EQ(result, "asdfjlsjkhgsjlkhglsdfjkgdfhs1fasdfsdf3fasdfsdf5fasdfsdf7fasdfsdf");

        hash.clear();


        ast = getParser().parse("{% if a %}asdfghj {{ a }}{% else %}asdfjlsjkhgsjlkhglsdfjkgdfhs{% for i in (1..10) %}{{ i }}fasdfsdf{% endfor %}{% endif %}");
        getOptimizer().optimize(ast, hash);
        program = getCompiler().compile(ast);
        hash["a"] = false;
        result =  getInterpreter().renderTemplate(program, hash);
        ASSERT_EQ(result, "asdfjlsjkhgsjlkhglsdfjkgdfhs1fasdfsdf2fasdfsdf3fasdfsdf4fasdfsdf5fasdfsdf6fasdfsdf7fasdfsdf8fasdfsdf9fasdfsdf10fasdfsdf");

        hash["a"] = 1;

        ast = getParser().parse("{% for i in (1..3) %}{{ i }}fasdfsdf{% endfor %}");
        program = getCompiler().compile(ast);
        result = getInterpreter().renderTemplate(program, hash);
        ASSERT_EQ(result, "1fasdfsdf2fasdfsdf3fasdfsdf");



        ast = getParser().parse("jaslkdjfasdkf {{ a }} kjhdfjkhgsdfg");
        program = getCompiler().compile(ast);
        result = getInterpreter().renderTemplate(program, hash);
        ASSERT_EQ(result, "jaslkdjfasdkf 1 kjhdfjkhgsdfg");



        ast = getParser().parse("asdlsjaflkdhsfjlkdshf");
        program = getCompiler().compile(ast);
        result = getInterpreter().renderTemplate(program, hash);
        ASSERT_EQ(result, "asdlsjaflkdhsfjlkdshf");


        ast = getParser().parse("kjhafsdjkhfjkdhsf {{ a + 1 }} jhafsdkhgsdfjkg");
        program = getCompiler().compile(ast);
        result = getInterpreter().renderTemplate(program, hash);
        ASSERT_EQ(result, "kjhafsdjkhfjkdhsf 2 jhafsdkhgsdfjkg");

        ast = getParser().parse("kjhafsdjkhfjkdhsf {{ a + 1 + 2 }} jhafsdkhgsdfjkg");
        program = getCompiler().compile(ast);
        result = getInterpreter().renderTemplate(program, hash);
        ASSERT_EQ(result, "kjhafsdjkhfjkdhsf 4 jhafsdkhgsdfjkg");

        ast = getParser().parse("fahsdjkhgflghljh {% if a %}A{% else %}B{% endif %} kjdjkghdf");
        program = getCompiler().compile(ast);
        result = getInterpreter().renderTemplate(program, hash);
        ASSERT_EQ(result, "fahsdjkhgflghljh A kjdjkghdf");


        ast = getParser().parse("asdjkfsdhsjkg {{ a | plus: 3 | minus: 5 | times: 6 }}");
        program = getCompiler().compile(ast);
        result = getInterpreter().renderTemplate(program, hash);
        ASSERT_EQ(result, "asdjkfsdhsjkg -6");*/



}


TEST(sanity, error) {
    CPPVariable hash = { };
    hash["a"] = 1;
    Node ast;
    std::string str;

    ASSERT_NO_THROW({
        ast = getParser().parse("{% for %}{% endfor %}");
    });
    ASSERT_TRUE(getParser().errors.size() > 0);

    ASSERT_NO_THROW({
        ast = getParser().parse("{% endif %}");
    });
    ASSERT_EQ(getParser().errors.size(), 1);


    ASSERT_THROW({
        ast = getParser().parse("{% assign a = a | plus: 5");
    }, Parser::Exception);

    ASSERT_NO_THROW({
        ast = getParser().parse("{% assign a a | plus: 5 %}");
    });
    ASSERT_EQ(getParser().errors.size(), 1);

    auto context = liquidCreateContext();
    liquidImplementPermissiveStandardDialect(context);
    auto parser = liquidCreateParser(context);
    LiquidLexerError lexerError;
    LiquidParserError parserError;
    LiquidTemplate tmpl = liquidParserParseTemplate(parser, "sdfosidfj{{ fasdf", sizeof("sdfosidfj{{ fasdf")-1, "", &lexerError, &parserError);
    ASSERT_TRUE(!tmpl.ast);
    liquidFreeTemplate(tmpl);
    liquidFreeParser(parser);
    liquidFreeContext(context);
}

#ifdef LIQUID_INCLUDE_RAPIDJSON_VARIABLE

#include "../src/rapidjsonvariable.h"

TEST(sanity, rj) {

    Node ast;

    Renderer renderer(getContext(), RapidJSONVariableResolver());
    ast = getParser().parse("{{ a }}");
    rapidjson::Document d;

    d.Parse("{\"a\":\"b\"}");

    string str = renderer.render(ast, &d);

    ASSERT_EQ(str, "b");
}

#endif

#ifdef LIQUID_INCLUDE_WEB_DIALECT

TEST(sanity, web) {

    CPPVariable hash = { };
    hash["a"] = 1;
    Node ast;
    std::string str;

    ast = getParser().parse("{{ '<html>' | escape }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "&lt;html&gt;");

    ast = getParser().parse("{{ 'a' | md5 }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "0cc175b9c0f1b6a831c399e269772661");

    ast = getParser().parse("{{ 'a' | sha1 }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "86f7e437faa5a7fce15d1ddcb9eaeaea377667b8");

    ast = getParser().parse("{{ 'a' | sha256 }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb");

    ast = getParser().parse("{{ 'a' | hmac_sha1: 'b' }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "8abe0fd691e3da3035f7b7ac91be45d99e942b9e");

    ast = getParser().parse("{{ 'a' | hmac_sha256: 'b' }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "cb448b440c42ac8ad084fc8a8795c98f5b7802359c305eabd57ecdb20e248896");

    ast = getParser().parse("{{ '<html></html>' | escape }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "&lt;html&gt;&lt;/html&gt;");

    ast = getParser().parse("{{ 1608524371 | date: \"%B %d, %Y\" }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "December 20, 2020");

    ast = getParser().parse("{{ \"tefasdfsdf\" | link_to: \"//test.com\", \"titletest\" }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "<a title=\"titletest\" href=\"//test.com\">tefasdfsdf</a>");
}

#endif


TEST(sanity, unparser) {
    CPPVariable variable, hash, theme;
    variable["a"] = 3;
    Node ast;


    /*std::unique_ptr<NodeType> registeredType = make_unique<TagNodeType>(TagNodeType::Composition::ENCLOSED, "stylesheet", 0, 1, LIQUID_OPTIMIZATION_SCHEME_NONE);
    registeredType->userRenderFunction = +[](LiquidRenderer renderer, LiquidNode node, void* variableStore, void* data) { };
    getContext().registerType(move(registeredType));

    theme["name"] = "Minimal";
    hash["theme"] = move(theme);

    FILE* file = fopen("/tmp/test.liquid", "rb");
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);
    string buffer;
    buffer.resize(size);
    fread(&buffer[0], size, 1, file);
    ast = getParser().parse(buffer, "test");
    getOptimizer().optimize(ast, hash);
//    sleep(5);
    fprintf(stderr, "WAT: %s\n", getParser().unparse(ast).data());
    exit(0);*/


    ast = getParser().parse("{{ 'asdlkhsdjgasjlk.product.test' | t }}");
    ASSERT_EQ(getParser().unparse(ast), "{{ \"asdlkhsdjgasjlk.product.test\" | t }}");

    ast = getParser().parse("{{ \"spot.js\" | asset_url | script_tag }}");
    ASSERT_EQ(getParser().unparse(ast), "{{ \"spot.js\" | asset_url | script_tag }}");


    ast = getParser().parse("{% if theme.name == \"Test1\" %}1{% elsif theme.name contains \"Test2\" %}3{% endif %}");
    ASSERT_EQ(getParser().unparse(ast), "{% if theme.name == \"Test1\" %}1{% elsif theme.name contains \"Test2\" %}3{% endif %}");


    ast = getParser().parse("{% if theme.name contains 'Brooklyn' %}\
        productContainer = document.querySelector('.template-collection .grid, .template-search .grid .grid-uniform');\
    {% elsif theme.name contains 'Narrative' %}\
        productContainer = document.querySelector('.template-collection .grid:nth-child(2), .template-search .grid');\
    {% elsif theme.name contains 'Minimal' -%}\
        productContainer = document.querySelector('.template-collection .grid .grid, .template-collection .grid .grid-uniform, .template-search .grid .grid');\
    {%- elsif theme.name contains 'Venture' %}\
        productContainer = document.querySelector('.template-collection #MainContent .grid--uniform, .template-search #MainContent .grid--uniform');\
    {% elsif theme.name contains 'Boundless' %}\
        productContainer = document.querySelector('.template-collection .collection-grid, .template-search .search-template-section .grid--uniform');\
    {% elsif theme.name contains 'Debut' %}\
        productContainer = document.querySelector('#Collection .grid, .template-search #MainContent ul');\
    {% endif %}");

    ASSERT_EQ(getParser().unparse(ast), "{% if theme.name contains \"Brooklyn\" %}\
        productContainer = document.querySelector('.template-collection .grid, .template-search .grid .grid-uniform');\
    {% elsif theme.name contains \"Narrative\" %}\
        productContainer = document.querySelector('.template-collection .grid:nth-child(2), .template-search .grid');\
    {% elsif theme.name contains \"Minimal\" %}productContainer = document.querySelector('.template-collection .grid .grid, .template-collection .grid .grid-uniform, .template-search .grid .grid');{% elsif theme.name contains \"Venture\" %}\
        productContainer = document.querySelector('.template-collection #MainContent .grid--uniform, .template-search #MainContent .grid--uniform');\
    {% elsif theme.name contains \"Boundless\" %}\
        productContainer = document.querySelector('.template-collection .collection-grid, .template-search .search-template-section .grid--uniform');\
    {% elsif theme.name contains \"Debut\" %}\
        productContainer = document.querySelector('#Collection .grid, .template-search #MainContent ul');\
    {% endif %}");

    theme["name"] = "Minimal";
    hash["theme"] = move(theme);
    getOptimizer().optimize(ast, hash);

    ASSERT_EQ(getParser().unparse(ast), "productContainer = document.querySelector('.template-collection .grid .grid, .template-collection .grid .grid-uniform, .template-search .grid .grid');");

    ast = getParser().parse("asdflkjsdlkhjgkea  sdjlkfasjlkdhgkjhgjlk {{ a }} {% if a > 15 %} {% endif %}");
    string target = getParser().unparse(ast);
    //auto str = renderTemplate(ast, variable);
    ASSERT_EQ(target, "asdflkjsdlkhjgkea  sdjlkfasjlkdhgkjhgjlk {{ a }} {% if a > 15 %} {% endif %}");


    ast = getParser().parse("asdflkjsdlkhjgkea  sdjlkfasjlkdhgkjhgjlk {{ a[\"a\"] }} {% if a > 15 %} {% endif %}");
    target = getParser().unparse(ast);
    //auto str = renderTemplate(ast, variable);
    ASSERT_EQ(target, "asdflkjsdlkhjgkea  sdjlkfasjlkdhgkjhgjlk {{ a[\"a\"] }} {% if a > 15 %} {% endif %}");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
