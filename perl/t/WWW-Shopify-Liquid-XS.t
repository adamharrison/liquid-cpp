use strict;
use warnings;

package WWW::Shopify::Liquid::XS::Filter::Test;
use parent 'WWW::Shopify::Liquid::Filter';

sub operate {
	my ($self, $hash, $operand) = @_;
	return reverse($operand);
}


package WWW::Shopify::Liquid::XS::Operator::Regex;
use parent 'WWW::Shopify::Liquid::Filter';

sub symbol { '=~' }
sub arity { "binary" }
sub fixness { "infix" }
sub priority { 10; }

sub operate {
	my ($self, $hash, $operand, $regex) = @_;
    my @groups = $operand =~ m/$regex/;
    return undef if int(@groups) == 0;
    return \@groups;
}

package main;

# Specific edge-cases which have come up through use in the various apps.
my $string = '<img src=http://cdn.shopify.com/s/files/1/0291/4345/t/2/assets/logo.png?171> <alt=UCC Resources><br><br>
Thanks for purchasing Faith Practices, all the resources can be found here: <a href="http://www.ucc.org/faith-practices/fp-secure/">http://www.ucc.org/faith-practices/fp-secure/</a>.<br><br>
Date {{ date | date: "%m/%d/%Y" }}{% if requires_shipping and shipping_address %}<br><br><b>Shipping address</b><br>
   {{ shipping_address.name }}<br>
   {{ shipping_address.street }}<br>
   {{ shipping_address.city }}, {{ shipping_address.province }}  {{ shipping_address.zip }}<br><br>
  {% endif %}{% if billing_address %}

 <b>Billing address</b><br>
   {{ billing_address.name }}<br>
   {{ billing_address.street }}<br>
   {{ billing_address.city }}, {{ billing_address.province }}  {{ billing_address.zip }}v
{% endif %}<br><br>

<b><ul>{% for line in line_items %} <li> <img src="{{ line.product.featured_image  | replace: ".jpg", "_large.jpg" }}" /> {{ line.quantity }}x {{ line.title }} for {{ "%0.2f" | sprintf: line.price }} each {% for note in line.properties %} {{note.first}} : {{note.last}} {% endfor %} </li> {% endfor %}</ul></b><br><br>

{% if discounts %}Discount (code: {{ discounts.first.code }}): {{ "%0.2f" | sprintf: discounts_savings }}{% endif %}<br>
Subtotal  : {{ "%0.2f" | sprintf: subtotal_price  }}{% for tax_line in tax_lines %}
{{ tax_line.title }}       : {{ "%0.2f" | sprintf: tax_line.price  }}{% endfor %}{% if requires_shipping %}<br>
Shipping  : {{ "%0.2f" | sprintf: shipping_price }}{% endif %}<br>
<p>Total : {{ "%0.2f" | sprintf: total_price }}</p><ul style="list-style-type:none"><br>{% assign gift_card_applied = false %}
{% assign gift_card_amount = 0 %}
{% for transaction in transactions %}
 {% if transaction.gateway  == "gift_card" %}
   {% assign gift_card_applied = true %}
   {% assign gift_card_amount = gift_card_amount | plus: transaction.amount %}
 {% endif %}
{% endfor %}
{% if gift_card_amount > 0 %}
<p>Gift cards: {{ "%0.2f" | sprintf: (gift_card_amount | times: -1) }}</p>
{% endif %}
<p><b>Total: {{ "%0.2f" | sprintf: (total_price | minus: gift_card_amount) }}</p></b>

<br><br>Thank you for shopping with UCC Resources!<br>
{{ shop.url }}';

use strict;
use warnings;
use Test::More;

use_ok('WWW::Shopify::Liquid::XS');
use_ok('DateTime');

my $liquid = WWW::Shopify::Liquid::XS->new;
my $result;

my $ast = $liquid->parse_text("{% for i in (0..2) %}{{ i }}{% endfor %}");
ok($ast);
$result = $liquid->render_ast({ }, $ast);
is($result, "012");


$ast = $liquid->parse_text("{% for trans in order.transactions %}{% if trans.status == 'success' and trans.kind == 'refund' %}{% unless forloop.first %}, {% endunless %}{{ trans.id }}{% endif %}{% endfor %}");
ok($ast);
$result = $liquid->render_ast({ order => { transactions => [{ status => 'success', kind => 'refund', id => 12445324 }] } }, $ast);
is($result, 12445324);

