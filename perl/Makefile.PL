use 5.024002;
use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    NAME              => 'WWW::Shopify::Liquid::XS',
    VERSION_FROM      => 'lib/WWW/Shopify/Liquid/XS.pm', # finds $VERSION, requires EU::MM from perl >= 5.5
    PREREQ_PM         => {}, # e.g., Module::Name => 1.1
    ABSTRACT_FROM     => 'lib/WWW/Shopify/Liquid/XS.pm', # retrieve abstract from module
    AUTHOR            => 'Adam Harrison <adamdharrison@gmail.com>',
    LICENSE           => 'mit',
    #Value must be from legacy list of licenses here
    #http://search.cpan.org/perldoc?Module%3A%3ABuild%3A%3AAPI
    LIBS              => ['-L../bin -lliquid -lcrypto -lstdc++'], # e.g., '-lm'
    DEFINE            => '-DLIQUID_INCLUDE_WEB_DIALECT', # e.g., '-DHAVE_SOMETHING'
    INC               => '-I.', # e.g., '-I. -I/usr/include/other'
	# Un-comment this if you add C files to link with later:
    # OBJECT            => '$(O_FILES)', # link all the C files too
);
