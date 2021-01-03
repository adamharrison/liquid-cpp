require 'liquidc'

module Liquid
    class Template
        attr_reader :template

        @@globalContext = LiquidC.new("strict")
        @@globalErrorMode = :lax

        def self.error_mode
            @@globalErrorMode
        end

        def self.error_mode=new_mode
            @@globalErrorMode = new_mode
        end

        def initialize(str, attrs)
            self.template = LiquidC::Template.new(@@globalContext, str, {
                :error_mode => (attrs[:error_mode] || @@globalErrorMode)
            })
        end

        def self.parse(str, attrs = {})
            return Liquid::Template.new(str, attrs)
        end

        def render(stash, attrs)
            renderer = LiquidC::Renderer.new(@@globalContext)
            return renderer.render(stash, self.template)
        end

        # I don't know wtf they were thinking.
        def self.register_filter(mod)
            mod.public_instance_methods.each { |x|
                @@globalContext.registerFilter(x.to_s, -1, -1, 0, Proc.new{ |renderer, node, stash, operand|
                    mod[x].call(operand)
                })
            }
        end

        def self.register_tag(name, klass)

        end
    end
end
