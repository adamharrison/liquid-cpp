require './LiquidC'

context = LiquidC.new()
renderer = LiquidC::Renderer.new(context)
template = LiquidC::Template.new(context, "{% if a %}asdfghj{% endif %}")

puts renderer.render({ "a" => 1 }, template)