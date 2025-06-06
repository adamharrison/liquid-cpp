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

string getTestFile(const std::string& path) {
    string str;
    FILE* file = fopen(path.data(), "rb");
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    str.resize(length);
    fread(str.data(), sizeof(char), length, file);
    fclose(file);
    return str;
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

void ASSERT_NO_PARSER_ERRORS() {
    for (size_t i = 0; i < getParser().errors.size(); ++i) {
        fprintf(stderr, "Error parsing: %s\n", Liquid::Parser::Error::english(getParser().errors[i]).c_str());
    }
    ASSERT_EQ(getParser().errors.size(), 0);
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
    ast = getParser().parse("asdbfsdf {{ a / 0 }} b");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 0 b");
    ast = getParser().parse("asdbfsdf {{ a / 0.0 }} b");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "asdbfsdf 0 b");
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
    ast = getParser().parse(u8"asdbfsdf             {{- 1 -}}b");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "asdbfsdf1b");

    ast = getParser().parse(u8"asdbfsdf        {{ 1 }} b");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "asdbfsdf        1 b");

    ast = getParser().parse(u8"asdbfsdf        {{- 1 }} b");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "asdbfsdf1 b");

    ast = getParser().parse(u8"asdbfsdf        {{- 1 -}} b");
    str = renderTemplate(ast, variable);
    ASSERT_EQ(str, "asdbfsdf1b");
    ast = getParser().parse(u8"asdbfsdf        {{- 1 -}}b");
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
    hash["a"] = "1,5,6";
    Node ast;
    std::string str;


    ast = getParser().parse("{% for i in a | split: \",\" %}{{ i }}{% endfor %}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "156");


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

    ast = getParser().parse("{% capture email_body %}All you have to do is activate it and choose a password.{% endcapture %}{% if custom_message != blank %} <p>{{ custom_message }}</p> {% else %} <p>{{ email_body }}</p> {% endif %}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, " <p>All you have to do is activate it and choose a password.</p> ");

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
    long long now = (long long)time(NULL);

    char buffer[1024];

    ast = getParser().parse("{{ (1 + 2) | times: 2}}");
    ASSERT_NO_PARSER_ERRORS();
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "6");

    ast = getParser().parse("{% assign a = 2 | plus: 2 %}{{ a }}");
    ASSERT_NO_PARSER_ERRORS();
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "4");

    ast = getParser().parse("{{ 1 + 2 | times: 2}}");
    ASSERT_NO_PARSER_ERRORS();
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "6");




    ast = getParser().parse("{{ 1 | plus: (1 | plus: 2) }}");
    ASSERT_NO_PARSER_ERRORS();

    ast = getParser().parse("{{ \"helloworld \" | append: (\"now\" | date: \"%s\") }}");
    ASSERT_NO_PARSER_ERRORS();
    str = renderTemplate(ast, hash);
    sprintf(buffer, "helloworld %lld", now);
    ASSERT_STREQ(str.data(), buffer);

    ast = getParser().parse("{{ (\"now\" | date: \"%s\") }}");
    ASSERT_NO_PARSER_ERRORS();

    ast = getParser().parse("{{ \"helloworld \" | append: 1709827882 }}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "helloworld 1709827882");



    /*ast = getParser().parse("Test {{ product.id | times: }}");
    ASSERT_EQ(getParser().errors.size(), 0);
    str = renderTemplate(ast, hash);
    ASSERT_STREQ(str.data(), "Test ");*/


    // This should probably be implciit.
    hash["now"] = now;
    CPPVariable product = { };
    CPPVariable option = { };
    CPPVariable variant = { };
    CPPVariable metafield = { };
    CPPVariable metafields;
    option["name"] = "Color";
    CPPVariable options;
    options[0] = std::move(option);
    product["title"] = "My Very First Product";
    product["options"] = std::move(options);
    product["created_at"] = "2021-09-01T00:00:00-00:00";
    product["tags"] = "sort_01234567, hide-red";
    product["product_type"] = "bodysuits";
    metafield["key"] = "color_map";
    metafield["namespace"] = "details";
    metafield["value"] = "multi pass";
    metafields[0] = std::move(metafield);
    product["metafields"] = std::move(metafields);
    variant["option1"] = "red";
    product["body_html"] = "This is a test description.\
    <p>Here's more text.</p>\
    Here's a list:\
    <li>\
        Bullet point 1.\
    \
    </li>\
    <li>\
        Bullet point 2.\
    </li>\
    <li>\
        Bullet point 3.\
    </li>";
    hash["variant"] = std::move(variant);
    hash["product"] = std::move(product);


    ast = getParser().parse("{% assign normalized = \"black blue brown green grey multi nude orange pink purple red white yellow\" | split: \" \" %}{% for p in product.metafields %}{% if p.namespace == \"details\" and p.key == \"color_map\" %}{% assign desc = p.value %}{% endif %}{% endfor %}{% if desc %}{% for c in normalized %}{% if desc contains c %}{{ c }}{% break %}{% endif %}{% endfor %}{% endif %}");
    ASSERT_EQ(getParser().errors.size(), 0);
    str = renderTemplate(ast, hash);
    ASSERT_STREQ(str.data(), "multi");

    ast = getParser().parse("{% assign desc = \"test bodysuits\" %}{% if desc -%}\n{%- if product.product_type == \"bodysuits\" %}{% if desc contains \"bodysuit\" %}bodysuit, romper, jumpsuit, catsuit{% endif -%}\n{%- elsif product.product_type == \"bralettes\" %}{% if desc contains \"bralette\" %}bralette{% endif -%}\n{%- elsif product.product_type == \"bras\" %}{% if desc contains \"unlined underwire\" or desc contains \"unlined balconette\" %}underwire, unlined underwire, unlined balconette{% endif -%}\n{%- elsif product.product_type == \"camis\" %}{% if desc contains \"crop cami\" %}crop cami, longline bralette{% elsif desc contains \"cami\" %}cami, crop cami{% endif -%}\n{%- elsif product.product_type == \"corsets\" %}{% if desc contains \"corset\" %}corset, crop corset, longline underwire{% endif -%}\n{%- elsif product.product_type == \"cropped corsets\" %}{% if desc contains \"crop corset\" or desc contains \"longling underwire\" %}crop corset, longline underwire, corset{% endif -%}\n{%- elsif product.product_type == \"garter belts\" %}{% if desc contains \"garter belt\" %}garter belt{% endif -%}\n{%- elsif product.product_type == \"jumpsuits\" %}{% if desc contains \"jumpsuit\" %}jumpsuit, catsuit, bodysuit, romper{% elsif desc contains \"romper\" %}romper, bodysuit, jumpsuit{% elsif desc contains \"catsuit\" %}catsuit, jumpsuit, bodysuit, romper{% endif -%}\n{%- elsif product.product_type == \"longline braletes\" %}{% if desc contains \"longline bralette\" %}longline bralette, crop cami{% endif -%}\n{%- elsif product.product_type == \"lounge bottoms\" %}{% if desc contains \"short\" %}short, romper{% elsif desc contains \"pant\" %}pant, jumpsuit{% endif -%}\n{%- elsif product.product_type == \"lounge tops\" %}{% if desc contains \"top\" %}top{% endif -%}\n{%- elsif product.product_type == \"panties\" %}{% if desc contains \"string\" %}string, thong{% elsif desc contains \"thong\" %}thong, low rise thong, high leg thong, string{% elsif desc contains \"bikini\" %}bikini, low rise bikini, high leg bikini{% elsif desc contains \"hipster\" %}hipster, bikini, high waist brief{% elsif desc contains \"short\" %}short, high waist brief{% elsif desc contains \"high waist brief\" %}high waist brief, short{% endif -%}\n{%- elsif product.product_type == \"slips\" %}{% if desc contains \"slip\" or desc contains \"babydoll\" or desc contains \"dress\" %}dress, slip, babydoll{% endif -%}\n{%- elsif product.product_type == \"swim bottoms\" %}{% if desc contains \"bikini bottom\" %}bikini bottom, cover-up top, cover-up bottom, cover-up dress{% endif -%}\n{%- elsif product.product_type == \"swim one-piece\" %}{% if desc contains \"one-piece swimsuit\" %}one-piece swimsuit, cover-up bottom, cover-up top, cover-up dress{% endif -%}\n{%- elsif product.product_type == \"swim tops\" %}{% if desc contains \"bikini top\" %}bikini top, cover-up bottom, cover-up top, cover-up dress{% endif -%}\n{%- elsif product.product_type == \"cover-ups tops\" %}{% if desc contains \"top\" %}bikini bottom, bikini top, bikini one-piece{% endif -%}\n{%- elsif product.product_type == \"cover-ups bottoms\" %}{% if desc contains \"pant\" %}bikini top, bikini one-pipece, bikini bottom{% endif -%}\n{%- elsif product.product_type == \"cover-ups one-piece\" %}{% if desc contains \"dress\" %}bikini top, bikini bottom, bikini one-piece{% endif %}{% endif %}{% endif -%}");
    ASSERT_EQ(getParser().errors.size(), 0);
    str = renderTemplate(ast, hash);
    ASSERT_STREQ(str.data(), "bodysuit, romper, jumpsuit, catsuit");


    ast = getParser().parse("{{ '34' | prepend: '000000' | slice: -6, 6 }}");
    ASSERT_EQ(getParser().errors.size(), 0);
    str = renderTemplate(ast, hash);
    ASSERT_STREQ(str.data(), "000034");


    ast = getParser().parse("{{ product.body_html | split: '<li>' | slice: 1 | join: '<li>' | prepend: '<li>' }}");
    ASSERT_EQ(getParser().errors.size(), 0);
    str = renderTemplate(ast, hash);
    ASSERT_STREQ(str.data(), "<li>\
        Bullet point 1.\
    \
    </li>\
    <li>\
        Bullet point 2.\
    </li>\
    <li>\
        Bullet point 3.\
    </li>");

    ast = getParser().parse("{%- assign product_split = product.body_html | split: '<li>' -%}{{ product_split | slice: 1, product_split.size | join: '<li>' | prepend: '<li>' }}");
    ASSERT_EQ(getParser().errors.size(), 0);
    str = renderTemplate(ast, hash);
    ASSERT_STREQ(str.data(), "<li>\
        Bullet point 1.\
    \
    </li>\
    <li>\
        Bullet point 2.\
    </li>\
    <li>\
        Bullet point 3.\
    </li>");

    hash["product"]["body_html"] = "<p>It may be chilly out, but this dress is packing a lot of HEAT! Featuring a double peek-a-boo neckline and cheeky side slit, this dress is an excellent choice for the gals who want a dress with a little edge.</p>\n<p><strong><span style=\"text-decoration: underline;\">Note:</span></strong> Dresses at Le Chateau may run smaller than Suzy Shier dresses. Please consult our <strong><a href=\"https://suzyshier.com/pages/le-chateau-size-chart/\" title=\"Size chart\"><span style=\"text-decoration: underline;\">size chart</span></a></strong> for more accurate measurements.</p>\n<li>Knit fabric</li>\n<li>Halter neckline</li>\n<li>Double peekaboo front keyhole</li>\n<li>Spaghetti straps</li>\n<li>Open back</li>\n<li>Bodycon fit</li>\n<li>Mini length</li>\n<li>Front side slit</li>\n<li>100% polyester lining</li>\n<li>96% polyester, 4% elastane</li>\n<li>Machine wash in cold water</li>";
    ast = getParser().parse("{{ product.body_html | split: '<li>' | slice: 1 | join: '' | remove: '</li>' }}");
    ASSERT_EQ(getParser().errors.size(), 0);
    str = renderTemplate(ast, hash);
    ASSERT_STREQ(str.data(), "Knit fabric\nHalter neckline\nDouble peekaboo front keyhole\nSpaghetti straps\nOpen back\nBodycon fit\nMini length\nFront side slit\n100% polyester lining\n96% polyester, 4% elastane\nMachine wash in cold water");

    ast = getParser().parse("Test {{ product.title | split: \" \" | slice: 1, 2 | join: \" \" }}");
    ASSERT_EQ(getParser().errors.size(), 0);
    str = renderTemplate(ast, hash);
    ASSERT_STREQ(str.data(), "Test Very First");

    ast = getParser().parse("Test {{ product.title | split: \" \" | first }}");
    ASSERT_EQ(getParser().errors.size(), 0);
    str = renderTemplate(ast, hash);
    ASSERT_STREQ(str.data(), "Test My");



    ast = getParser().parse("{% assign idx = nil %}{% for o in product.options %}{% if o.name == 'Color' %}{% assign idx = 'option' | append: forloop.index %}{% endif %}{% endfor %}{% if idx %}{% assign tag = 'hide-' | append: variant[idx] %}{% if product.tags contains tag %}0{% else %}1{% endif %}{% else %}1{% endif %}");
    ASSERT_EQ(getParser().errors.size(), 0);
    str = renderTemplate(ast, hash);
    ASSERT_STREQ(str.data(), "0");



    ast = getParser().parse("{{ year_bit | append: day_bit} }}");
    ASSERT_EQ(getParser().errors.size(), 1);

    ast = getParser().parse("{{ \"abc\" | slice: 0,2 }}");
    ASSERT_EQ(getParser().errors.size(), 0);
    str = renderTemplate(ast, hash);
    ASSERT_STREQ(str.data(), "ab");




    /*ast = getParser().parse("{% assign published_timestamp = product.created_at | date: \"%s\" %}{% assign days_since_published = now | date: \"%s\" | minus: published_timestamp | divided_by: 86400 | floor %}\
{% for tag in (product.tags | split: \", \") %}\
    {% if tag contains 'sort_' %}\
        {% assign sort_tag = tag | remove: 'sort_' %}\
    {% endif %}\
    {% if tag contains 'sort_' %}\
        {% assign override_tag = '01234567' %}\
    {% endif %}\
{% endfor %}\
{% if override_tag %}\
    tag: {{ override_tag }}\
    {% assign year_bit = override_tag | slice: 0,4 %}\
    {% assign month_bit = override_tag | slice: 4,2 %}\
    {% assign day_bit = override_tag | slice: 6,2 %}\
\
    year: {{ year_bit }} \
    month: {{ month_bit }} \
    day: {{ day_bit }}\
\
    {% assign override_string = year_bit | append: \"-\" | append: month_bit | append: \"-\" | append: day_bit} | append: 'T00:00:00-08:00' %}\
\
{{ override_string }}\
\
    {% assign override_timestamp = override_string | date: \"%s\" %}\
    {% assign days_since_published = now | date: \"%s\" | minus: override_timestamp | divided_by: 86400 | floor %}\
{% endif %}\
{{ days_since_published }}{{ sort_tag | default: \"000000\" }}");
    ASSERT_EQ(getParser().errors.size(), 0);
    str = renderTemplate(ast, hash);
    ASSERT_STREQ(str.data(), "");*/


    ast = getParser().parse("{% assign a = now | date: '%s' %}{% assign b = product.created_at | date: '%s' %}{% assign c = a | minus: b | divided_by: 86400 | at_least: 0 %}{{ a }} {{ b }} {{ c | floor }}");
    string str1 = renderTemplate(ast, hash);
    ASSERT_EQ(getParser().errors.size(), 0);

    ast = getParser().parse("{% assign a = 'now' | date: '%s' %}{% assign b = product.created_at | date: '%s' %}{% assign c = a | minus: b | divided_by: 86400 | at_least: 0 %}{{ a }} {{ b }} {{ c | floor }}");
    string str2 = renderTemplate(ast, hash);
    ASSERT_EQ(getParser().errors.size(), 0);
    ASSERT_EQ(str1, str2);

    sprintf(buffer, "%lld 1630472400 %lld", now, (now - 1630472400)/86400);

    ASSERT_STREQ(str1.data(), buffer);

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

TEST(sanity, sequence) {
    CPPVariable hash;
    Node ast;
    std::string str;
    
    ast = getParser().parse("{% assign start = 17 %}{% for i in (start..19) %}{{ i }}{% endfor %}");
    str = renderTemplate(ast, hash);
    ASSERT_STREQ(str.c_str(), "171819");
}

TEST(sanity, composite) {
    CPPVariable hash, order, transaction, event, variant, product, registry, shop;
    Node ast;
    std::string str;

    registry["url"] = std::string("https://TEST.myshopify.com/apps/giftregistry/registry/1");
    hash["registry"] = std::move(registry);
    ast = getParser().parse("{{ registry.url | remove: \"https://\" | split: \"/\" | first | prepend: \"https://\" }}/pages/comfort-guarantee");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "https://TEST.myshopify.com/pages/comfort-guarantee");




    ast = getParser().parse("{% if a > 2 -%}1{% else-%}45{% endif %}");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "45");


    CPPVariable variantList = CPPVariable({ });
    variant["sku"] = "0024545-ASD-2134";
    variantList.a.push_back(move(make_unique<CPPVariable>(variant)));

    product["variants"] = move(variantList);
    hash["product"] = std::move(product);


    ast = getParser().parse("{% assign digits = product.variants.first.sku | slice: 5, 2 | plus: 0 %}{{ digits }}{% if digits >= 63 %}63{% elsif digits >= 61 %}61{% elsif digits >= 40 %}40{% elsif digits >= 21 %}21{% elsif digits >= 1 %}01{% endif %}");
    str = renderTemplate(ast, hash);
    ASSERT_STREQ(str.c_str(), "4540");

    ast = getParser().parse("{% assign digits = product.variants[0].sku | slice: 5, 2 | plus: 0 %}{{ digits }}{% if digits >= 63 %}63{% elsif digits >= 61 %}61{% elsif digits >= 40 %}40{% elsif digits >= 21 %}21{% elsif digits >= 1 %}01{% endif %}");
    str = renderTemplate(ast, hash);
    ASSERT_STREQ(str.c_str(), "4540");


    variant["price"] = 10;
    product["variants"] = CPPVariable({ variant });
    product["tags"] = "sort_012345";
    hash["product"] = std::move(product);

    ast = getParser().parse("{% assign min_price = 999999999 %}{% for v in product.variants %}{% if v.price < min_price %}{% assign min_price = v.price %}{% endif %}{% endfor %}{% for tag in (product.tags | split: \", \") %}{% if tag contains 'sort_' %}{% assign sort_tag = tag | remove: 'sort_' %}{% endif %}{% endfor %}{{ min_price | times: 100 }}{{ sort_tag | default: \"000000\" }}");
    str = renderTemplate(ast, hash);

    ASSERT_STREQ(str.c_str(), "1000012345");

    hash["B"] = "D";
    ast = getParser().parse("A {{ B }} C", sizeof("A {{ B }} C")-1, "testfile");
    str = renderTemplate(ast, hash);
    ASSERT_EQ(str, "A D C");

    CPPVariable lip1, lip2, lip3, lip4;
    CPPVariable line_item, line_item1, line_item2, product_option1, product_option2, product_option3;

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


    ast = parser.parse("{{ \"test\" | prepend: \"test2\" }}");
    result = renderer.render(ast, hash);
    ASSERT_EQ(result, "test2test");
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
        ast = getParser().parse("{{ product.title | split: }}");
    });

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
    rapidjson::Document d;
    renderer.resolverCustomData = &d;

    string str;
    d.SetObject();
    d.Parse("{\"products\":[{\"inventory_quantity\":1},{\"inventory_quantity\":3}]}");

    ast = getParser().parse("{% assign total = 0 %}{% for p in products %}{% assign total = total + p.inventory_quantity %}{% endfor %}{{ total }}");
    str = renderer.render(ast, &d);
    ASSERT_EQ(str, "4");

    d.Parse("{\"products\":[{\"variants\":[{\"inventory_quantity\":1},{\"inventory_quantity\":3}]},{\"variants\":[{\"inventory_quantity\":9},{\"inventory_quantity\":6}]}]}");

    ast = getParser().parse("{% assign total = 0 %}{% for p in products %}{% for v in p.variants %}{% assign total = total + v.inventory_quantity %}{% endfor %}{% endfor %}{{ total }}");
    str = renderer.render(ast, &d);
    ASSERT_EQ(str, "19");

    d.SetObject();
    ast = getParser().parse("{% assign total = 0 %}{{ total }}");
    str = renderer.render(ast, &d);
    ASSERT_EQ(str, "0");



    rapidjson::Document f(rapidjson::kObjectType);
    rapidjson::Value v(rapidjson::kArrayType);
    v.PushBack(rapidjson::Value(10), f.GetAllocator());
    v.PushBack(rapidjson::Value(20), f.GetAllocator());
    v.PushBack(rapidjson::Value(30), f.GetAllocator());
    v.PushBack(rapidjson::Value(40), f.GetAllocator());
    f.AddMember(rapidjson::Value("a", f.GetAllocator()), v, f.GetAllocator());
    ast = getParser().parse("{{ a.size }}");
    str = renderer.render(ast, &f);
    ASSERT_EQ(str, "4");

    ast = getParser().parse("{{ a.first }}");
    str = renderer.render(ast, &f);
    ASSERT_EQ(str, "10");

    ast = getParser().parse("{% for b in a %}{{ b }}{% endfor %}");
    str = renderer.render(ast, &f);
    ASSERT_EQ(str, "10203040");



    ast = getParser().parse("{{ a }}");

    d.Parse("{\"a\":\"b\"}");

    str = renderer.render(ast, &d);

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
