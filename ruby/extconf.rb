#!/usr/bin/env ruby

require 'mkmf'

extension_name = 'LiquidC'

dir_config(extension_name);

$libs = append_library($libs, "stdc++");
$libs = append_library($libs, "liquid");

create_makefile(extension_name);
