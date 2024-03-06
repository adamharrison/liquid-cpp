require "liquidcpp/liquidcpp"
class LiquidCPP 
    alias_method :orig_initialize, :initialize
    def initialize(*args)
      orig_initialize(*args)
      self.registerFilter("date", 1, 1, LiquidCPP::OPTIMIZATION_SCHEME_NONE, Proc.new { |renderer, node, store, operand, argument|
        timeOp = nil
        case operand
        when String
            if operand == 'now' then
            timeOp = Time.now
            elsif operand.to_i.to_s == operand then #epoch integer enterd as a String
            timeOp = Time.at(operand.to_i)
            else #raw date entered as string. "2021-09-01T00:00:00-00:00" ex
            begin
                timeOp = Time.parse(operand)
            rescue => e #junk string entered
                timeOp = nil
            end
            end
        when Time
            timeOp = operand
        when  Integer #epoch int
            timeOp = Time.at(operand)
        else
            timeOp = nil
        end
        next unless timeOp && timeOp.class == Time
        time_utc = timeOp.utc
        time_utc.strftime(argument[0].to_s)
      })
    end
  end