$ast = $liquid->parse_text("{% for trans in order.transactions %}{% if trans.status == 'success' and trans.kind == 'refund' %}{% unless forloop.first %}, {% endunless %}{{ trans.created_at }}{% endif %}{% endfor %}");
my $now = DateTime->now;
$result = $liquid->render_ast({ order => { transactions => [{ status => 'success', kind => 'refund', created_at => DateTime->now }] } }, $ast);
is($result, $now->iso8601);
ok($ast);

$ast = $liquid->parse_text("{% for i in (0..2) %}{% if line_item.product.options[i].name == \"Color\" %}{% assign groups = line_item.variant_title | split: \" / \" %}{{ groups[i] }}{% endif %}{% endfor %}");
$result = $liquid->render_ast({ line_item => { variant_title => "Red / adsfds", product => { options => [{ name => "Color" }, { name => "Size" }] } } }, $ast);
is($result, "Red");

my $proverbs = '{% assign a = 0 %}{% for i in order.shipping_lines %}{% assign a = a + i.price %}{% endfor %}{% if line_item.loop_index == 0 %}$ {{ a + (line_item.price * line_item.quantity) + order.total_tax - order.total_discounts }}{% else %}$ {{ line_item.quantity * line_item.price }}{% endif %}';
$ast = $liquid->parse_text($proverbs);
$result = $liquid->render_ast({ order => {
	shipping_lines => [{
        price => 10
	}],
	total_tax => 20,
	total_discounts => 1
}, line_item => {
	loop_index => 0,
	price => 10,
	quantity => 2
} }, $ast);
is($result, ('$ ' . '' . (10+(10*2)+20-1)));


my $mountain = "{% unless product.tags contains 'pre-sale' %}
	{% if product.published_at %}
		{% for variant in product.variants %}{% assign a = 1 %}{% if variant.inventory_quantity == null or variant.inventory_quantity > 0 %}{% assign a = 0 %}{% endif %}{% endfor %}{{ a }}
	{% else %}
		{% for variant in product.variants %}{% if variant.inventory_quantity and variant.inventory_quantity > 0 %}1{% endif %}{% endfor %}
	{% endif %}
{% endunless %}";
$ast = $liquid->parse_text($mountain);
ok($ast);


$liquid->register_filter('WWW::Shopify::Liquid::XS::Filter::Test');

my $filter = $liquid->render_text({ }, '{{ "123456" | test }}');
is($filter, "654321");


$liquid->register_operator('WWW::Shopify::Liquid::XS::Operator::Regex');

my $text = $liquid->render_text({ product => {
	tags => "test, asd3, asiojdofs, 43266356, adfssdf, 139847, tags, tag2, 2teasf, thirdtag, 100"
} }, "{% assign tags = product.tags | split: ', ' %}{% assign is_first = 1 %}{% for tag in tags %}{% if tag =~ '^\\d+\$' %}{% unless is_first %}, {% endunless %}{% assign is_first = 0 %}{{ tag }}{% endif %}{% endfor %}");
is($text, "43266356, 139847, 100");

$text = $liquid->render_text({
	variant => { id => 1, option1 => "Red", option2 => "Large", option3 => "Silk"},
	product => {
		variants => [{ id => 1, option1 => "Red", option2 => "Large", option3 => "Silk"}],
		options => [{ name => "Material", position => 3 }, { name => "Color", position => 1 }, { name => "Size", position => 2 }]
	}
}, "{% assign color = 1 %}{% if color %}{{ variant['option' + color] }}{% else %}B{% endif %}");
is($text, "Red");

$text = $liquid->render_text({
	line_item => {
		sku => 'ASDTOPS',
		loop_index => 0
	},
	order => {
		line_items => [{
			sku => 'ASDTOPS',
			price => 10
		}, {
			sku => 'ASDBOTS',
			price => 0
		}]
	}
}, "{{ order.line_items[line_item.loop_index].sku }}");
is($text, "ASDTOPS");

