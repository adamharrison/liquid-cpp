use strict;
use warnings;

package WWW::Shopify::Liquid::XS::Exception;
use overload
	fallback => 1,
	'""' => sub { return $_[0]->english . ($_[0]->line ? " on line " . $_[0]->line . ", character " . $_[0]->{line}->[1] .  ($_[0]->{line}->[3] ? ", file " . $_[0]->{line}->[3] : '') : ''); };
sub line { return $_[0]->{line} ? (ref($_[0]->{line}) && ref($_[0]->{line}) eq "ARRAY" ? $_[0]->{line}->[0] : $_[0]->{line}) : undef; }
sub column { return $_[0]->{line} && ref($_[0]->{line}) && ref($_[0]->{line}) eq "ARRAY" ? $_[0]->{line}->[1] : undef; }
sub stack { return $_[0]->{stack}; }
sub error { return $_[0]->{error}; }

sub new {
    my ($package, $error) = @_;
    return bless {
        error => $error->[0],
        data => $error->[3],
        line => [$error->[1], $error->[2]]
    }, $package;
}
sub english {
    my ($self) = @_;
    my $message;

    my @codes = (
        "No error",
        "Unexpected end to block",
        "Unknown tag",
        "Unknown operator",
        "Unknown operator or qualifier",
        "Unknown filter",
        "Invalid symbol",
        "Unbalanced end to group"
    );

    if (defined $self->{error} && $self->{error} < int(@codes)) {
        $message = $codes[$self->{error}];
        $message .= " '" . $self->{data} . "'" if defined $self->{data};
    }
    $message = "Unknown error" if !$message;

    return $message;
}

package WWW::Shopify::Liquid::XS::Renderer;

sub new {
    my ($package, $liquid) = @_;
    return bless { liquid => $liquid, renderer => WWW::Shopify::Liquid::XS::createRenderer($liquid->{context}) }, $package;
}

sub parent { return $_[0]->{liquid}; }

sub DESTROY {
    my ($self) = @_;
    WWW::Shopify::Liquid::XS::freeRenderer($self->{renderer});
}

sub render {
    my ($self, $hash, $template) = @_;
    my $error = [];
    my $result = WWW::Shopify::Liquid::XS::renderTemplate($self->{renderer}, $hash, $template->{template}, $error);
    die WWW::Shopify::Liquid::XS::Exception->new($error[0]) if !$self->{silence_exceptions} && int(@$error) > 0;
    return $result;
}

sub silence_exceptions { $_[0]->{silence_exceptions} = $_[1] if @_ > 1; return $_[0]->{silence_exceptions}; }
sub print_exceptions { $_[0]->{print_exceptions} = $_[1] if @_ > 1; return $_[0]->{print_exceptions}; }
sub inclusion_context { $_[0]->{inclusion_context} = $_[1] if @_ > 1; return $_[0]->{inclusion_context}; }
sub max_inclusion_depth { $_[0]->{max_inclusion_depth} = $_[1] if @_ > 1; return $_[0]->{max_inclusion_depth}; }

package WWW::Shopify::Liquid::XS::Optimizer;
use parent 'WWW::Shopify::Liquid::XS::Renderer';

sub optimize {
    my ($self, $hash, $template) = @_;
    return $template;
}

package WWW::Shopify::Liquid::XS::Template;

sub new {
    my ($package, $context, $text) = @_;
    my $error = [];
    my $template = WWW::Shopify::Liquid::XS::createTemplate($context, $text, $error);
    die WWW::Shopify::Liquid::XS::Exception->new($error) if !$template;
    return bless { template => $template }, $package;
}

sub DESTROY {
    my ($self) = @_;
    WWW::Shopify::Liquid::XS::freeTemplate($self->{template});
}

package WWW::Shopify::Liquid::XS::Tag::Include;

sub is_enclosing { 0; }
sub max_arguments { return 1; }
sub min_arguments { return 1; }

