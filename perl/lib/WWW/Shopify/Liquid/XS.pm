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
    my ($package, $context) = @_;
    return bless { renderer => WWW::Shopify::Liquid::XS::createRenderer($context) }, $package;
}

sub DESTROY {
    my ($self) = @_;
    WWW::Shopify::Liquid::XS::freeRenderer($self->{renderer});
}

sub render {
    my ($self, $hash, $template) = @_;
    my $error = [];
    return WWW::Shopify::Liquid::XS::renderTemplate($self->{renderer}, $hash, $template->{template}, $error);
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
    my ($package) = @_;
    my $self = bless { context => createContext() }, $package;
    WWW::Shopify::Liquid::XS::implementStandardDialect($self->{context});
    return $self;
}

sub DESTROY {
    my ($self) = @_;
    freeContext($self->{context});
}

sub renderer {
    my ($self) = @_;
    $self->{renderer} = WWW::Shopify::Liquid::XS::Renderer->new($self->{context}) if !$self->{renderer};
    return $self->{renderer};
}

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
