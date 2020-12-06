#!/usr/bin/env ruby

require 'mkmf'

extension_name = 'liquidc'

dir_config(extension_name);

$CFLAGS += " -O3 -I" + File.absolute_path(File.dirname(__FILE__) + "/../../../src")

$libs = append_library($libs, "stdc++");
$libs += "-L" + File.absolute_path(File.dirname(__FILE__) + "/../../../bin") + " -lliquid";

create_makefile(extension_name);