$text = $liquid->render_text({
	line_item => {
		sku => 'ASDTOPS'
	},
	order => {
		line_items => [{
			sku => 'ASDTOPS'
		}, {
			sku => 'ASDBOTS'
		}]
	}
}, "{% if line_item.sku contains ' --- ' %}{{ line_item.sku | split: ' --- ' | last }}{% else %}{% assign base = (line_item.sku =~ '^(\\w+)TOP') %}BASE{{ base[0] }}|{% for line in order.line_items %}{% if line.sku != line_item.sku and line.sku contains base[0] %}{{ line.sku }}{% endif %}{% endfor %}{% endif %}");
is($text, "BASEASD|ASDBOTS");


done_testing();

# $text = $liquid->render_text({
	# line_item => {
		# sku => 'ASDTOPS',
		# loop_index => 0
	# },
	# order => {
		# line_items => [{
			# sku => 'ASDTOPS',
			# price => 10
		# }, {
			# sku => 'ASDBOTS',
			# price => 0
		# }]
	# }
# }, "{% if line_item.sku contains ' --- ' %}{{ line_item.sku | split: ' --- ' | last }}{% elsif order.line_items[line_item.loop_index+1] and order.line_items[line_item.loop_index+1].price == 0 %}{{ order.line_items[line_item.loop_index+1].sku }}{% endif %}");
# is($text, "ASDBOTS");

# $text = $liquid->render_text({
	# a => 1,
	# l => [1,2,3]
# }, "{{ l[a+1] }}{{ l[a+1] }}");
# is($text, "33");

# $text = $liquid->render_text({
	# order => {
		# gateway => "moneris",
		# payment_details => {
			# credit_card_company => "MasterCard"
		# }
	# }
# }, "{% if order.gateway == 'paypal' %}PayPal{% elsif order.payment_details.credit_card_company == 'Visa' %}Visa{% elsif order.payment_details.credit_card_company == 'MasterCard' %}MC{% elsif order.payment_details.credit_card_company == 'American Express' %}American Express{% else %}Void{% endif %}");
# is($text, "MC");

# use DateTime;
# $now = DateTime->now;
# $text = $liquid->render_text({
	# order => { fulfillments => [{ created_at => $now }] }
# }, "{{ order.fulfillments[0].created_at | date: '%m.%d.%y' }}");
# is($text, $now->strftime('%m.%d.%y'));

# # Big block.

# my $block = '{% assign isa = orders.first.id %}
# {{ now | date: "%y%m%d" }}*{{ now | date: "%h%m" }}*U*00400*{{ isa | sprintf: "%09d" }}*0*P*>~GS*PO*024662722*079254738*{{ now | date: "%y%m%d" }}*{{ now | date: "%h%m" }}*{{ isa }}*X*004030~
# {% for order in orders %}
# ST*850*{{ order.id }}~BEG*00*SA*{{ order.name }}~{% if order.payment_details %}REF*PQ*
# {{ order.payment_details.credit_card_number | replace: " ", "" }}*{{ order.authorization }}~{% endif %}
# {% for line_item in order.line_items %}PO1**{{line_item.quantity}}*EA*{{ line_item.price }}*EA*VN*{{ line_item.sku }}*UI*{{ line_item.barcode }}~{% endfor %}{% endfor %}SE*{{ total_segments }}~GE*1*{{ isa }}~IEA*1*{{ isa }}~';
# @tokens = $liquid->lexer->parse_text($block);
# $ast = $liquid->parser->parse_tokens(@tokens);

# $block = "{% for note in order.note_attributes %}{% if note.name == 'Edition' %}{% assign notes = note.value | split: '\n' %}{% for line in notes %}{% if line contains line_item.title %}{% assign parts = line | split: 'edition: ' %}{{ parts | last }}{% endif %}{% endfor %}{% endif %}{% endfor %}";
# @tokens = $liquid->lexer->parse_text($block);
# $ast = $liquid->parser->parse_tokens(@tokens);
# isa_ok($tokens[2], 'WWW::Shopify::Liquid::Token::Tag');
# is($tokens[2]->tag, 'assign');

