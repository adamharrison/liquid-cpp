#include <cstring>

#include "compiler.h"

namespace Liquid {

    // Instructions are
    // 1 byte op-code.
    // 3 byte for register designation.
    // 4 byte for operand.
    // 8 bytes per instruction total.

    // Sample things to run:
    // ahsdfjkghsjgf
    // OP_MOVSTR reg0, 0xDEADBEEF
    // OP_OUTPUT

    // jaslkdjfasdkf {{ a }} kjhdfjkhgsdfg
    // OP_MOVSTR    reg0, 0xDEADBEEF
    // OP_OUTPUT    reg0
    // OP_MOVSTR    reg0, 0xDEADBEEF
    // OP_RESOLVE   reg0, 0x0
    // OP_OUTPUT    reg0
    // OP_MOVSTR    reg0, 0xDEADBEEF
    // OP_OUTPUT    reg0
    // OP_EXIT

    // kjhafsdjkhfjkdhsf {{ a + 1 }} jhafsdkhgsdfjkg
    // OP_MOVSTR    reg0, 0xDEADBEEF
    // OP_OUTPUT    reg0
    // OP_MOVSTR    reg0, 0xDEADBEEF
    // OP_RESOLVE   reg0, 0x0
    // OP_MOVINT    reg1, 1
    // OP_ADD       reg1
    // OP_OUTPUT    reg0
    // OP_MOVSTR    reg0, 0xDEADBEEF
    // OP_OUTPUT    reg0
    // OP_EXIT


    // afjkdhsjlkghsdfkhgkfjl {{ a - 1 }} dshfjgdjkhgf
    // OP_MOVSTR    reg0, 0xDEADBEEF
    // OP_OUTPUT    reg0
    // OP_MOVSTR    reg0, 0xDEADBEEF
    // OP_RESOLVE   reg0, 0x0
    // OP_MOVINT    reg1, 1
    // OP_SUB       reg0, reg1
    // OP_OUTPUT    reg0
    // OP_MOVSTR    reg0, 0xDEADBEEF
    // OP_OUTPUT    reg0
    // OP_EXIT

    // fahsdjkhgflghljh {% if a %}A{% else %}B{% endif %}kjdjkghdf
    // OP_MOVSTR    reg0, 0xDEADBEEF
    // OP_OUTPUT    reg0
    // OP_MOVSTR    reg0, 0xDEADBEEF
    // OP_RESOLVE   reg0, 0x0
    // OP_JMPFALSE  reg0, 0xDEADBEEF        -- JMPs to B
    // OP_MOVSTR    reg0, 0xDEADBEEF        -- A
    // OP_OUTPUT    reg0
    // OP_JMP       0xDEADBEEF              -- JMPs to C
    // OP_MOVSTR    reg0, 0xDEADBEEF        -- B
    // OP_OUTPUT    reg0
    // OP_MOVSTR    reg0, 0xDEADBEEF        -- C
    // OP_OUTPUT    reg0
    // OP_EXIT

    // asdjkfsdhsjkg {{ a | plus: 3 | minus: 5 | multiply: 6 }}
    // OP_MOVSTR    reg0, 0xDEADBEEF
    // OP_OUTPUT    reg0
    // OP_MOVSTR    reg0, 0xDEADBEEF
    // OP_RESOLVE   reg0
    // OP_MOVINT    reg1, 3
    // OP_ADD       reg0, reg1
    // OP_MOVINT    reg0, 5
    // OP_SUB       reg0, reg1
    // OP_MOVINT    reg0, 6
    // OP_MUL       reg0, reg1
    // OP_OUTPUT    reg0
    // OP_EXIT


    // asdjkfsdhsjkg {{ a.b.c }}
    // OP_MOVSTR    reg0, 0xDEADBEEF
    // OP_OUTPUT    reg0
    // OP_MOVSTR    reg0, 0xDEADBEEF
    // OP_RESOLVE   reg0, 0x0
    // OP_MOV       reg1, reg0
    // OP_MOVSTR    reg0, 0xDEADBEEF
    // OP_RESOLVE   reg0, reg1
    // OP_MOV       reg1, reg0
    // OP_MOVSTR    reg0, 0xDEADBEEF
    // OP_RESOLVE   reg0, reg1
    // OP_OUTPUT    reg0
    // OP_EXIT


    // {{ asddfs | custom_filter: (2 + 57), 5, 7 }}
    // OP_MOVINT    reg0, 2
    // OP_MOVINT    reg1, 57
    // OP_ADD       reg0, reg1
    // OP_PUSH      reg0
    // OP_MOVINT    reg0, 5
    // OP_PUSH      reg0
    // OP_MOVINT    reg0, 7
    // OP_PUSH      reg0
    // OP_MOVSTR    reg0, 0xDEADBEEF,
    // OP_RESOLVE   reg0, 0x0
    // OP_CALL      0xDEADBEEF
    // OP_POP       0x3
    // OP_OUTPUT    reg0
    // OP_EXIT

