use strict;
use warnings;

package WWW::Shopify::Liquid::Filter::XS::Date;
sub name { "date" }
sub min_arguments { return 1; }
sub max_arguments { return 1; }
sub operate {
	my ($self, $hash, $operand, @arguments) = @_;
	return '' unless $operand;
	$operand = DateTime->now if !ref($operand) && $operand eq "now";
	return DateTime->from_epoch( epoch => $operand )->strftime(@arguments) if $operand && !ref($operand);
	return $operand->strftime(@arguments);
}


package WWW::Shopify::Liquid::Filter::XS::Sprintf;
sub name { "sprintf" }
sub min_arguments { return 1; }
sub max_arguments { return 1; }
sub operate {
	my ($self, $hash, $operand, @arguments) = @_;
	return '' unless $operand;
	return sprintf($operand, @arguments);
}


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
        "Unexpected operand",
        "Invalid arguments",
        "Invalid symbol",
        "Unbalanced end to group",
        "Parse depth exceeded"
    );

    if (defined $self->{error} && $self->{error} < int(@codes)) {
        $message = $codes[$self->{error}];
        $message .= " '" . $self->{data} . "'" if defined $self->{data};
    }
    $message = "Unknown error" if !$message;

    return $message;
}

package WWW::Shopify::Liquid::XS::Renderer;

use Scalar::Util qw(weaken);

sub new {
    my ($package, $liquid) = @_;
    my $self = bless { liquid => $liquid, max_inclusion_depth => 10, inclusion_depth => 0 }, $package;
    weaken($self->{liquid});
    $self->{renderer} = WWW::Shopify::Liquid::XS::createRenderer($liquid->{context}, $self);
    return $self;
}

sub parent { return $_[0]->{liquid}; }


sub DESTROY {
    my ($self) = @_;
    WWW::Shopify::Liquid::XS::freeRenderer($self->{renderer});
}

use Encode;

sub render {
    my ($self, $hash, $template) = @_;
    my $error = [];
    $self->inclusion_depth(0);
    my $result = WWW::Shopify::Liquid::XS::renderTemplate($self->{renderer}, $hash, $template->{template}, $error);
    die WWW::Shopify::Liquid::XS::Exception->new($error->[0]) if !$self->{silence_exceptions} && int(@$error) > 0;
    return decode("UTF-8", $result);
}

sub silence_exceptions { $_[0]->{silence_exceptions} = $_[1] if @_ > 1; return $_[0]->{silence_exceptions}; }
sub print_exceptions { $_[0]->{print_exceptions} = $_[1] if @_ > 1; return $_[0]->{print_exceptions}; }
sub inclusion_context { $_[0]->{inclusion_context} = $_[1] if @_ > 1; return $_[0]->{inclusion_context}; }
sub inclusion_depth { $_[0]->{inclusion_depth} = $_[1] if @_ > 1; return $_[0]->{inclusion_depth}; }
sub max_inclusion_depth { $_[0]->{max_inclusion_depth} = $_[1] if @_ > 1; return $_[0]->{max_inclusion_depth}; }
sub make_method_calls {
    if (@_ > 1) {
        $_[0]->{make_method_calls} = $_[1];
        WWW::Shopify::Liquid::XS::rendererSetMakeMethodCalls($_[0]->{renderer}, $_[1]);
    }
    return $_[0]->{make_method_calls};
}
sub clone_hash { $_[0]->{clone_hash} = $_[1] if @_ > 1; return $_[0]->{clone_hash}; }

package WWW::Shopify::Liquid::XS::Parser;
use Encode;

sub new {
    my ($package, $context) = @_;
    my $self = bless {
        parser => WWW::Shopify::Liquid::XS::createParser($context->{context})
    }, $package;
    return $self;
}

