require 'liquidc'

module Liquid
    class Error < StandardError
        def self.translate_error(e)
            if e.is_a?(LiquidC::Parser::Error)
                case e.type
                when 2
                    error = SyntaxError.new("Unknown tag '" + e.message + "'", e.line, e.file)
                when 7
                    error = SyntaxError.new("Invalid arguments for '" + e.message + "', expected " + e.args[0].to_s + ", got " +  + ", e.line, e.file)
                else
                    error = SyntaxError.new(e.message, e.line, e.file)
                end
            end
        end
    end

    class SyntaxError < Error
        attr_accessor :line_number
        attr_accessor :template_name

        def initialize(message, line_number, template_name)
            super("Liquid syntax error: " + message)
            self.line_number = line_number
            self.template_name = template_name
        end
    end

    class Template
        attr_reader :template
        attr_accessor :errors

        @@globalContext = LiquidC.new("strict")
        @@globalErrorMode = :lax

        def self.error_mode
            @@globalErrorMode
        end

        def self.error_mode=new_mode
            @@globalErrorMode = new_mode
        end

        def initialize(str, attrs)
            parser = LiquidC::Parser.new(@@globalContext)
            error_mode = attrs[:error_mode] || @@globalErrorMode
            error = nil
            begin
                @template = parser.parse(str)
                warnings = parser.warnings
                if error_mode != :lax && warnings.size > 0
                    self.errors = warnings
                    if error_mode == :strict
                        raise self.errors[0]
                    end
                end
                # Raise on exceptions that :lax raises on, which is apparently not accepting of "everything" as the docs say.
                raise warnings.select { |x| x.type == 2 }.first if warnings.select { |x| x.type == 2 }.size > 0
            rescue LiquidC::Parser::Error => e
                error = Liquid::Error.translate_error(e)
            end
            raise error if error
        end

        def self.parse(str, attrs = {})
            return Liquid::Template.new(str, attrs)
        end

        def render(stash, attrs = {})
            renderer = LiquidC::Renderer.new(@@globalContext)
            renderer.setStrictVariables(true) if attrs[:strict_variables]
            renderer.setStrictFilters(true) if attrs[:strict_filters]
            result = renderer.render(stash, self.template)
            self.errors = renderer.warnings
            return result
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