    // {% if a %}asdfghj {{ a }}{% else %}asdfjlsjkhgsjlkhglsdfjkgdfhs{% for i in (1..10) %}{{ i }}fasdfsdf{% endfor %}{% endif %}
    // OP_MOVSTR    reg0, 0xDEADBEEF
    // OP_RESOLVE   reg0, 0x0
    // OP_JMPFALSE  reg0, 0xDEADBEEF        -- To A
    // OP_MOVSTR    reg0, 0xDEADBEEF
    // OP_OUTPUT    reg0
    // OP_MOVSTR    reg0, 0xDEADBEEF
    // OP_RESOLVE   reg0, 0x0
    // OP_OUTPUT    reg0
    // OP_JMP       0xDEADBEEF              -- To B
    // OP_MOVSTR    reg0, 0xDEADBEEF        -- A
    // OP_OUTPUT    reg0
    // OP_MOVINT    reg0, 0x1
    // OP_PUSHINT   reg0
    // OP_MOVINT    reg0, STACK-4           -- Loop start (C)
    // OP_MOVINT    reg1, 10
    // OP_SUB       reg0, reg1
    // OP_JMPFALSE  reg0, 0xDEADBEEF        -- To B
    // OP_OUTPUT    reg0
    // OP_MOVSTR    reg0, 0xDEADBEEF
    // OP_JMP       reg0, 0xDEADBEEF        -- To C
    // OP_EXIT                              -- B



    // {% if a %}asdfghj {{ a }}{% else %}asdfjlsjkhgsjlkhglsdfjkgdfhs{% for i in (1..10) %}{% for j in (1..10) %}{{ i }}fasdfsdf{{ j }}{% endfor %}{% endfor %}{% endif %}
    // OP_MOVSTR    reg0, 0xDEADBEEF
    // OP_RESOLVE   reg0, 0x0
    // OP_JMPFALSE  reg0, 0xDEADBEEF        -- To A
    // OP_MOVSTR    reg0, 0xDEADBEEF
    // OP_OUTPUT    reg0
    // OP_MOVSTR    reg0, 0xDEADBEEF
    // OP_RESOLVE   reg0, 0x0
    // OP_OUTPUT    reg0
    // OP_JMP       0xDEADBEEF              -- To B
    // OP_MOVSTR    reg0, 0xDEADBEEF        -- A
    // OP_OUTPUT    reg0
    // OP_MOVINT    reg0, 0x1
    // OP_PUSHINT   reg0
    // OP_MOVINT    reg0, STACK-4           -- Loop start (C)
    // OP_MOVINT    reg1, 10
    // OP_SUB       reg0, reg1
    // OP_JMPFALSE  reg0, 0xDEADBEEF        -- To B
    // OP_OUTPUT    reg0
    // OP_MOVSTR    reg0, 0xDEADBEEF
    // OP_JMP       reg0, 0xDEADBEEF        -- To C
    // OP_EXIT                              -- B

    // This should probably be changed out, but am super lazy at present.
    size_t hash(const char* s, int len) {
        size_t h = 5381;
        int c;
        const char* end = s + len;
        while (((c = *s++) && s <= end))
            h = ((h << 5) + h) + c;
        return h;
    }

    bool hasOperand(OPCode opcode) {
        switch (opcode) {
            case OP_EXIT:
            case OP_OUTPUT:
            case OP_ADD:
            case OP_SUB:
            case OP_MUL:
            case OP_MOD:
            case OP_DIV:
            case OP_PUSH:
                return false;
            default:
                return true;
        }
    }

    const char* getSymbolicOpcode(OPCode opcode) {
        switch (opcode) {
            case OP_MOV:
                return "OP_MOV";
            case OP_MOVSTR:
                return "OP_MOVSTR";
            case OP_MOVINT:
                return "OP_MOVINT";
            case OP_MOVFLOAT:
                return "OP_MOVFLOAT";
            case OP_PUSH:
                return "OP_PUSH";
            case OP_POP:
                return "OP_POP";
            case OP_ADD:
                return "OP_ADD";
            case OP_SUB:
                return "OP_SUB";
            case OP_MUL:
                return "OP_MUL";
            case OP_MOD:
                return "OP_MOD";
            case OP_DIV:
                return "OP_DIV";
            case OP_OUTPUT:
                return "OP_OUTPUT";
            case OP_ASSIGN:
                return "OP_ASSIGN";
            case OP_JMP:
                return "OP_JMP";
            case OP_JMPFALSE:
                return "OP_JMPFALSE";
            case OP_CALL:
                return "OP_CALL";
            case OP_RESOLVE:
                return "OP_RESOLVE";
            case OP_EXIT:
                return "OP_EXIT";
        }
        assert(false);
        return nullptr;
    }

