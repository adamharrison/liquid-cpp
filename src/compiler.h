#ifndef LIQUIDCOMPILER_H
#define LIQUIDCOMPILER_H

#include <vector>
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
        OP_PUSH,        // Pushes the operand onto the stack, from the specified register.
        OP_POP,         // Moves the stack point back to the preivous variable.
        OP_ADD,         // Takes the targeted register, and adds it to the return register.
        OP_SUB,         // Takes the targeted register, and sub it to the return register.
        OP_MUL,         // Takes the targeted register, and multplies it to the return register.
        OP_MOD,         // Takes the targeted register, and mods it to the return register.
        OP_DIV,         // Takes the targeted register, and divs it to the return register.
        OP_OUTPUT,      // Takes the return register, and appends it to the selected output buffer.
        OP_OUTPUTMEM,   // Takes the targeted memory address, and appends it to the selected output buffer. Optimized version of OP_OUTPUT to reduce copying.
        OP_ASSIGN,      // Assigning a variable from the contents of the return register to the specified operand. If 0x0 to the gloabl vairable context.
        OP_JMP,         // Unconditional jump.
        OP_JMPFALSE,     // Jumps to the instruction if primary register is false.
        OP_CALL,        // Calls the function specified with the amount of arugments on the stack.
        OP_RESOLVE,      // Resovles the named variable in the register and places it into the same register. Operand is either 0x0, for the top-level context, or a register, which contains the context for the next deference.,
        OP_EXIT         // Quits the program.
    };

    bool hasOperand(OPCode opcode);
    const char* getSymbolicOpcode(OPCode opcode);

    // Entrypoint is always codeOffset.
    // Then comes the data segment, where all strings are located.
    // Then comes the actual code segment.
    struct Program {
        unsigned int codeOffset;
        std::vector<char> code;
    };

    struct Compiler {
        std::vector<char> data;
        std::vector<char> code;
        int freeRegister;
        std::unordered_map<long long, int> existingStrings;

        const Context& context;
        // For forloops, and the forloop vairables, amongst other things. Space should likely be allocated only when actually  needed.
        // Returns the offset from the current stack frame.
        typedef int (dropFrameCallback)(void* data);
        std::unordered_map<std::string, std::vector<int>> dropFrames;

        Compiler(const Context& context);
        ~Compiler();

        int add(const char* str, int length);
        int add(OPCode code, int target);
        int add(OPCode code, int target, long long operand);
        void modify(int offset, OPCode code, int target, long long operand);
        int currentOffset() const;

        // Called internally.
        int compileBranch(const Node& branch);
        Program compile(const Node& tmpl);

        string decompile(const Program& program);
    };

    struct Interpreter : Renderer {
        static constexpr int SHORT_STRING_SIZE = 64;

        struct Register {
            enum class Type {
                FLOAT,
                INT,
                BOOL,
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

        int buffer = 0;
        string buffers[2];
        Register registers[TOTAL_REGISTERS];
        char* stackPointer;

        int frames[MAX_FRAMES];
        char stack[STACK_SIZE];

        Interpreter(const Context& context, LiquidVariableResolver resolver);
        ~Interpreter();

        // Should be used for conversions only, not for actual optimized code.
        Node getStack(int i);
        void popStack(int i);
        void pushRegister(Register& reg, const Node& node);

        string renderTemplate(const Program& tmpl, Variable store);
    };
}

#endif