sub operate {
    my ($self, $hash, @arguments) = @_;
    my ($path, $template) = $self->{renderer}->retrieve_include($self, $hash, $arguments[0]);
    return $self->{renderer}->render($hash, $node);
}

sub retrieve_include {
	my ($self, $hash, $string) = @_;
	if ($self->{renderer}->inclusion_context) {
		# Inclusion contexts are evaluated from left to right, in order of priority.
		my @inclusion_contexts = $pipeline->inclusion_context && ref($pipeline->inclusion_context) && ref($pipeline->inclusion_context) eq "ARRAY" ? @{$pipeline->inclusion_context} : $pipeline->inclusion_context;
		die new WWW::Shopify::Liquid::Exception:XS([], "Backtracking not allowed in inclusions.") if $string =~ m/\.\./;
		for (@inclusion_contexts) {
			if (ref($_) eq "CODE") {
				return $_->($self, $hash, $action, $pipeline, $string);
			} else {
				my $path = $_ . "/" . $string . ".liquid";
				return ($path, scalar(read_file($path))) if -e $path;
			}
		}
    }
	return $self;
}

sub process_include {
	my ($self, $hash, $string, $path, $text, $argument) = @_;
	# If text is already an AST, then we do not parse the text.
	my $ast = ref($text) ? $text : $pipeline->parent->parse_text($text);
	$hash->{$string} = $argument if defined $argument;
    $self->{renderer}->render($hash, $ast);
    return $result;
}

sub operate {
	my ($self, $hash, @arguments) = @_;
	die new WWW::Shopify::Liquid::Exception([], "Recursive inclusion probable, greater than depth " . $pipeline->max_inclusion_depth . ". Aborting.")
		if $self->{renderer}->inclusion_depth > $self->{renderer}->max_inclusion_depth;
	return '' unless int(@arguments) > 0;
	my ($path, $text) = $self->retrieve_include($hash, $arguments[0]);
	my $include_depth = $self->{renderer}->inclusion_depth;
	$self->{renderer}->inclusion_depth($include_depth+1);
	$result = $self->process_include($hash, $arguments[0], $path, $text, $arguments[1]);
	$self->{renderer}->inclusion_depth($include_depth);
	return $result;

}



package WWW::Shopify::Liquid::XS;

use 5.024002;
use strict;
use warnings;

require Exporter;

our @ISA = qw(Exporter);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use WWW::Shopify::Liquid::XS ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(

) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
    liquid_render_file
    liquid_parse_file
);

our $VERSION = '0.01';

require XSLoader;
XSLoader::load('WWW::Shopify::Liquid::XS', $VERSION);


sub liquid_parse_file {
    my ($file) = @_;
    my $liquid = WWW::Shopify::Liquid::XS->new;
    return $liquid->parse_file($file);
}

sub liquid_render_file {
    my ($hash, $file) = @_;
    my $liquid = WWW::Shopify::Liquid::XS->new;
    return $liquid->render_file($hash, $file);
}

sub new {
    my ($package, %settings) = @_;
    my $self = bless {
        context => createContext(),
        dialects => [],
    %settings }, $package;
    WWW::Shopify::Liquid::XS::implementPermissiveStandardDialect($self->{context});
    $self->register_tag('WWW::Shopify::Liquid::XS::Tag::Include');
    $_->apply($self) for (@{$self->{dialects}});
    WWW::Shopify::Liquid::XS::implementWebDialect($self->{context}) if $settings{'implement_web_dialect'};
    $self->{renderer} = WWW::Shopify::Liquid::XS::Renderer->new($self->{context});
    $self->{optimizer} = WWW::Shopify::Liquid::XS::Optimizer->new($self->{context});
    return $self;
}

sub DESTROY {
    my ($self) = @_;
    freeContext($self->{context});
}

sub renderer { return $_[0]->{renderer}; }

sub parse_file {
    my ($self, $file) = @_;
    open(my $fh, "<", $file);
    my $text = do { local $/; <$fh> };
    return $self->parse_text($text);
}

sub parse_text {
    my ($self, $text) = @_;
    my $template = WWW::Shopify::Liquid::XS::Template->new($self->{context}, $text);
    return $template;
}

