# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl WWW-Shopify-Liquid-XS.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use strict;
use warnings;

use Test::More tests => 1;
BEGIN { use_ok('WWW::Shopify::Liquid::XS') };

#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.

