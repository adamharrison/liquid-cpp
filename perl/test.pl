#!/usr/bin/env perl

use strict;
use warnings;

package WWW::Shopify::Liquid::XS::Filter::Test;
use parent 'WWW::Shopify::Liquid::Filter';

sub operate {
	my ($self, $hash, $operand) = @_;
	return reverse($operand);
}

package main;

use WWW::Shopify::Liquid::XS;

my $liquid = WWW::Shopify::Liquid::XS->new;
$liquid->register_filter('WWW::Shopify::Liquid::XS::Filter::Test');
my $text;

# $text = $liquid->render_text({ }, "{% if a %}35325324634ygfdfgd{% else %}sdfjkhafjksdhfjlkshdfjk{% endif %}");
# print $text . "\n";

$text = $liquid->render_text({ }, "{{ \"abcdef\" | test }}");
print $text . "\n";

# $text = $liquid->render_text({ }, "{% if a 35325324634ygfdfgd{% else %}sdfjkhafjksdhfjlkshdfjk{% endif %}");
