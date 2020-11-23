
## Introduction

Is a soon-to-be fully featured C++17 renderer for Liquid; Shopify's templating language.

## Status

In development. Currently a bit all over the place.

### Quick Start

#### C++

The C++ library, which is built with the normal Makefile can be linked in as a static library. Will eventually be available as a header-only library.

```c++
    #include <cstdio>
    #include "liquid.h"

    using namespace Liquid;

    int(int argc, char* argv[]) {
        Context context;
        Parser parser(context);
        StandardDialect::implement(context);
        context.registerType<CPPVariableNode>();

        const char exampleFile[] = "{% if a > 1 %}123423{% else %}sdfjkshdfjkhsdf{% endif %}";
        // Throws an exception if there's a parsing error.
        Node ast = parser.parseFile(exampleFile, sizeof(exampleFile)-1);
        Renderer renderer(context);

        CPPVariable store;
        store["a"] = 10;

        std::string result = result = renderer.render(ast, store);
        fprintf(stdout, "%s\n", result.data());
        return 0;
    }
```

#### C

The C library, which is built with the normal Makefile can be linked in as a static library.

```c
    #include <stdio.h>

    int(int argc, char* argv[]) {
        // The liquid context represents all registered tags, operators, filters, and a way to resolve variables.
        LiquidContext context = liquidCreateContext(LIQUID_SETTINGS_DEFAULT);

        // Very few of the bits of liquid are part of the 'core'; instead, they are implemented as dialects. In order to
        // stick in most of the default bits of Liquid, you can ask the context to implement the standard dialect, but this
        // is not necessary. The only default tag available is {% raw %}, as this is less a tag, and more a lexing hint. No
        // filters, or operators, other than the unary - operator, are available by default; they are are all part of the liquid standard dialect.
        liquidImplementStandardDialect(context);
        // In addition, dialects can be layered. Implementing one dialects does not forgo implementating another; and dialects
        // can override one another; whichever dialect was applied last will apply its proper tags, operators, and filters.
        // Currently, there is no way to deregsiter a tag, operator, or filter once registered.

        // If no LiquidVariableResolver is specified; an internal default is used that won't read anything you pass in, but will funciton for {% assign %}, {% capture %} and other tags.
        LiquidVariableResolver resolver = {
            ...
        };
        liquidRegisterVariableResolver(resolver);

        const char exampleFile[] = "{% if a > 1 %}123423{% else %}sdfjkshdfjkhsdf{% endif %}";
        LiquidTemplate tmpl = liquidCreateTemplate(context, exampleFile, sizeof(exampleFile)-1);
        if (liquidGetError()) {
            fprintf(stderr, "Error parsing template: %s", liquidGetError());
            exit(-1);
        }
        // This object should be thread-local.
        LiquidRenderer renderer = liquidCreateRenderer(context);
        // Use something that works with your language here; as resolved by the LiquidVariableResolver above.
        void* variableStore = NULL;
        LiquidTemplateRender result = liquidRenderTemplate(renderer, variableStore, tmpl);
        fprintf(stdout, "%s\n", liquidTemplateRenderGetBuffer(result));

        // All resources, unless otherwise specified, must be free explicitly.
        liquidFreeTemplateRender(result);
        liquidFreeRenderer(renderer);
        liquidFreeTemplate(tmpl);
        liquidFreeContext(context);

        return 0;
    }
```

#### Ruby

There're two ways to get the ruby library working;

```ruby
    require 'liquid-c'
    context = LiquidC.new()
    renderer = LiquidC::Renderer.new(context)
    template = LiquidC::Template.new(context, "{% if a %}asdfghj {{ a }}{% endif %}")
    puts renderer.render({ "a" => 1 }, template)
```

Or, alternatively, one can use the "drop in replacement" function, which will register the exact same constructs as the normal liquid library.

This is generally discouraged, as you lose some useful features of the library; like the ability to have independent liquid contexts with different
tags, filters, operators, and settings; and you have thread-safe rendering of shared templates, vs. just shoving everything into a global namespace.
But, it's of course, up to you.

```ruby
    require 'liquid-c'
    LiquidC::DIR()

    template = Liquid::Template.parse("{% if a %}asdfghj {{ a }}{% endif %}")
    puts template.render({ "a" => 1 }, template)
```

May eventually stick the "drop in replacement" into a separate module, that just calls it a in single include.

#### Perl

Coming Soon! Will likely be called `WWW::Shopify::Liquid::XS`.

## Features / Roadmap

This is what I'm aiming for at any rate.

### Done

* Includes a standard dialect that contains all array, string and math filters by default, as well as all normal operators, and all control flow, iteration and variable tags.
* Significantly less memory usage than ruby-based liquid.
* Contextualized set of filters, operators, and tags per liquid object instantaited; no global state as in the regular Shopify gem, easily allowing for many flavours of liquid in the same process.
* Ability to easily specify additions of filters, operators, and tags called `Dialects`, which can be mixed and matched.
* Small footprint. Aiming for under 5K SLOC, with full standard Liquid as part of the core library.

### Partial

* Togglable extra features, such as real operators, parentheses, etc.. (This is in, but not togglable).
* Line accurate, and helpful error messages. (Some are accurate and helpful, others are not).
* Significant speedup over ruby-based liquid. (Need to do benchmarks; seems like a minimum of a 6-fold speedup over regular Liquid)
* Ability to step through and examine the liquid AST. (AST is generated, but no mechanisms to step through yet)
* Full test suite that runs all major examples from Shopify's doucmentation. (Test suite runs some examples, but not all).
* Extremely easy to embed in other libraries, software, and langauges. Possiblilty of having it as a header-only library. (Emebedding easy; header-only, not yet).
* Fully featured `extern "C"` interface for easy linking to most scripting languages. OOB bindings for both Ruby and Perl will be provided, that will act as drop-in replacements for `Liquid` and `WWW::Shopify::Liquid`. (Ruby, but not Perl yet).

### TODO

* Ability to partially render content, then spit back out the remaining liquid that genreated it.
* Fully compatible with both `Liquid`, Shopify's ruby gem, and `WWW::Shopify::Liquid`, the perl implementation.
* Optional compatibilty with rapidjson to allow for JSON reading.
* Ability to set limits on memory consumed, and time spent rendering.
* Built-in optimizer that will do things like loop unrolling, conditional elimiation, etc...


## License

### MIT License

Copyright 2020 Adam Harrison

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

