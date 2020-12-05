Gem::Specification.new "liquidc", "1.0" do |s|
	s.name		= 'liquidc'
	s.version	= '0.0.1'
	s.date		= '2020-12-05'
	s.summary	= "LiquidC"
	s.description	= "The ruby gem that hooks into the liquid-cpp library."
	s.authors	= ["Adam Harrison"]
	s.email		= 'adamdharrison@gmail.com'
	s.files		= ["lib/liquidc.rb"]
	s.homepage	= 'https://github.com/adamharrison/liquid-cpp'
	s.license	= 'MIT'
	s.extensions 	= ["ext/liquidc/extconf.rb"]
end
