require 'liquidcpp'

module Liquid

    class Tag
    end

    class Block < Tag
        attr_accessor :body

        def render(stash)
            return self.body
        end
    end

    class Include < Tag
        def render(stash)

        end
    end

    class LocalFileSystem
        attr_accessor :root

        def initialize(path)
            self.root = path
        end

        def full_path(path)

        end
    end



    class Error < StandardError
        def self.translate_error(e)
            if e.is_a?(LiquidCPP::Parser::Error)
                case e.type
                when 2
                    error = SyntaxError.new("Unknown tag '" + e.message + "'", e.line, e.file)
                when 7
                    error = SyntaxError.new("Invalid arguments for '" + e.args[0] + "', expected " + e.args[1] + ", got" + e.args[1], e.line, e.file)
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

    class ArgumentError < Error

    end

    class FileSystemError < Error

    end

    class ContextError < Error

    end

    class StackLevelError < Error

    end

    class MemoryError < Error

    end

    class ZeroDivisionError < Error

    end

    class FloatDomainError < Error

    end

    class UndefinedVariable < Error

    end

    class UndefinedDropMethod < Error

    end

    class UndefinedFilter < Error

    end

    class MethodOverrideError < Error

    end

    class DisabledError < Error

    end

    class InternalError < Error

    end

    class Template
        attr_reader :template
        attr_accessor :errors

        @@globalContext = LiquidCPP.new("strict")
        @@globalErrorMode = :lax

        def self.error_mode
            @@globalErrorMode
        end

        def self.error_mode=new_mode
            @@globalErrorMode = new_mode
        end

        def initialize(str, attrs)
            parser = LiquidCPP::Parser.new(@@globalContext)
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
            rescue LiquidCPP::Parser::Error => e
                error = Liquid::Error.translate_error(e)
            end
            raise error if error
        end

        def self.parse(str, attrs = {})
            return Liquid::Template.new(str, attrs)
        end

        def render(stash, attrs = {})
            renderer = LiquidCPP::Renderer.new(@@globalContext)
            renderer.setStrictVariables(true) if attrs[:strict_variables]
            renderer.setStrictFilters(true) if attrs[:strict_filters]
            result = renderer.render(stash, self.template)
            self.errors = renderer.warnings
            return result
        end

        # I don't know wtf they were thinking.
        def self.register_filter(mod)
            mod.public_instance_methods.each { |x|
                @@globalContext.registerFilter(x.to_s, -1, -1, LiquidC::OPTIMIZATION_SCHEME_FULL, Proc.new{ |renderer, node, stash, operand|
                    mod[x].call(operand)
                })
            }
        end

        def self.register_tag(name, klass)
            if klass.is_a?(Liquid::Block)
                @@globalContext.registerTag(name, LiquidC::TAG_TYPE_ENCLOSING, -1, -1, LiquidC::OPTIMIZATION_SCHEME_FULL, Proc.new { |renderer, node, stash, child, arguments|
                    klass.new(name, *arguments).render(stash)
                })
            else
                @@globalContext.registerTag(name, LiquidC::TAG_TYPE_FREE, -1, -1, LiquidC::OPTIMIZATION_SCHEME_FULL, Proc.new { |renderer, node, stash, child, arguments|
                    tag = klass.new(name, *arguments)
                    tag.instance_variable_set(:body, child)
                    tag.render(stash)
                })
            end
        end
    end

    Liquid::Template.registerTag("include", Liquid::Include)
end
