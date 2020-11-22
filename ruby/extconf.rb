#!/usr/bin/env ruby

require 'mkmf'

extension_name = 'LiquidC'

dir_config(extension_name);


$libs = append_library($libs, "stdc++");

create_makefile(extension_name);