sub render_text {
    my ($self, $hash, $text) = @_;
    my $template = $self->parse_text($text);
    return $self->renderer->render($hash, $template);
}

sub render_file {
    my ($self, $hash, $text) = @_;
    my $template = $self->parse_file($text);
    return $self->renderer->render($hash, $template);
}

sub render_ast {
    my ($self, $hash, $template) = @_;
    return $self->renderer->render($hash, $template);
}

sub optimize_ast {
    my ($self, $hash, $template) = @_;
    return $self->optimizer->optimize($hash, $template);
}

sub register_tag {
    my ($self, $tag) = @_;
    die WWW::Shopify::Liquid::XS::Exception->new("Requies tag to have an operate sub; currently custom processing is not supported.") unless $tag->can('operate');
    die WWW::Shopify::Liquid::XS::Exception->new("Can't register tag '" . $tag->name . "'.") unless WWW::Shopify::Liquid::XS::registerTag($self->{context}, $tag->name, ($tag->is_enclosing ? "enclosing" : "free"), $tag->min_arguments, (defined $tag->max_arguments ? $tag->max_arguments : -1), $tag->is_enclosing ? sub {
        my ($renderer, $node, $hash, $contents, @arguments) = @_;
        return $tag::operate({
            node => $node,
            renderer => $renderer
        }, $hash, $contents, @arguments);
    } : sub {
        my ($renderer, $node, $hash, @arguments) = @_;
        return $tag::operate({
            node => $node,
            renderer => $renderer
        }, $hash, @arguments);
    }) == 0;
}

sub register_filter {
    my ($self, $filter) = @_;
    die WWW::Shopify::Liquid::XS::Exception->new("Requires filter to have an operate sub; currently custom processing is not supported.") unless $filter->can('operate');
    die WWW::Shopify::Liquid::XS::Exception->new("Can't register filter '" . $filter->name . "'.") unless WWW::Shopify::Liquid::XS::registerFilter($self->{context}, $filter->name, $filter->min_arguments, (defined $filter->max_arguments ? $filter->max_arguments : -1), sub {
        my ($renderer, $node, $hash, $operand, @arguments) = @_;
        my $result = $filter::operate({
            node => $node,
            renderer => $renderer
        }, $hash, $operand, @arguments);
        return $result;
    }) == 0;
}


sub register_operator {
    my ($self, $operator) = @_;
    die WWW::Shopify::Liquid::XS::Exception->new("Requires operator to have an operate sub; currently custom processing is not supported.") unless $operator->can('operate');
    die WWW::Shopify::Liquid::XS::Exception->new("Can't register operator '" . $operator->symbol. "'.") unless WWW::Shopify::Liquid::XS::registerOperator($self->{context}, $operator->symbol, $operator->arity, $operator->fixness, $operator->priority, sub {
        my ($renderer, $node, $hash, @operands) = @_;
        my $result = $operator::operate({
            node => $node,
            renderer => $renderer
        }, $hash, @operands);
        return $result;
    }) == 0;
}


# Preloaded methods go here.

1;
__END__
# Below is stub documentation for your module. You'd better edit it!

=head1 NAME

WWW::Shopify::Liquid::XS - Perl interface to the libliquid library.

=head1 SYNOPSIS

    use WWW::Shopify::Liquid::XS;
    my $liquid = WWW::Shopify::Liquid::XS->new;
    my $text = $liquid->render_text({ }, "{% if a %}35325324634ygfdfgd{% else %}sdfjkhafjksdhfjlkshdfjk{% endif %}");
    print $text;

=head1 DESCRIPTION

Presents an identical interface to WWW::Shopify::Liquid; but should be many times faster, and less memory-consuming.

=head1 SEE ALSO

L<WWW::Shopify::Liquid>

=head1 AUTHOR

Adam Harrison, E<lt>adamdharrison@gmail.comE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2020 by Adam Harrison

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.24.2 or,
at your option, any later version of Perl 5 you may have available.


=cut
