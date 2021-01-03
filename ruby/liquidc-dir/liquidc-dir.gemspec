Gem::Specification.new "liquidc-dir", "0.1.0" do |s|
	s.name		    = 'liquidc-dir'
	s.version	    = '1.0.0'
	s.date		    = '2021-01-03'
	s.summary	    = "LiquidC Drop-In-Replacement"
	s.description   = "A gem that replaces the normal liquid classes with LiquidC versions."
	s.authors	    = ["Adam Harrison"]
	s.email		    = 'adamdharrison@gmail.com'
	s.homepage	    = 'https://github.com/adamharrison/liquid-cpp'
	s.license       = 'MIT'
	s.files         = ["lib/liquidc-dir.rb"]
	s.add_runtime_dependency 'liquidc', '~> 1.0'
end
