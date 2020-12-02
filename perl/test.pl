#!/usr/bin/env perl

use strict;
use warnings;

package main;

use WWW::Shopify::Liquid::XS;

my $liquid = WWW::Shopify::Liquid::XS->new;

my $block = '{% for line_item in order.line_items %}{% for property in line_item.properties %}{% if property.name == "Gift Email" and property.value == email %}{% assign li = line_item %}{% endif %}{% endfor %}{% endfor %}{{ li.id }}';
my $ast = $liquid->parse_text($block);
my $order = {
	line_items => [{
		id => 1,
		properties => [{
			name => "Gift Email",
			value => 'test1@gmail.com'
		}, {
			name => "Gift Image",
			value => 1
		}]
	}, {
		id => 2,
		properties => [{
			name => "Gift Email",
			value => 'test2@gmail.com'
		}, {
			name => "Gift Image",
			value => 2
		}]
	}]
};


my $text = $liquid->render_ast({ order => $order, email => 'test1@gmail.com' }, $ast);
print STDERR "TEXT: $text\n";