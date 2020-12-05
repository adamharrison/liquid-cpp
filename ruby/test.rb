require 'liquid'
require './LiquidC'

context = LiquidC.new("strict")
renderer = LiquidC::Renderer.new(context)


templateContent = "{% if a %}asdfghj {{ a }}{% else %}asdfjlsjkhgsjlkhglsdfjkgdfhs{% for i in (1..10) %}{{ i }}fasdfsdf{% endfor %}{% endif %}"


start = Time.now

(1..10000).each { |x| 
	template = LiquidC::Template.new(context, templateContent)
	s = renderer.render({ "a" => 0 }, template)
	raise s
}

puts (Time.now - start)*1000

start = Time.now

(1..10000).each { |x| 
	template = Liquid::Template.parse(templateContent)
	s = template.render({ "a" => 0 })
}

puts (Time.now - start)*1000
