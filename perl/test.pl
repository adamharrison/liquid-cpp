
use WWW::Shopify::Liquid::XS;

my $liquid = WWW::Shopify::Liquid::XS->new;

my $text = $liquid->render_text({ }, "{% if a %}35325324634ygfdfgd{% else %}sdfjkhafjksdhfjlkshdfjk{% endif %}");

print $text . "\n";