sub parse {
    my ($self, $text, $file) = @_;
    my $error = [];
    my $template = WWW::Shopify::Liquid::XS::Template->new(WWW::Shopify::Liquid::XS::parseTemplate($self->{parser}, encode("UTF-8", $text), $error));
     if (!$template) {
        $error = WWW::Shopify::Liquid::XS::Exception->new($error);
        $error->{line}->[3] = $file;
        die $error;
    }
    return $template;
}

package WWW::Shopify::Liquid::XS::Optimizer;

sub new {
    my ($package, $renderer) = @_;
    my $self = bless {
        optimizer => WWW::Shopify::Liquid::XS::createOptimizer($renderer->{renderer})
    }, $package;
    return $self;
}

sub optimize {
    my ($self, $hash, $template) = @_;
    WWW::Shopify::Liquid::XS::optimizeTemplate($self->{optimizer}, $template->{template}, $hash);
    return $template;
}

package WWW::Shopify::Liquid::XS::Template;

sub new {
    my ($package, $template) = @_;
    return bless { template => $template }, $package;
}

# For the walk retrieve all tokens in an array,
sub tokens {
    my ($self) = @_;
    my @nodes;
    WWW::Shopify::Liquid::XS::walkTemplate($self->{template}, sub {
        my ($node) = @_;
        push(@nodes, $node);
    });
}

sub DESTROY {
    my ($self) = @_;
    WWW::Shopify::Liquid::XS::freeTemplate($self->{template});
}

package WWW::Shopify::Liquid::XS::Tag::Include;

sub is_enclosing { 0; }
sub max_arguments { return 1; }
sub min_arguments { return 1; }
sub name { "include"; }
sub optimizes { 0; }