# $block = '{% for line_item in order.line_items %}{% for property in line_item.properties %}{% if property.name == "Gift Email" and property.value == email %}{% assign li = line_item %}{% endif %}{% endfor %}{% endfor %}{{ li.id }}';
# @tokens = $liquid->lexer->parse_text($block);
# $ast = $liquid->parser->parse_tokens(@tokens);
# my $order = {
	# line_items => [{
		# id => 1,
		# properties => [{
			# name => "Gift Email",
			# value => 'test1@gmail.com'
		# }, {
			# name => "Gift Image",
			# value => 1
		# }]
	# }, {
		# id => 2,
		# properties => [{
			# name => "Gift Email",
			# value => 'test2@gmail.com'
		# }, {
			# name => "Gift Image",
			# value => 2
		# }]
	# }]
# };


# $text = $liquid->renderer->render({ order => $order, email => 'test1@gmail.com' }, $ast);
# is($text, 1);

# $text = $liquid->renderer->render({ order => $order, email => 'test2@gmail.com' }, $ast);
# is($text, 2);

# $text = $liquid->render_text({ settings => { productspg_featured_limit => 3 } }, '{% for product in (1..10) limit: settings.productspg_featured_limit offset: 5 %}{{ forloop.index }}{% endfor %}');
# is($text, '678');

# my $pattern = "{{ a | replace: \"\r\n\", \"\" }}";
# @tokens = $liquid->parse_text($pattern);
# isa_ok($tokens[0], 'WWW::Shopify::Liquid::Tag::Output');
# isa_ok($tokens[0]->{arguments}->[0], 'WWW::Shopify::Liquid::Filter::Replace');
# is(int(@{$tokens[0]->{arguments}->[0]->{arguments}}), 2);
# is($tokens[0]->{arguments}->[0]->{arguments}->[0]->{core}, "\r\n");
# is($tokens[0]->{arguments}->[0]->{arguments}->[1]->{core}, "");

# $text = $liquid->render_text({ a => "test1\r\ntest2" }, "{{ a | replace: \"\r\n\", \"\" }}");
# is($text, "test1test2");

# my $hash;
# ($text, $hash) = $liquid->render_text({ a => [] }, "{% assign a[0] = {} %}");
# ok($hash->{a});
# isa_ok($hash->{a}, 'ARRAY');
# is(int(@{$hash->{a}}), 1);
# isa_ok($hash->{a}->[0], 'HASH');
# is(int(keys(%{$hash->{a}->[0]})), 0);

# @tokens = $liquid->parser->parse_tokens($liquid->lexer->parse_text("{% assign a[0] = {} %}{% assign a[0].test = 1 %}"));
# is(int(@tokens), 1);
# ok($tokens[0]->{operands});
# ok($tokens[0]->{operands}->[1]);
# ok($tokens[0]->{operands}->[1]->{arguments}->[0]);
# ok($tokens[0]->{operands}->[1]->{arguments}->[0]->{operands});
# ok($tokens[0]->{operands}->[1]->{arguments}->[0]->{operands}->[0]->{core});
# is(int(@{$tokens[0]->{operands}->[1]->{arguments}->[0]->{operands}->[0]->{core}}), 3);

# $ast = $liquid->parser->parse_tokens(@tokens);
# ok($ast);

# ($text, $hash) = $liquid->render_text({ a => [] }, "{% assign a[0] = {} %}{% assign a[0].test = 1 %}");
# ok($hash->{a});
# isa_ok($hash->{a}, 'ARRAY');
# is(int(@{$hash->{a}}), 1);
# is(int(keys(%{$hash->{a}->[0]})), 1);

# ($text, $hash) = $liquid->render_text({ a => [] }, "{{ 1 * 2 | ceil }}");
# is($text, '2');


# ($text, $hash) = $liquid->render_text({ a => [] }, "{{ (1 * 2) }}");
# is($text, '2');

# $ast = $liquid->parser->parse_tokens($liquid->lexer->parse_text("{{ (1 * 2) | ceil }}"));
# isa_ok($ast, 'WWW::Shopify::Liquid::Tag::Output');
# isa_ok($ast->{arguments}->[0], 'WWW::Shopify::Liquid::Filter::Ceil');
# isa_ok($ast->{arguments}->[0]->{operand}, 'WWW::Shopify::Liquid::Operator::Multiply');


# ($text, $hash) = $liquid->render_text({ a => [] }, "{{ (1 * 2) | ceil }}");
# is($text, '2');

# ($text, $hash) = $liquid->render_text({ customer => { metafields => { a => { 'shared-secret' => 1 } } }, shop_namespace => 'a' }, "{{ customer.metafields[shop_namespace]['shared-secret'] }}");
# is($text, '1');