    int Compiler::add(const char* str, int len) {
        size_t hashValue = hash(str, len);
        auto it = existingStrings.emplace(hashValue, data.size());
        if (it.second) {
            int offset = data.size();
            int size = sizeof(len) + len + 1;
            data.resize(offset + size);
            *((int*)&data[offset]) = len;
            memcpy(&data[offset+sizeof(int)], str, len);
            data[offset+len+sizeof(int)] = 0;
            return offset;
        } else {
            return it.first->second;
        }
    }

    int Compiler::add(OPCode opcode, int target) {
        int offset = code.size();
        code.resize(offset + sizeof(int));
        *((int*)&code[offset]) = (opcode & 0xFF) | ((target << 8) & 0xFFFF00);
        return offset;
    }

    int Compiler::add(OPCode opcode, int target, long long operand) {
        int offset = code.size();
        code.resize(offset + sizeof(int) + sizeof(long long));
        *((int*)&code[offset]) = (opcode & 0xFF) | ((target << 8) & 0xFFFF00);
        *((long long*)&code[offset+sizeof(int)]) = operand;
        return offset;
    }

    Compiler::Compiler(const Context& context) : context(context) {

    }

    Compiler::~Compiler() {

    }

    int Compiler::compileBranch(const Node& branch) {
        int offset = code.size();
        if (!branch.type) {
            switch (branch.variant.type) {
                case Variant::Type::STRING: {
                    add(OP_MOVSTR, freeRegister, add(branch.variant.s.data(), branch.variant.s.size()));
                    ++freeRegister;
                } break;
                case Variant::Type::INT: {
                    add(OP_MOVINT, freeRegister, branch.variant.i);
                    ++freeRegister;
                } break;
                case Variant::Type::FLOAT: {
                    // Intentionally i.
                    add(OP_MOVFLOAT, freeRegister, branch.variant.i);
                    ++freeRegister;
                } break;
                default:
                    assert(false);
                break;
            }
        } else {
            branch.type->compile(*this, branch);
        }
        return offset;
    }

    Program Compiler::compile(const Node& tmpl) {
        Program program;
        freeRegister = 0;
        data.clear();
        code.clear();
        existingStrings.clear();

        compileBranch(tmpl);

        add(OP_EXIT, 0x0);
        program.code.resize(code.size() + data.size());
        memcpy(&program.code[0], data.data(), data.size());
        program.codeOffset = data.size();
        memcpy(&program.code[program.codeOffset], code.data(), code.size());
        return program;
    }

    string Compiler::decompile(const Program& program) {
        size_t i = 0;
        string result;
        char buffer[128];
        while (i < program.codeOffset) {
            sprintf(buffer, "0x%08x", (int)i);
            int length = *((int*)&program.code[i]);
            result.append(buffer);
            result.append(" \"");
            result.append(&program.code[i+sizeof(int)], length);
            result.append("\"");
            result.append("\n");
            i += length + sizeof(int)+1;
        }
        while (i < program.code.size()) {
            unsigned int instruction = *(unsigned int*)&program.code[i];
            sprintf(buffer, "0x%08x %-12s REG%02d", (int)i, getSymbolicOpcode((OPCode)(program.code[i] & 0xFF)), instruction >> 8);
            result.append(buffer);
            i += sizeof(int);
            if (hasOperand((OPCode)(instruction & 0xFF))) {
                sprintf(buffer, ", 0x%08x", (unsigned int)*(long long*)&program.code[i]);
                result.append(buffer);
                i += sizeof(long long);
            }
            result.append("\n");
        }
        return result;
    }

    Interpreter::Interpreter(const Context& context, LiquidVariableResolver resolver) : context(context), resolver(resolver) {
        buffers[0].reserve(10*1024);
        buffers[1].reserve(10*1024);
    }
    Interpreter::~Interpreter() {

    }