sub retrieve_include {
	my ($self, $hash, $string) = @_;
	if ($self->{renderer}->inclusion_context) {
		# Inclusion contexts are evaluated from left to right, in order of priority.
		my @inclusion_contexts = $self->{renderer}->inclusion_context && ref($self->{renderer}->inclusion_context) && ref($self->{renderer}->inclusion_context) eq "ARRAY" ? @{$self->{renderer}->inclusion_context} : $self->{renderer}->inclusion_context;
		die new WWW::Shopify::Liquid::Exception::XS("Backtracking not allowed in inclusions.") if $string =~ m/\.\./;
		for (@inclusion_contexts) {
			if (ref($_) eq "CODE") {
				return $_->($self, $hash, "render", $self->{renderer}, $string);
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
	my $ast = ref($text) ? $text : $self->{renderer}->parent->parse_text($text);
	$hash->{$string} = $argument if defined $argument;
    return $self->{renderer}->render($hash, $ast);
}

sub operate {
	my ($self, $hash, @arguments) = @_;
	die new WWW::Shopify::Liquid::Exception::XS("Recursive inclusion probable, greater than depth " . $self->{renderer}->max_inclusion_depth . ". Aborting.")
		if $self->{renderer}->inclusion_depth > $self->{renderer}->max_inclusion_depth;
	return '' unless int(@arguments) > 0;
	my ($path, $text) = $self->retrieve_include($hash, $arguments[0]);
	my $include_depth = $self->{renderer}->inclusion_depth;
	$self->{renderer}->inclusion_depth($include_depth+1);
	my $result = $self->process_include($hash, $arguments[0], $path, $text, $arguments[1]);
	$self->{renderer}->inclusion_depth($include_depth);
	return $result;
}


# Analyzes liquid in a non-trivial manner to perform useful calculations.
# Generaly for dependncy analysis.
package WWW::Shopify::Liquid::XS::Analyzer::Entity;
sub new {
	my ($package, $hash) = @_;
	$hash = {} unless $hash;
	return bless {
		id => undef,
		# Things we include.
		dependencies => [],
		# Things that include us.
		references => [],
		# Flattened lists of dependencies and references.
		full_dependencies => [],
		full_references => [],
		file => undef,
		ast => undef,
		%$hash
	}, $package;
}
sub ast { return $_[0]->{ast}; }
sub id { return $_[0]->{id}; }
sub file { return $_[0]->{file}; }

package WWW::Shopify::Liquid::XS::Analyzer;
sub new {
	my ($package) = shift;
	my $self = bless {
		@_
	}, $package;
	$self->{liquid} = WWW::Shopify::Liquid::XS->new unless $self->liquid;
	return $self;
}
sub liquid { return $_[0]->{liquid}; }

sub retrieve_includes_ast {
	my ($self, $ast) = @_;
	return () unless $ast;
	return grep { defined $_ && $_->isa('WWW::Shopify::Liquid::XS::Tag::Include') } $ast->tokens;
}

sub expand_entity {
	my ($self, $entity, $type, $used_list) = @_;
	$used_list = { $entity->id => 1 } unless $used_list;
	return map { $used_list->{$_->id} ? () : ($_, $self->expand_entity($_, $type, $used_list)) } @{$entity->{$type}};
}

sub generate_include_entity {
	my ($self, $include) = @_;
	my ($path) = $include->retrieve_include({}, "render", $self->liquid->renderer, $include->include_literal);
	my $entity = $self->generate_entity_file($path);
	return $entity;

}

use List::MoreUtils qw(uniq);
sub populate_dependencies {
	my ($self, @entities) = @_;
	my %entity_list = map { $_->id => $_ } @entities;
	# Go through, and flatten out those lists.
	for my $entity (grep { $_->ast } @entities) {
		my @included_literals = $self->retrieve_includes_ast($entity->ast);
		$entity_list{$_->include_literal} = $self->generate_include_entity($_) for (grep { !$entity_list{$_->include_literal} } @included_literals);
		my @included_entities = map { $entity_list{$_->include_literal} } @included_literals;
		push(@{$entity->{dependencies}}, @included_entities);
		push(@{$_->{references}}, $entity) for (@included_entities);
	}
	for (@entities) {
		$_->{full_dependencies} = [$self->expand_entity($_, 'dependencies')];
		$_->{full_references} = [$self->expand_entity($_, 'references')];
	}

	return @entities;
}

# If an entity is changed/added, then this should be called on the entity.
sub add_refresh_entity {
	my ($self, $entity, @entities) = @_;
	my %entity_list = map { $_->id => $_ } @entities;
	if (!$entity_list{$entity->id}) {
		my @included_literals = $self->retrieve_includes_ast($entity->ast);
		$entity_list{$_->include_literal} = $self->generate_include_entity($_) for (grep { !$entity_list{$_->include_literal} } @included_literals);
		my @included_entities = map { $entity_list{$_->include_literal} } @included_literals;
		push(@{$entity->{dependencies}}, @included_entities);
		push(@{$_->{references}}, $entity) for (@included_entities);
		$entity->{full_dependencies} = [$self->expand_entity($entity, 'dependencies')];
		$entity->{full_references} = [$self->expand_entity($entity, 'references')];
	}
	return $entity;
}

sub add_refresh_path {
	my ($self, $path, @entities) = @_;
	return $self->add_refresh_entity($self->generate_entity_file($path), @entities);
}

sub generate_entity_file {
	my ($self, $path) = @_;
	return WWW::Shopify::Liquid::XS::Analyzer::Entity->new({
		id => do { my $handle = $path; $handle =~ s/^.*\/([^\/]+?)(\.liquid)?$/$1/; $handle },
		ast => $self->liquid->parse_file($path),
		file => $path
	})
}

sub generate_entities_files {
	my ($self, @paths) = @_;
	return map {
		$self->generate_entity_file($_);
	} @paths;
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
    $self->{renderer} = WWW::Shopify::Liquid::XS::Renderer->new($self);
    $self->{parser} = WWW::Shopify::Liquid::XS::Parser->new($self);
    $self->{optimizer} = WWW::Shopify::Liquid::XS::Optimizer->new($self->{renderer});
    $_->apply($self) for (@{$self->{dialects}});
    WWW::Shopify::Liquid::XS::implementWebDialect($self->{context}) if $settings{'implement_web_dialect'};
    $self->register_filter('WWW::Shopify::Liquid::Filter::XS::Date');
    $self->register_filter('WWW::Shopify::Liquid::Filter::XS::Sprintf');
    return $self;
}

sub renderer { return $_[0]->{renderer}; }
sub parser { return $_[0]->{parser}; }
sub optimizer { return $_[0]->{optimizer}; }
sub analyzer { return $_[0]->{analyzer} ||= WWW::Shopify::Liquid::XS::Analyzer->new(liquid => $_[0]); }

sub DESTROY {
    my ($self) = @_;
    freeContext($self->{context});
}

use Encode;

sub parse_file {
    my ($self, $file) = @_;
    open(my $fh, "<", $file);
    my $text = decode("UTF-8", do { local $/; <$fh> });
    return $self->parse_text($text, $file);
}

sub parse_text {
    my ($self, $text, $file) = @_;
    return $self->parser->parse($text, $file);
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
    my $opsub = $tag->can('operate');
    my $optimizes = !$tag->can('optimizes') || $tag->optimizes ? 2 : 0;
    die WWW::Shopify::Liquid::XS::Exception->new("Requies tag to have an operate sub; currently custom processing is not supported.") unless $opsub;
    die WWW::Shopify::Liquid::XS::Exception->new("Can't register tag '" . $tag->name . "'.") unless WWW::Shopify::Liquid::XS::registerTag($self->{context}, $tag->name, ($tag->is_enclosing ? "enclosing" : "free"), $tag->min_arguments, (defined $tag->max_arguments ? $tag->max_arguments : -1), $optimizes, $tag->is_enclosing ? sub {
        my ($renderer, $node, $hash, $contents, @arguments) = @_;
        return $opsub->((bless {
            node => $node,
            renderer => $renderer
        }, $tag), $hash, $contents, @arguments);
    } : sub {
        my ($renderer, $node, $hash, @arguments) = @_;
        return $opsub->((bless {
            node => $node,
            renderer => $renderer
        }, $tag), $hash, @arguments);
    }) == 0;
}

sub register_filter {
    my ($self, $filter) = @_;
    my $opsub = $filter->can('operate');
    my $optimizes = !$filter->can('optimizes') || $filter->optimizes ? 2 : 0;
    die WWW::Shopify::Liquid::XS::Exception->new("Requires filter to have an operate sub; currently custom processing is not supported.") unless $opsub;
    die WWW::Shopify::Liquid::XS::Exception->new("Can't register filter '" . $filter->name . "'.") unless WWW::Shopify::Liquid::XS::registerFilter($self->{context}, $filter->name, $filter->min_arguments, (defined $filter->max_arguments ? $filter->max_arguments : -1), $optimizes, sub {
        my ($renderer, $node, $hash, $operand, @arguments) = @_;
        my $result = $opsub->((bless {
            node => $node,
            renderer => $renderer
        }, $filter), $hash, $operand, @arguments);
        return $result;
    }) == 0;
}


sub register_operator {
    my ($self, $operator) = @_;
    my $opsub = $operator->can('operate');
    my $optimizes = !$operator->can('optimizes') || $operator->optimizes ? 2 : 0;
    die WWW::Shopify::Liquid::XS::Exception->new("Requires operator to have an operate sub; currently custom processing is not supported.") unless $opsub;;
    die WWW::Shopify::Liquid::XS::Exception->new("Can't register operator '" . $operator->symbol. "'.") unless WWW::Shopify::Liquid::XS::registerOperator($self->{context}, $operator->symbol, $operator->arity, $operator->fixness, $operator->priority, $optimizes, sub {
        my ($renderer, $node, $hash, @operands) = @_;
        my $result = $opsub->((bless {
            node => $node,
            renderer => $renderer,
        }, $operator), $hash, @operands);
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