# ($text, $hash) = $liquid->render_text({ a => [] }, "{% if 1 < 2 %}1{% else %}0{% endif %}");
# is($text, '1');

# ($text, $hash) = $liquid->render_text({ a => [] }, "{% if 2 > 1 %}1{% else %}0{% endif %}");
# is($text, '1');

# ($text, $hash) = $liquid->render_text({ a => [] }, "{% if 2 >= 1 %}1{% else %}0{% endif %}");
# is($text, '1');

# ($text, $hash) = $liquid->render_text({ a => [] }, "{% if 1 <= 2 %}1{% else %}0{% endif %}");
# is($text, '1');

# ($text, $hash) = $liquid->render_text({ a => DateTime->now, start => DateTime->today, end => DateTime->today->add(days => 1) }, "{% if a > start and a < end %}1{% else %}0{% endif %}");
# is($text, '1');
# ($text, $hash) = $liquid->render_text({ a => DateTime->now, start => DateTime->today, end => DateTime->today->add(days => 1) }, "{% if a < start and a > end %}1{% else %}0{% endif %}");
# is($text, '0');
# ($text, $hash) = $liquid->render_text({ a => undef, start => DateTime->today, end => DateTime->today->add(days => 1) }, "{% if a > start and a < end %}1{% else %}0{% endif %}");
# is($text, '0');

# ($text, $hash) = $liquid->render_text({ address => { address1 => "44", address2 => "Dickinson Court" } }, "{% assign total = 0 %}{% assign line = address.address1 + \" \" + address.address2 %}{{ line }}");
# is($text, "44 Dickinson Court");

# ($text, $hash) = $liquid->render_text({ }, "{% assign a = 44 ~ 45 %}{{ a }}");
# is($text, '4445');

# ($text, $hash) = $liquid->render_text({ a => [] }, "{% assign a[2] = 'asdfsfdf' %}");
# is($hash->{a}->[2], 'asdfsfdf');

# ($text, $hash) = $liquid->render_text({ a => [] }, "{% capture a[2] %}asdfsfdf{% endcapture %}");
# is($hash->{a}->[2], 'asdfsfdf');

# ($text, $hash) = $liquid->render_text({ a => { asd => 1, bde => 2 } }, "{% for l in a %}{{ l }}: {{ a[l] }}{% endfor %}");
# like($text, qr/(asd: 1bde: 2|bde: 2asd: 1)/);

# ($text, $hash) = $liquid->render_text({ a => { b => 1, c => undef } }, "{% if a.b and a.c and a.d %}1{% endif %}");
# is($text, '');

# ($text, $hash) = $liquid->render_text({ a => { b => 1, c => undef } }, "{% if a.b and a.d %}1{% endif %}");
# is($text, '');

# ($text, $hash) = $liquid->render_text({ a => { b => 1, c => undef } }, "{% if a.b and a.c %}{% endif %}");
# is($text, '');

# ($text, $hash) = $liquid->render_text({ a => [1,2,3] }, "{% if a[5] !~ 'ASD' %}1{% endif %}");
# is($text, '1');

# ($ast) = $liquid->parser->parse_argument_tokens($liquid->lexer->parse_expression([1,0,0,undef], "order.created_at > (now | date_math: -2, 'weeks')"));
# ok($ast);
# $ast = $liquid->optimizer->optimize({ now => DateTime->now }, $ast);
# is(int(@{$ast->{operands}}), 2);
# is(int(grep { defined $_ } @{$ast->{operands}}), 2);

# ($text, $hash) = $liquid->render_text({ order => { } }, "{{ order.metafields.size }}");
# is($text, '');

# @tokens = $liquid->lexer->parse_text("{{ 'a{{ item }}b{{ item }}c' | split: '\{\{ item }}' | last }}");
# is(int(@tokens), 1);
# isa_ok($tokens[0], 'WWW::Shopify::Liquid::Token::Output');
# @tokens = $liquid->lexer->parse_text("{{ 'a{% item %}b{% item %}c' | split: '{% item %}' | last }}");
# is(int(@tokens), 1);
# isa_ok($tokens[0], 'WWW::Shopify::Liquid::Token::Output');