    string Interpreter::renderTemplate(const Program& prog, Variable store) {
        buffer = 0;
        const unsigned int* instructionPointer = reinterpret_cast<const unsigned int*>(&prog.code[prog.codeOffset]);
        unsigned int instruction, target;
        long long operand;
        Variable var;
        bool finished = false;
        while (!finished) {
            instruction = *instructionPointer++;
            target = instruction >> 8;
            switch (instruction & 0xFF) {
                case OP_MOVSTR: {
                    operand = *((long long*)instructionPointer); instructionPointer += 2;
                    unsigned int length = *(unsigned int*)&prog.code[operand];
                    assert(length < SHORT_STRING_SIZE);
                    registers[target].type = Register::Type::SHORT_STRING;
                    registers[target].length = length;
                    memcpy(registers[target].buffer, &prog.code[operand+sizeof(unsigned int)], length);
                    registers[target].buffer[length] = 0;
                } break;
                case OP_MOVINT: {
                    operand = *((long long*)instructionPointer); instructionPointer += 2;
                    registers[target].type = Register::Type::INT;
                    registers[target].i = operand;
                } break;
                case OP_MOVFLOAT: {
                    operand = *((long long*)instructionPointer); instructionPointer += 2;
                    registers[target].type = Register::Type::FLOAT;
                    registers[target].f = *(double*)&operand;
                } break;
                case OP_JMP:
                    operand = *((long long*)instructionPointer); instructionPointer += 2;
                    instructionPointer = reinterpret_cast<const unsigned int*>(&prog.code[operand + prog.codeOffset]);
                break;
                case OP_RESOLVE: {
                    operand = *((long long*)instructionPointer); instructionPointer += 2;
                    var = operand == 0 ? (void*)store : registers[operand].pointer;
                    Register& reg = registers[target];
                    bool success = false;
                    switch (reg.type) {
                        case Register::Type::INT:
                            success = resolver.getArrayVariable(LiquidRenderer { this }, var, reg.i, var);
                        break;
                        case Register::Type::SHORT_STRING:
                            success = resolver.getDictionaryVariable(LiquidRenderer { this }, var, reg.buffer, var);
                        break;
                        case Register::Type::FLOAT:
                        case Register::Type::LONG_STRING:
                        case Register::Type::EXTRA_LONG_STRING:
                        case Register::Type::VARIABLE:
                            assert(false);
                        break;
                    }
                    if (success) {
                        registers[target].type = Register::Type::VARIABLE;
                        switch (resolver.getType(LiquidRenderer { this }, var)) {
                            case LIQUID_VARIABLE_TYPE_INT:
                                reg.type = Register::Type::INT;
                                resolver.getInteger(LiquidRenderer { this }, var, &reg.i);
                            break;
                            case LIQUID_VARIABLE_TYPE_FLOAT:
                                reg.type = Register::Type::FLOAT;
                                resolver.getFloat(LiquidRenderer { this }, var, &reg.f);
                            break;
                            case LIQUID_VARIABLE_TYPE_STRING: {
                                reg.type = Register::Type::FLOAT;
                                long long length = resolver.getStringLength(LiquidRenderer { this }, var);
                                assert(length < SHORT_STRING_SIZE);
                                reg.length = (unsigned char)length;
                                resolver.getString(LiquidRenderer { this }, var, reg.buffer);
                                reg.buffer[length] = 0;
                            } break;
                            default:
                                registers[target].pointer = var;
                        }
                    } else {
                        registers[target].type = Register::Type::VARIABLE;
                        registers[target].pointer = nullptr;
                    }
                } break;
                case OP_ASSIGN:

                break;
                case OP_OUTPUT:
                    // This could potentially be made *way* more efficient.
                    switch (registers[target].type) {
                        case Register::Type::INT:
                            buffers[buffer].append(std::to_string(registers[target].i));
                        break;
                        case Register::Type::SHORT_STRING:
                            buffers[buffer].append(registers[target].buffer, registers[target].length);
                        break;
                        case Register::Type::FLOAT:
                            buffers[buffer].append(std::to_string(registers[target].f));
                        break;
                        case Register::Type::VARIABLE:
                        case Register::Type::LONG_STRING:
                        case Register::Type::EXTRA_LONG_STRING:
                        break;
                    }
                break;
                case OP_JMPFALSE: {
                    bool shouldJump = false;
                    switch (registers[target].type) {
                        case Register::Type::INT:
                            shouldJump = registers[target].i;
                        break;
                        case Register::Type::SHORT_STRING:
                            shouldJump = registers[target].length > 0;
                        break;
                        case Register::Type::FLOAT:
                            shouldJump = registers[target].f;
                        break;
                        case Register::Type::LONG_STRING:
                        case Register::Type::EXTRA_LONG_STRING:
                        case Register::Type::VARIABLE:
                        break;
                    }
                    if (!shouldJump) {
                        operand = (*instructionPointer++);
                        instructionPointer = reinterpret_cast<const unsigned int*>(&prog.code[operand + prog.codeOffset]);
                    }
                } break;
                case OP_EXIT:
                    finished = true;
                break;
            }
        }
        return buffers[0];
    }
}
