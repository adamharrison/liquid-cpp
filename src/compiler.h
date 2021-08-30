#ifndef LIQUIDCOMPILER_H
#define LIQUIDCOMPILER_H

#include <vector>
#include <stack>
#include <unordered_map>
#include <string>

#include "common.h"
#include "renderer.h"

namespace Liquid {


    // Liquid is primarily unary operations on things with filters. So we have a primary register which most things occupy, and then the stack where everything that is not in primary position is affecte dby.
    // When referring to an operand, the return register is always 0x0, and anything else is that 4-byte position in the code array.
    // Every OP code has 2 operands.
    // Register 0, is known as the "return" register. As much work as possible is done in this memory, and basically all operations will assume
    // that this register is what's returning from any given thing.
    enum OPCode {
        OP_MOV,         // Copies one register to another.
        OP_MOVSTR,      // Pushes the operand from memory into the register.
        OP_MOVINT,      // Pushes the operand into the register.
        OP_MOVBOOL,     // Pushes the operand into the register.
        OP_MOVFLOAT,    // Pushes the operand into the register.
        OP_MOVNIL,      // Pushes the operand into the register.
        OP_STACK,       // Pulls the nth item of the stack into a register.
        OP_PUSH,        // Pushes the operand onto the stack, from the specified register.
        OP_POP,         // Moves the stack point back to the preivous variable.
        OP_ADD,         // Adds regsiter 0x0 and the target register.
        OP_SUB,         // Subtracts the target register from 0x0.
        OP_EQL,         // Checks whether the register is equal to register 0x0.
        OP_OUTPUT,      // Takes the return register, and appends it to the selected output buffer.
        OP_OUTPUTMEM,   // Takes the targeted memory address, and appends it to the selected output buffer. Optimized version of OP_OUTPUT to reduce copying.
        OP_ASSIGN,      // Assigning a variable specified in the target register. Key is at 0x0, and value is at the operand register.
        OP_JMP,         // Unconditional jump.
        OP_JMPFALSE,    // Jumps to the instruction if primary register is false.
        OP_JMPTRUE,     // Jumps to the instruction if the priamry register is true.
        OP_CALL,        // Calls the function specified with the amount of arugments on the stack.
        OP_RESOLVE,     // Resovles the named variable in the register and places it into the same register. Operand is either 0x0, for the top-level context, or a register, which contains the context for the next deference.,
        OP_LENGTH,      // Gets the length of the specified variable held in the target register, and puts it into 0x0.
        OP_ITERATE,     // Iterates through the variable in the specified register, and pops the value into 0x0. If iteration is over, JMPs to the specified instruction.
        OP_INVERT,      // Coerces to a boolean
        OP_PUSHBUFFER,  // Pushes a buffer onto to the buffer stack, with the contents of the target register.
        OP_POPBUFFER,   // Pops a buffer off the buffer stack, flushing the contents of the buffer to the target register.
        OP_EXIT         // Quits the program.
    };

    bool hasOperand(OPCode opcode);
    const char* getSymbolicOpcode(OPCode opcode);

    // Entrypoint is always codeOffset.
    // Then comes the data segment, where all strings are located.
    // Then comes the actual code segment.
    struct Program {
        unsigned int codeOffset;
        std::vector<unsigned char> code;
    };

    struct Compiler {
        std::vector<unsigned char> data;
        std::vector<unsigned char> code;
        int freeRegister;
        std::unordered_map<long long, int> existingStrings;

        const Context& context;
        // For forloops, and the forloop vairables, amongst other things. Space should likely be allocated only when actually  needed.
        // Returns the offset from the current stack frame.
        struct DropFrameState {
            int stackPoint;
        };
        int stackSize;
        typedef int (*DropFrameCallback)(Compiler& compiler, DropFrameState& state, const Node& node);
        std::unordered_map<std::string, std::vector<std::pair<DropFrameCallback, DropFrameState>>> dropFrames;

        void addDropFrame(const std::string& name, DropFrameCallback callback) {
            dropFrames[name].emplace_back(callback, DropFrameState { stackSize });
        }

        void clearDropFrame(const std::string& name) {
            dropFrames[name].pop_back();
        }

        Compiler(const Context& context);
        ~Compiler();

        int add(const char* str, int length);
        int add(OPCode code, int target);
        int add(OPCode code, int target, long long operand);

        int addPush(int target);
        int addPop(int amount);

        void modify(int offset, OPCode code, int target, long long operand);
        int currentOffset() const;

        // Called internally.
        int compileBranch(const Node& branch);
        Program compile(const Node& tmpl);

        string disassemble(const Program& program);
    };

    struct Interpreter : Renderer {
        static constexpr int SHORT_STRING_SIZE = 64;

        struct Register {
            enum class Type {
                FLOAT,
                INT,
                BOOL,
                NIL,
                SHORT_STRING,       // Inline, or in a register.
                LONG_STRING,        // On stack. Up to 65k; short is multiplexed with the type info.
                EXTRA_LONG_STRING,  // In memory.
                VARIABLE            // 3rd party variable.
            };

            Type type;
            union {
                double f;
                bool b;
                long long i;
                void* pointer;
                struct {
                    unsigned char length;
                    char buffer[SHORT_STRING_SIZE];
                };
            };
        };

        static constexpr int STACK_SIZE = 100*1024;
        static constexpr int TOTAL_REGISTERS = 4;
        static constexpr int MAX_FRAMES = 128;

        stack<string> buffers;

        Register registers[TOTAL_REGISTERS];
        const unsigned int* instructionPointer;
        char* stackPointer;

        int frames[MAX_FRAMES];
        char stackBlock[STACK_SIZE];

        Interpreter(const Context& context);
        Interpreter(const Context& context, LiquidVariableResolver resolver);
        ~Interpreter();

        // Should be used for conversions only, not for actual optimized code.
        Node getStack(int i);
        void getStack(Register& reg, int i);
        void popStack(int i);
        void pushStack(Register& reg);
        void pushRegister(Register& reg, const Node& node);
        void pushRegister(Register& reg, const string& str);
        void pushRegister(Register& reg, string&& str);

        bool run(const unsigned char* code, Variable store, void (*callback)(const char* chunk, size_t len, void* data), void* data, const unsigned char* iteration = nullptr);

        void renderTemplate(const Program& tmpl, Variable store, void (*)(const char* chunk, size_t len, void* data), void* data);
        string renderTemplate(const Program& tmpl, Variable store);
    };
}

#endif