# $ast = $liquid->parse_text("{{ 'a{{ item }}b{{ item }}c' | split: '\{\{ item }}' | last }}");
# isa_ok($ast, 'WWW::Shopify::Liquid::Tag::Output');

# ($text, $hash) = $liquid->render_text({ order => { } }, "{{ 'a{{ item }}b{{ item }}c' | split: '\\{\\{ item }}' | last }}");
# is($text, 'c');
# ($text, $hash) = $liquid->render_text({ order => { } }, "{{ 6 % 4 }}");
# is($text, '2');

# ($text, $hash) = $liquid->render_text({ order => { } }, q({{ "New Mom's Bundle of Joy" }}));
# is($text, q(New Mom's Bundle of Joy));

# ($text, $hash) = $liquid->render_text({ order => { } }, q({{ (0..1) | json }}));
# is($text, q(["0","1"]));

# ($text, $hash) = $liquid->render_text({ order => { } }, q({{ (0..(1-1)) | json }}));
# like($text, qr/\["?0"?\]/);

# $text = $liquid->render_text({ line_item => { sku => "B", quantity => 2 } }, "{{ line_item.sku }}{% for i in (1..line_item.quantity) %}A{% endfor %}");
# is($text, 'BAA');

# $text = $liquid->render_text({ discount_code => { code => "ABCDE" } }, "{% if (discount_code.code | downcase) =~ 'bcd' %}1{% endif %}");


# @tokens = $liquid->lexer->parse_text("{{ order.line_items[(repeat+1) * 5 + 1].sku }}");
# ok(@tokens);
# is(int(@tokens), 1);
# is(int(@{$tokens[0]->{core}}), 1);
# isa_ok($tokens[0]->{core}->[0], 'WWW::Shopify::Liquid::Token::Variable');

# $ast = $liquid->parser->parse_tokens(@tokens);
# ok($ast);
# $text = $liquid->render_ast({ repeat => 3, order => { line_items => [map { { sku => $_ } } 0..100] } }, $ast);
# is($text, 21);


# $text = $liquid->render_text({ a => 1 }, "{% increment a %}{{ a }}");
# is($text, 2);
# $text = $liquid->render_text({ a => 1 }, "{% decrement a %}{{ a }}");
# is($text, 0);

# ($text, $hash) = $liquid->render_text({ a => [1,5,7] }, "{{ a | reverse | join: ';' }}");
# is($text, '7;5;1');

# ($text, $hash) = $liquid->render_text({ a => [1,5,7] }, "{% for b in a %}{{ b }}{% endfor %}{% for b in a %}{{ b }}{% endfor %}");
# is($text, '157157');

# $ast = $liquid->parse_text("{% for b in a %}{{ b }}{% endfor %}{% for b in a %}{{ b }}{% endfor %}");
# ok($ast);

# $ast = $liquid->optimize_ast({ a => [1,5,7] }, $ast);

# $text = $liquid->render_ast($hash, $ast);
# is($text, '157157');


# $liquid->renderer->silence_exceptions(0);
# ($text, $hash) = $liquid->render_text({ a => { b => "c", d => "e" } }, "{% remove_key a.b %}");
# ok(exists $hash->{a});
# ok(!exists $hash->{a}->{b});

# $text = $liquid->render_text({ a => [{ test => 2 }, { test => 3 }] }, "{{ a | map: op.test | sum }}");
# is($text, 5);
# $text = $liquid->render_text({ a => [{ test => 2 }, { test => 3 }] }, "{{ a | map: op.test | min }}");
# is($text, 2);
# $text = $liquid->render_text({ a => [{ test => 2 }, { test => 3 }] }, "{{ a | map: op.test | max }}");
# is($text, 3);

# $text = $liquid->render_text({ a => [{ test => 2 }, { test => 3 }] }, "{{ a | grep: (op.test == 2) | map: op.test | max }}");

# is($text, 2);

# # $text = $liquid->render_text({ }, "{% assign true = 0 %}{{ true }}");
# # is($text, 1);

# $text = $liquid->render_text({ a => { b => 2 } }, "{% remove_key a['b'] %}{{ a.size }}");
# is($text, 0);
# $text = $liquid->render_text({ a => { b => 2 } }, "{% assign c = 'b' %}{% remove_key a[c] %}{{ a.size }}");
# is($text, 0);


# done_testing();
