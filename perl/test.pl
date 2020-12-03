#!/usr/bin/env perl

use strict;
use warnings;

package main;

use WWW::Shopify::Liquid::Dialect::Web;
use WWW::Shopify::Liquid::XS;

my $liquid = WWW::Shopify::Liquid::XS->new;
WWW::Shopify::Liquid::Dialect::Web->apply($liquid);

my $block = '{% escape_js %}var a = "sdkfakjdsfkjdsalf
sdlfhjskfhsalkfh

sahfkjsad{% endescape_js %}";';
my $ast = $liquid->parse_text($block);

my $text = $liquid->render_ast({ }, $ast);
print STDERR "TEXT: $text\n";
