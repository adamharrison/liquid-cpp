#!/usr/bin/env ruby

require 'liquidc-dir'

template = Liquid::Template.parse("{{ a }}")

puts template.render({ a => 1 })
