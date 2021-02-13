#!/usr/bin/env ruby

require 'liquid'
require 'liquid/c'
require 'liquidcpp'

context = LiquidCPP.new("strict")
renderer = LiquidCPP::Renderer.new(context)
parser = LiquidCPP::Parser.new(context)
compiler = LiquidCPP::Compiler.new(context)
optimizer = LiquidCPP::Optimizer.new(renderer)

context.registerFilter("test", -1, -1, LiquidCPP::OPTIMIZATION_SCHEME_NONE, Proc.new { |renderer, node, stash, operand|
    "operand" + operand
})
context.registerTag("enclosingtag", LiquidCPP::TAG_TYPE_ENCLOSING, -1, -1, LiquidCPP::OPTIMIZATION_SCHEME_NONE, Proc.new { |renderer, node, stash, child, arguments|
    'enclosed'
})
context.registerTag("freetag", LiquidCPP::TAG_TYPE_FREE, -1, -1, LiquidCPP::OPTIMIZATION_SCHEME_NONE, Proc.new { |renderer, node, stash, arguments|
    'free'
})

require 'json'

tmpl = parser.parseTemplate("{{ product.id }}")
json = JSON.parse('{"product":{"id":1}}')

puts "TEST1: " + renderer.render(json, tmpl)


text = renderer.render({ }, parser.parseTemplate("{% enclosingtag i %}{% endenclosingtag %}{% freetag %}"))
puts "TEST: " + text
raise "No warnings." if parser.warnings.size > 0


puts renderer.render({ }, parser.parseTemplate("{% endif %}"))
puts parser.warnings
puts renderer.warnings

templateContent = "{% if a %}asdfghj {{ a }}{% else %}asdfjlsjkhgsjlkhglsdfjkgdfhs{% for i in (1..100) %}{{ i }}fasdfsdf{% endfor %}{% endif %}"


puts "A"

template1 = parser.parseTemplate(templateContent)
optimizer.optimize({ }, template1);
template2 = compiler.compileTemplate(template1)
template3 = Liquid::Template.parse(templateContent)

# raise compiler.decompileProgram(template2)

iterations = 10000

start = Time.now

i = 0
(1..iterations).each { |x|
    ++i
}

puts (Time.now - start)*1000

start = Time.now

s = nil
(1..iterations).each { |x|
	s = renderer.render({ "a" => false }, template1)
}
puts s

puts (Time.now - start)*1000

start = Time.now

s = nil
(1..iterations).each { |x|
	s = renderer.render({ "a" => false }, template2)
}
puts s

puts (Time.now - start)*1000


start = Time.now

(1..iterations).each { |x|
	s = template3.render({ "a" => false })
}
puts s

puts (Time.now - start)*1000


templateContent = '{{ "asdadasdAS" | test }}'

puts "WAT";

templateNew = parser.parseTemplate(templateContent);

puts "TEMPLATE NEW: #{templateNew}";

puts "TEST: " + renderer.render({ }, templateNew);

puts "D";
