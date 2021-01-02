#!/usr/bin/env perl

use strict;
use warnings;

package Test;

my @a = (1,2,3,5,6,7);

sub a { return "B"; }

package main;

use WWW::Shopify::Liquid::Dialect::Web;
use WWW::Shopify::Liquid::XS;

my $liquid = WWW::Shopify::Liquid::XS->new;
WWW::Shopify::Liquid::Dialect::Web->apply($liquid);

my $block = '{{ a.a }}';
my $ast = $liquid->parse_text($block);
$liquid->renderer->make_method_calls(1);

my $text = $liquid->render_ast({ a => (bless { }, "Test") }, $ast);

print STDERR "TEXT: $text END\n";

$liquid = undef;

print STDERR "DONE\n";
