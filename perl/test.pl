#!/usr/bin/env perl

use strict;
use warnings;

use WWW::Shopify::Liquid::XS;

my $liquid = WWW::Shopify::Liquid::XS->new;
my $text;

# $text = $liquid->render_text({ }, "{% if a %}35325324634ygfdfgd{% else %}sdfjkhafjksdhfjlkshdfjk{% endif %}");
# print $text . "\n";

$text = $liquid->render_text({ }, "{% for i in (0..2) %}{{ i }}{% endfor %}");

# $text = $liquid->render_text({ }, "{% if a 35325324634ygfdfgd{% else %}sdfjkhafjksdhfjlkshdfjk{% endif %}");
