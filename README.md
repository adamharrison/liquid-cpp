
## Introduction

Is a soon-to-be fully featured C++17 renderer for Liquid; Shopify's templating language.

## Status

In development. Currently a bit all over the place.

### Quick Start

### Building

Building the library is easy; so long as you have the appropriate build-system; currently the library uses cmake to build across platforms.

It can be built and installed like so, from the main directory:

```
mkdir -p build && cd build && cmake .. && make && sudo make install
```

Eventually, I'll have a .deb that can be downloaded from somewhere for Ubuntu distros, but that's not quite up yet.

#### C++

The C++ library, which is built with the normal Makefile can be linked in as a static library. Will eventually be available as a header-only library.

```c++
#include <cstdio>
#include <liquid/liquid.h>

int(int argc, char* argv[]) {
    // The liquid context represents all registered tags, operators, filters, and a way to resolve variables.
    Liquid::Context context;
    // Very few of the bits of liquid are part of the 'core'; instead, they are implemented as dialects. In order to
    // stick in most of the default bits of Liquid, you can ask the context to implement the standard dialect, but this
    // is not necessary. The only default tag available is {% raw %}, as this is less a tag, and more a lexing hint. No
    // filters, or operators, other than the unary - operator, are available by default; they are are all part of the liquid standard dialect.
    // In addition to setting all these nodes up, this also sets the 'truthiness' of variables to evaluted in a loose manner; meaning
    // that in addition to false and nil, being not true; 0, and empty string are also considered not true.
    // In order to implement the stricter Shopify ruby version of things, use `implementStrict` instead.
    Liquid::StandardDialect::implementPermissive(context);
    // In addition, dialects can be layered. Implementing one dialect does not forgo implementating another; and dialects
    // can override one another; whichever dialect was applied last will apply its proper tags, operators, and filters.
    // Currently, there is no way to deregsiter a tag, operator, or filter once registered.

    // Register the standard, out of the box variable implementation that lets us pass a type union'd variant that can hold either a long long, double, pointer, string, vector, or unordered_map<string, ...> .
    context.registerType<Liquid::CPPVariableNode>();

    // Initialize a parser. These should be thread-local. One parser can parse many files.
    Liquid::Parser parser(context);

    const char exampleFile[] = "{% if a > 1 %}123423{% else %}sdfjkshdfjkhsdf{% endif %}";
    // Throws an exception if there's a parsing error.
    Liquid::Node ast = parser.parseFile(exampleFile, sizeof(exampleFile)-1);
    // Initialize a renderer. These should be thread-local. One renderer can render many templates.
    Liquid::Renderer renderer(context);

    Liquid::CPPVariable store;
    store["a"] = 10;

    std::string result = renderer.render(ast, store);
    // Should output "123423"
    fprintf(stdout, "%s\n", result.data());
    return 0;
}
```

This program should be linked with `-lliquid`.

#### C

The following program below is analogous to the one above, only using the external C interface.

```c
#include <stdio.h>
#include <liquid/liquid.h>

int(int argc, char* argv[]) {
    LiquidContext context = liquidCreateContext();
    liquidImplementPermissiveStandardDialect(context);

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

This program should be linked with `-lliquid -lstdc++`.

#### Ruby

##### Install

Currently the package isn't uploaded on rubygems, so it has to be build manually. Luckily; this is easy:

```
cd ruby && rake compile && rake gem && gem install pkg/*.gem && cd -
```

##### Usage

There're two ways to get the ruby library working;

```ruby
require 'liquidc'
context = LiquidC.new()
renderer = LiquidC::Renderer.new(context)
template = LiquidC::Template.new(context, "{% if a %}asdfghj {{ a }}{% endif %}")
puts renderer.render({ "a" => 1 }, template)
```

Or, alternatively, one can use the "drop in replacement" module, which wraps all this, which will register the exact same constructs as the normal liquid library. (Coming Soon!)

```ruby
require 'liquid-creplacement'

template = Liquid::Template.parse("{% if a %}asdfghj {{ a }}{% endif %}")
puts template.render({ "a" => 1 }, template)
```

This is generally discouraged, as you lose some useful features of the library; like the ability to have independent liquid contexts with different
tags, filters, operators, and settings; and you have explicit thread-safe rendering of shared templates, vs. just shoving everything into a global namespace.
But, it's of course, up to you.

#### Perl

##### Install

Currently the module hasn't been uploaded to CPAN, but can be built and installed like so;

```
cd perl && perl Makefile.PL && make && sudo make install && cd -
```

Will eventually upload this.

##### Usage

Uses the exact same interface as `WWW::Shopify::Liquid`, and is basically almost fully compatible with it as `WWW::Shopify::Liquid::XS`.

All core constrcuts are overriden, and there is no optimizer at present, but all top-level functions should work correctly; and most dialects that have tags, filters and operators,
that use `operate` instead of `process` or `render` should function without changes.

```perl
    use WWW::Shopify::Liquid::XS;

    my $liquid = WWW::Shopify::Liquid::XS->new;
    my $text = $liquid->render_file({ }, "myfile.liquid");
    print "$text\n";
```

### Python

This will probably come around at some point; but currently, there are no bindings for Python.

## Features / Roadmap

This is what I'm aiming for at any rate.

### Done

* Includes a standard dialect that contains all array, string and math filters by default, as well as all normal operators, and all control flow, iteration and variable tags.
* Significantly less memory usage than ruby-based liquid.
* Contextualized set of filters, operators, and tags per liquid object instantaited; no global state as in the regular Shopify gem, easily allowing for many flavours of liquid in the same process.
* Ability to easily specify additions of filters, operators, and tags called `Dialects`, which can be mixed and matched.
* Small footprint. Aiming for under 5K SLOC, with full standard Liquid as part of the core library.
* Fully featured `extern "C"` interface for easy linking to most scripting languages. OOB bindings for both Ruby and Perl will be provided, that will act as drop-in replacements for `Liquid` and `WWW::Shopify::Liquid`.
* Significant speedup over ruby-based liquid. (Need to do benchmarks; but at first glance seems like a minimum of a 6-fold speedup over regular Liquid)
* Fully compatible with both `Liquid`, Shopify's ruby gem, and `WWW::Shopify::Liquid`, the perl implementation. The only thing we're missing is optimizer, or AST walk capabilities.
* Use a standard build system; like cmake, instead of a manual make.

### Partial

* Togglable extra features, such as real operators, parentheses, etc.. (This is in, but not togglable).
* Line accurate, and helpful error messages. (Some are accurate and helpful, others are not).
* Ability to step through and examine the liquid AST. (AST is generated, but no mechanisms to step through yet)
* Full test suite that runs all major examples from Shopify's doucmentation. (Test suite runs some examples, but not all).
* Ability to set limits on memory consumed, and time spent rendering. (Partially implemented).

### TODO

* Ability to partially render content, then spit back out the remaining liquid that genreated it.
* Optional compatibilty with rapidjson to allow for JSON reading.
* Built-in optimizer that will do things like loop unrolling, conditional elimiation, etc...


## License

### MIT License

Copyright 2020 Adam Harrison

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

