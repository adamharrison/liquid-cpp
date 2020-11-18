
## Introduction

Is a soon-to-be fully featured C++17 renderer for Liquid; Shopify's templating language.

## Status

In development. Currently a bit all over the place.

## Features / Roadmap

This is what I'm aiming for at any rate.

### Done

* Includes a standard dialect that contains all array, string and math filters by default, as well as all normal operators, and all control flow, iteration and variable tags.
* Significantly less memory usage than ruby-based liquid.
* Contextualized set of filters, operators, and tags per liquid object instantaited; no global state as in the regular Shopify gem, easily allowing for many flavours of liquid in the same process.
* Ability to easily specify additions of filters, operators, and tags called `Dialects`, which can be mixed and matched.
* Small footprint. Aiming for under 5K SLOC, with full standard Liquid as part of the core library.

### Partial

* Togglable extra features, such as real operators, parentheses, etc..
* Line accurate, and helpful error messages. (Some are accurate and helpful, others are not).
* Significant speedup over ruby-based liquid. (Need to do benchmarks)
* Ability to step through and examine the liquid AST. (AST is generated, but no mechanisms to step through yet)
* Full test suite that runs all major examples from Shopify's doucmentation. (Test suite runs some examples, but not all).

### TODO

* Ability to partially render content, then spit back out the remaining liquid that genreated it.
* Fully compatible with both `Liquid`, Shopify's ruby gem, and `WWW::Shopify::Liquid`, the perl implementation.
* Optional compatibilty with rapidjson to allow for JSON reading.
* Ability to set limits on memory consumed, and time spent rendering.
* Built-in optimizer that will do things like loop unrolling, conditional elimiation, etc...
* Extremely easy to embed in other libraries, software, and langauges. Possiblilty of having it as a header-only library.
* Fully featured `extern "C"` interface for easy linking to most scripting languages. OOB bindings for both Ruby and Perl will be provided, that will act as drop-in replacements for `Liquid` and `WWW::Shopify::Liquid`.


## License

### MIT License

Copyright 2020 Adam Harrison

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

