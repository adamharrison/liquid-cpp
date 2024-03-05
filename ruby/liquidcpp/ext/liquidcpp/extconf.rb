#!/usr/bin/env ruby

require 'mkmf'

extension_name = 'liquidcpp/liquidcpp'

dir_config(extension_name);

$CFLAGS += " -O0 -g"
# $CFLAGS += " -O3"

$libs = append_library($libs, "stdc++");
$libs = append_library($libs, "liquid");

create_makefile(extension_name);
