#!/usr/bin/env ruby

require 'liquid'
require 'liquidc'

context = LiquidC.new("strict")
renderer = LiquidC::Renderer.new(context)
parser = LiquidC::Parser.new(context)
optimizer = LiquidC::Optimizer.new(renderer)
context.registerFilter("test", -1, -1, 0, Proc.new{ |renderer, node, stash, operand|
    "operand" + operand
})

puts renderer.render({ }, parser.parse("{% endif %}"))
puts parser.warnings
puts renderer.warnings

templateContent = "{% if a %}asdfghj {{ a }}{% else %}asdfjlsjkhgsjlkhglsdfjkgdfhs{% for i in (1..10) %}{{ i }}fasdfsdf{% endfor %}{% endif %}"

start = Time.now

template1 = parser.parse(templateContent)
optimizer.optimize({ }, template1);
template2 = Liquid::Template.parse(templateContent)

s = nil
(1..10000).each { |x|
	s = renderer.render({ "a" => false }, template1)
}
puts s

puts (Time.now - start)*1000

start = Time.now

(1..10000).each { |x|
	s = template2.render({ "a" => false })
}
puts s

puts (Time.now - start)*1000


templateContent = '{{ "asdadasdAS" | test }}'

puts "WAT";

templateNew = parser.parse(templateContent);

puts "TEMPLATE NEW: #{templateNew}";

puts "TEST: " + renderer.render({ }, templateNew);

puts "D";
