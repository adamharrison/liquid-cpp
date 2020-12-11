require 'liquid'
require 'liquidc'

context = LiquidC.new("strict")
renderer = LiquidC::Renderer.new(context)


templateContent = "{% if a %}asdfghj {{ a }}{% else %}asdfjlsjkhgsjlkhglsdfjkgdfhs{% for i in (1..10) %}{{ i }}fasdfsdf{% endfor %}{% endif %}"


start = Time.now

template1 = LiquidC::Template.new(context, templateContent)
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
