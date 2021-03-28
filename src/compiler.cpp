#include <cstring>
#include <cstdlib>

#include "compiler.h"
#include "context.h"

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
            case OP_INC:
            case OP_ADD:
            case OP_SUB:
            case OP_MUL:
            case OP_MOD:
            case OP_DIV:
            case OP_PUSH:
            case OP_MOVNIL:
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
            case OP_MOVBOOL:
                return "OP_MOVBOOL";
            case OP_MOVFLOAT:
                return "OP_MOVFLOAT";
            case OP_MOVNIL:
                return "OP_MOVNIL";
            case OP_STACK:
                return "OP_STACK";
            case OP_PUSH:
                return "OP_PUSH";
            case OP_POP:
                return "OP_POP";
            case OP_INC:
                return "OP_INC";
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
            case OP_OUTPUTMEM:
                return "OP_OUTPUTMEM";
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
            case OP_ITERATE:
                return "OP_ITERATE";
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
            // Align on a 4 byte boundary.
            data.resize(offset + size + ((offset + size) % 4));
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
        assert(!hasOperand(opcode));
        return offset;
    }

    int Compiler::addPush(int target) {
        int offset = add(OP_PUSH, target);
        stackSize++;
        return offset;
    }

    int Compiler::addPop(int amount) {
        int offset = add(OP_POP, 0x0, amount);
        stackSize -= amount;
        return offset;
    }

    int Compiler::add(OPCode opcode, int target, long long operand) {
        int offset = code.size();
        code.resize(offset + sizeof(int) + sizeof(long long));
        *((int*)&code[offset]) = (opcode & 0xFF) | ((target << 8) & 0xFFFF00);
        *((long long*)&code[offset+sizeof(int)]) = operand;
        assert(hasOperand(opcode));
        return offset;
    }

    void Compiler::modify(int offset, OPCode opcode, int target, long long operand) {
        *((int*)&code[offset]) = (opcode & 0xFF) | ((target << 8) & 0xFFFF00);
        *((long long*)&code[offset+sizeof(int)]) = operand;
        assert(hasOperand(opcode));
    }

    int Compiler::currentOffset() const { return code.size(); }

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
                case Variant::Type::NIL: {
                    add(OP_MOVNIL, freeRegister);
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
        stackSize = 0;
        data.clear();
        code.clear();
        existingStrings.clear();

        compileBranch(tmpl);

        add(OP_EXIT, 0x0);
        program.code.resize(code.size() + data.size());
        memcpy(&program.code[0], data.data(), data.size());
        program.codeOffset = data.size();
        memcpy(&program.code[program.codeOffset], code.data(), code.size());
        // Go and adjust the JMPs to JMP to the appropriate offsets now that we've shoved the data above.
        unsigned int i = program.codeOffset;
        while (i < program.code.size()) {
            OPCode instruction = (OPCode)((*(unsigned int*)&program.code[i]) & 0xFF);
            i += sizeof(unsigned int);
            switch (instruction) {
                case OP_JMP:
                case OP_JMPFALSE:
                case OP_ITERATE:
                    *((long long*)&program.code[i]) += program.codeOffset;
                break;
                default:
                break;
            }
            if (hasOperand(instruction))
                i += sizeof(long long);
        }
        return program;
    }

    string Compiler::disassemble(const Program& program) {
        size_t i = 0;
        string result;
        char buffer[128];
        while (i < program.codeOffset) {
            sprintf(buffer, "0x%08x", (int)i);
            int length = *((int*)&program.code[i]);
            result.append(buffer);
            result.append(" \"");
            result.append((const char*)&program.code[i+sizeof(int)], length);
            result.append("\"");
            result.append("\n");
            i += length + sizeof(int) + 1;
            i += i % 4;
        }
        while (i < program.code.size()) {
            unsigned int instruction = *(unsigned int*)&program.code[i];
            sprintf(buffer, "0x%08x %-12s REG%02d", (int)i, getSymbolicOpcode((OPCode)(program.code[i] & 0xFF)), instruction >> 8);
            result.append(buffer);
            i += sizeof(int);
            if (hasOperand((OPCode)(instruction & 0xFF))) {
                long long number = *(long long*)&program.code[i];
                sprintf(buffer, ", 0x%08x%08x", (unsigned int)(number >> 32), (unsigned int)(number & 0xFFFFFFFF));
                result.append(buffer);
                i += sizeof(long long);
            }
            result.append("\n");
        }
        return result;
    }

    Interpreter::Interpreter(const Context& context) : Renderer(context) {
        buffer.reserve(10*1024);
        stackPointer = stack;
    }
    Interpreter::Interpreter(const Context& context, LiquidVariableResolver resolver) : Renderer(context, resolver) {
        buffer.reserve(10*1024);
        stackPointer = stack;
    }
    Interpreter::~Interpreter() {

    }

    // -1 is top
    // -2 is below top,e tc..
    Node Interpreter::getStack(int idx) {
        char* localPointer = stackPointer;
        for (int i = -1; i >= idx; --i) {
            unsigned int type = *(unsigned int*)(localPointer - sizeof(unsigned int));
            switch ((Register::Type)(type & 0xFF)) {
                case Register::Type::INT:
                    localPointer -= sizeof(unsigned int) + sizeof(long long);
                    if (idx == i)
                        return Node(*(long long*)localPointer);
                break;
                case Register::Type::FLOAT:
                    localPointer -= sizeof(unsigned int) + sizeof(double);
                    if (idx == i)
                        return Node(*(double*)localPointer);
                break;
                case Register::Type::BOOL: {
                    unsigned int value = type >> 8;
                    localPointer -= sizeof(unsigned int);
                    if (idx == i)
                        return Node(value ? true : false);
                } break;
                case Register::Type::NIL: {
                    localPointer -= sizeof(unsigned int);
                    if (idx == i)
                        return Node(nullptr);
                } break;
                case Register::Type::SHORT_STRING:
                case Register::Type::LONG_STRING: {
                    unsigned int len = type >> 8;
                    localPointer -= sizeof(unsigned int) + len + (len % 4);
                    if (idx == i)
                        return Node(string(localPointer, len));
                } break;
                case Register::Type::EXTRA_LONG_STRING:
                case Register::Type::VARIABLE:
                    assert(false);
                break;
            }
        }
        assert(false);
        return Node();
    }


    void Interpreter::getStack(Register& reg, int idx) {
        char* localPointer = stackPointer;
        for (int i = -1; i >= idx; --i) {
            unsigned int type = *(unsigned int*)(localPointer - sizeof(unsigned int));
            Register::Type regType = (Register::Type)(type & 0xFF);
            switch (regType) {
                case Register::Type::INT:
                    localPointer -= sizeof(unsigned int) + sizeof(long long);
                    if (idx == i) {
                        reg.type = regType;
                        reg.i = *(long long*)localPointer;
                        return;
                    }
                break;
                case Register::Type::FLOAT:
                    localPointer -= sizeof(unsigned int) + sizeof(double);
                    if (idx == i) {
                        reg.type = regType;
                        reg.f = *(double*)localPointer;
                        return;
                    }
                break;
                case Register::Type::BOOL: {
                    unsigned int value = type >> 8;
                    localPointer -= sizeof(unsigned int);
                    if (idx == i) {
                        reg.type = regType;
                        reg.b = value ? true : false;
                        return;
                    }
                } break;
                case Register::Type::NIL: {
                    localPointer -= sizeof(unsigned int);
                    if (idx == i) {
                        reg.type = regType;
                        return;
                    }
                } break;
                case Register::Type::SHORT_STRING: {
                    unsigned int len = type >> 8;
                    localPointer -= sizeof(unsigned int) + len + (len % 4);
                    if (idx == i) {
                        reg.type = regType;
                        strncpy(reg.buffer, localPointer, SHORT_STRING_SIZE);
                        return;
                    }
                } break;
                case Register::Type::VARIABLE:
                    localPointer -= sizeof(unsigned int) + sizeof(void*);
                    if (idx == i) {
                        reg.type = regType;
                        reg.pointer = *(void**)localPointer;
                        return;
                    }
                break;
                case Register::Type::LONG_STRING:
                case Register::Type::EXTRA_LONG_STRING:
                    assert(false);
                break;
            }
        }
        assert(false);
    }

    void Interpreter::pushStack(Register& reg) {
        switch (reg.type) {
            case Register::Type::INT:
                *((long long*)stackPointer) = reg.i;
                stackPointer += sizeof(long long);
                *((unsigned int*)stackPointer) = (unsigned int)Register::Type::INT;
                stackPointer += sizeof(unsigned int);
            break;
            case Register::Type::BOOL:
                *((unsigned int*)stackPointer) = (reg.b << 8) | (unsigned int)Register::Type::BOOL;
                stackPointer += sizeof(unsigned int);
            break;
            case Register::Type::NIL:
                *((unsigned int*)stackPointer) = (unsigned int)Register::Type::NIL;
                stackPointer += sizeof(unsigned int);
            break;
            case Register::Type::SHORT_STRING:
                memcpy(stackPointer, reg.buffer, reg.length);
                stackPointer += reg.length + (reg.length % 4);
                *((unsigned int*)stackPointer) = ((unsigned int)Register::Type::SHORT_STRING | (reg.length << 8));
                stackPointer += sizeof(unsigned int);
            break;
            case Register::Type::FLOAT:
                *((double*)stackPointer) = reg.f;
                stackPointer += sizeof(double);
                *((unsigned int*)stackPointer) = (unsigned int)Register::Type::FLOAT;
                stackPointer += sizeof(unsigned int);
            break;
            case Register::Type::VARIABLE:
                *((void**)stackPointer) = reg.pointer;
                stackPointer += sizeof(void*);
                *((unsigned int*)stackPointer) = (unsigned int)Register::Type::VARIABLE;
                stackPointer += sizeof(unsigned int);
            break;
            case Register::Type::LONG_STRING:
            case Register::Type::EXTRA_LONG_STRING:
                assert(false);
            break;
        }
    }

    void Interpreter::popStack(int popCount) {
        assert(stackPointer > stack);
        for (int i = 0; i < popCount; ++i) {
            unsigned int type = *(unsigned int*)(stackPointer - sizeof(unsigned int));
            switch ((Register::Type)(type & 0xFF)) {
                case Register::Type::INT:
                    stackPointer -= sizeof(unsigned int) + sizeof(long long);
                break;
                case Register::Type::NIL:
                case Register::Type::BOOL:
                    stackPointer -= sizeof(unsigned int);
                break;
                case Register::Type::FLOAT:
                    stackPointer -= sizeof(unsigned int) + sizeof(double);
                break;
                case Register::Type::SHORT_STRING:
                case Register::Type::LONG_STRING: {
                    unsigned int len = type >> 8;
                    stackPointer -= sizeof(unsigned int) + len + (len % 4);
                } break;
                case Register::Type::VARIABLE:
                    stackPointer -= sizeof(unsigned int) + sizeof(void*);
                break;
                case Register::Type::EXTRA_LONG_STRING:
                    assert(false);
                break;
            }
        }
    }

    void Interpreter::pushRegister(Register& reg, const Node& node) {
        assert(!node.type);
        switch (node.variant.type) {
            case Variant::Type::INT:
                reg.type = Register::Type::INT;
                reg.i = node.variant.i;
            break;
            case Variant::Type::FLOAT:
                reg.type = Register::Type::FLOAT;
                reg.f = node.variant.f;
            break;
            case Variant::Type::STRING:
                reg.type = Register::Type::SHORT_STRING;
                assert(node.variant.s.size() < SHORT_STRING_SIZE);
                memcpy(reg.buffer, node.variant.s.data(), node.variant.s.size());
                reg.buffer[node.variant.s.size()] = 0;
            break;
            case Variant::Type::STRING_VIEW:
                assert(false);
            break;
            case Variant::Type::BOOL:
                reg.type = Register::Type::BOOL;
                reg.b = node.variant.b;
            break;
            case Variant::Type::NIL:
            case Variant::Type::ARRAY:
            case Variant::Type::POINTER:
            case Variant::Type::VARIABLE:
                assert(false);
            break;
        }
    }

    string Interpreter::renderTemplate(const Program& prog, Variable store) {
        buffer.clear();
        renderTemplate(prog, store, +[](const char* chunk, size_t len, void* data) {
            static_cast<Interpreter*>(data)->buffer.append(chunk, len);
        }, this);
        return buffer;
    }

    char* itoa(int value, char* result) {
        char* ptr = result, *ptr1 = result, tmp_char;
        int tmp_value;

        do {
            tmp_value = value;
            value /= 10;
            *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * 10)];
        } while ( value );

        // Apply negative sign
        if (tmp_value < 0) *ptr++ = '-';
        *ptr-- = '\0';
        while(ptr1 < ptr) {
            tmp_char = *ptr;
            *ptr--= *ptr1;
            *ptr1++ = tmp_char;
        }
        return result;
    }

    bool Interpreter::run(const unsigned char* code, Variable store, void (*callback)(const char* chunk, size_t len, void* data), void* data, const unsigned char* iteration) {
        unsigned int instruction, target;
        long long operand;
        Node node;
        while (true) {
            instruction = *instructionPointer++;
            target = instruction >> 8;
            switch (instruction & 0xFF) {
                case OP_MOVSTR: {
                    operand = *((long long*)instructionPointer); instructionPointer += 2;
                    unsigned int length = *(unsigned int*)&code[operand];
                    assert(length < SHORT_STRING_SIZE);
                    registers[target].type = Register::Type::SHORT_STRING;
                    registers[target].length = length;
                    memcpy(registers[target].buffer, &code[operand+sizeof(unsigned int)], length);
                    registers[target].buffer[length] = 0;
                } break;
                case OP_MOV: {
                    operand = *((long long*)instructionPointer); instructionPointer += 2;
                    registers[operand] = registers[target];
                } break;
                case OP_MOVBOOL: {
                    operand = *((long long*)instructionPointer); instructionPointer += 2;
                    registers[target].type = Register::Type::BOOL;
                    registers[target].b = operand ? true : false;
                } break;
                case OP_MOVINT: {
                    operand = *((long long*)instructionPointer); instructionPointer += 2;
                    registers[target].type = Register::Type::INT;
                    registers[target].i = operand;
                } break;
                case OP_MOVFLOAT: {
                    operand = *((double*)instructionPointer); instructionPointer += 2;
                    registers[target].type = Register::Type::FLOAT;
                    registers[target].f = operand;
                } break;
                case OP_MOVNIL: {
                    registers[target].type = Register::Type::NIL;
                } break;
                case OP_INC: {
                    if (registers[target].type == Register::Type::INT)
                        ++registers[target].i;
                } break;
                case OP_SUB: {
                    assert(registers[target].type == Register::Type::INT);
                    assert(registers[0].type == Register::Type::INT);
                    registers[0].i -= registers[target].i;
                } break;
                case OP_STACK: {
                    operand = *((long long*)instructionPointer); instructionPointer += 2;
                    getStack(registers[target], operand);
                } break;
                case OP_PUSH: {
                    pushStack(registers[target]);
                } break;
                case OP_POP: {
                    operand = *((long long*)instructionPointer); instructionPointer += 2;
                    popStack(operand);
                } break;
                case OP_JMP:
                    operand = *((long long*)instructionPointer); instructionPointer += 2;
                    instructionPointer = reinterpret_cast<const unsigned int*>(&code[operand]);
                break;
                case OP_CALL: {
                    operand = *((long long*)instructionPointer); instructionPointer += 2;
                    unsigned int argCount = *(unsigned int*)(stackPointer - sizeof(unsigned int) * 3);
                    pushRegister(registers[0], ((NodeType*)operand)->render(*this, node, store));
                    popStack(argCount+1);
                } break;
                case OP_RESOLVE: {
                    operand = *((long long*)instructionPointer); instructionPointer += 2;
                    Variable var = operand == 0 ? (void*)store : registers[operand].pointer;
                    Register& reg = registers[target];
                    bool success = false;
                    switch (reg.type) {
                        case Register::Type::INT:
                            success = variableResolver.getArrayVariable(LiquidRenderer { this }, var, reg.i, var);
                        break;
                        case Register::Type::SHORT_STRING:
                            success = variableResolver.getDictionaryVariable(LiquidRenderer { this }, var, reg.buffer, var);
                        break;
                        case Register::Type::NIL:
                        case Register::Type::BOOL:
                        case Register::Type::FLOAT:
                        case Register::Type::LONG_STRING:
                        case Register::Type::EXTRA_LONG_STRING:
                        case Register::Type::VARIABLE:
                            assert(false);
                        break;
                    }
                    if (success) {
                        registers[target].type = Register::Type::VARIABLE;
                        switch (variableResolver.getType(LiquidRenderer { this }, var)) {
                            case LIQUID_VARIABLE_TYPE_INT:
                                reg.type = Register::Type::INT;
                                variableResolver.getInteger(LiquidRenderer { this }, var, &reg.i);
                            break;
                            case LIQUID_VARIABLE_TYPE_BOOL:
                                reg.type = Register::Type::BOOL;
                                variableResolver.getBool(LiquidRenderer { this }, var, &reg.b);
                            break;
                            case LIQUID_VARIABLE_TYPE_FLOAT:
                                reg.type = Register::Type::FLOAT;
                                variableResolver.getFloat(LiquidRenderer { this }, var, &reg.f);
                            break;
                            case LIQUID_VARIABLE_TYPE_NIL:
                                reg.type = Register::Type::NIL;
                            break;
                            case LIQUID_VARIABLE_TYPE_STRING: {
                                reg.type = Register::Type::FLOAT;
                                long long length = variableResolver.getStringLength(LiquidRenderer { this }, var);
                                assert(length < SHORT_STRING_SIZE);
                                reg.length = (unsigned char)length;
                                variableResolver.getString(LiquidRenderer { this }, var, reg.buffer);
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
                case OP_ITERATE: {
                    if (iteration == (const unsigned char*)instructionPointer) {
                        instructionPointer += 2;
                        return true;
                    }
                    // TODO: This is a bit of a hack. Probably should allow for an iterator context which will allow us to call this non-recursively.
                    operand = *((long long*)instructionPointer);
                    Register& reg = registers[target];
                    void* pointers[] = { this, const_cast<unsigned char*>(code), store.pointer, (void*)callback, data, const_cast<unsigned int*>(instructionPointer) };
                    instructionPointer += 2;
                    Variable var = reg.pointer == nullptr ? (void*)store : reg.pointer;
                    variableResolver.iterate(LiquidRenderer { this }, var, +[](void* variable, void* data){
                        void** pointers = (void**)data;
                        Interpreter* interpreter = (Interpreter*)pointers[0];
                        interpreter->registers[0].type = Register::Type::VARIABLE;
                        interpreter->registers[0].pointer = variable;
                        interpreter->run((const unsigned char*)pointers[1], Variable { pointers[2] }, (void (*)(const char*, size_t, void*))pointers[3], pointers[4], (const unsigned char*)pointers[5]);
                        return true;
                    }, pointers, 0, -1, false);
                    instructionPointer = reinterpret_cast<const unsigned int*>(&code[operand]);
                } break;
                case OP_OUTPUTMEM: {
                    operand = *((long long*)instructionPointer); instructionPointer += 2;
                    unsigned int len = *(unsigned int*)&code[operand];
                    callback((const char*)&code[operand+sizeof(unsigned int)], len, data);
                } break;
                case OP_OUTPUT:
                    // This could potentially be made *way* more efficient.
                    switch (registers[target].type) {
                        case Register::Type::INT: {
                            char buffer[32];
                            itoa(registers[target].i, buffer);
                            callback(buffer, strlen(buffer), data);
                        } break;
                        case Register::Type::BOOL:
                            if (registers[target].b)
                                callback("true", 4, data);
                            else
                                callback("false", 5, data);
                        break;
                        case Register::Type::SHORT_STRING:
                            callback(registers[target].buffer, registers[target].length, data);
                        break;
                        case Register::Type::FLOAT: {
                            char buffer[32];
                            size_t len = sprintf(buffer, "%g", registers[target].f);
                            callback(buffer, len, data);
                        } break;
                        case Register::Type::VARIABLE: {
                            long long len = variableResolver.getStringLength(LiquidRenderer { this }, registers[target].pointer);
                            if (len > 0) {
                                if (len < 4096) {
                                    char buffer[4096];
                                    variableResolver.getString(LiquidRenderer { this }, registers[target].pointer, buffer);
                                    callback(buffer, len, data);
                                } else {
                                    vector<char> buffer;
                                    buffer.resize(len);
                                    variableResolver.getString(LiquidRenderer { this }, registers[target].pointer, &buffer[0]);
                                    callback(buffer.data(), len, data);
                                }
                            }
                        } break;
                        case Register::Type::NIL: break;
                        case Register::Type::LONG_STRING:
                        case Register::Type::EXTRA_LONG_STRING:
                            assert(false);
                        break;
                    }
                break;
                case OP_JMPFALSE: {
                    bool shouldJump = false;
                    operand = *((long long*)instructionPointer); instructionPointer += 2;
                    switch (registers[target].type) {
                        case Register::Type::INT:
                            shouldJump = registers[target].i;
                        break;
                        case Register::Type::BOOL:
                            shouldJump = registers[target].b;
                        break;
                        case Register::Type::SHORT_STRING:
                            shouldJump = registers[target].length > 0;
                        break;
                        case Register::Type::NIL:
                            shouldJump = false;
                        break;
                        case Register::Type::FLOAT:
                            shouldJump = registers[target].f;
                        break;
                        case Register::Type::LONG_STRING:
                        case Register::Type::EXTRA_LONG_STRING:
                        case Register::Type::VARIABLE:
                        break;
                    }
                    if (!shouldJump)
                        instructionPointer = reinterpret_cast<const unsigned int*>(&code[operand]);
                } break;
                case OP_EXIT:
                    return false;
                default:
                    assert(false);
                break;
            }
        }
    }

    void Interpreter::renderTemplate(const Program& prog, Variable store, void (*callback)(const char* chunk, size_t len, void* data), void* data) {
        mode = Renderer::ExecutionMode::INTERPRETER;
        instructionPointer = reinterpret_cast<const unsigned int*>(&prog.code[prog.codeOffset]);
        stackPointer = stack;
        run(prog.code.data(), store, callback, data);
    }


    void OperatorNodeType::compile(Compiler& compiler, const Node& node) const {
        int freeRegister = compiler.freeRegister;
        for (auto it = node.children.rbegin(); it != node.children.rend(); ++it) {
            compiler.compileBranch(**it);
            compiler.add(OP_PUSH, freeRegister);
            compiler.freeRegister = freeRegister;
        }
        compiler.add(OP_MOVINT, compiler.freeRegister, node.children.size());
        compiler.add(OP_PUSH, compiler.freeRegister);
        compiler.add(OP_CALL, 0x0, (long long)this);
        compiler.freeRegister = 1;
    }

    void Context::ConcatenationNode::compile(Compiler& compiler, const Node& node) const {
        for (auto& child : node.children) {
            if (child->type) {
                compiler.compileBranch(*child.get());
            } else {
                assert(child->variant.type == Variant::Type::STRING);
                int offset = compiler.add(child->variant.s.data(), child->variant.s.size());
                compiler.add(OP_OUTPUTMEM, 0x0, offset);
            }
        }
    }

    void Context::ArgumentNode::compile(Compiler& compiler, const Node& node) const {
        for (auto& child : node.children) {
            compiler.compileBranch(*child.get());
            compiler.add(OP_PUSH, compiler.freeRegister - 1);
        }
    }


    void Context::OutputNode::compile(Compiler& compiler, const Node& node) const {
        assert(node.children.size() == 1);
        compiler.compileBranch(*node.children[0].get()->children[0].get());
        compiler.add(OP_OUTPUT, 0x0);
        compiler.freeRegister = 0;
    }


    void Context::VariableNode::compile(Compiler& compiler, const Node& node) const {
        int target = 0x0;

        string var;
        for (size_t i = 0; i < node.children.size(); ++i) {
            if (!var.empty())
                var.append(".");
            if (node.children[i]->type || node.children[i]->variant.type != Variant::Type::STRING)
                break;
            var.append(node.children[i]->variant.s);
        }
        auto it = compiler.dropFrames.find(var);
        if (it != compiler.dropFrames.end() && it->second.size() > 0) {
            it->second.back().first(compiler, it->second.back().second);
        } else {
            for (size_t i = 0; i < node.children.size(); ++i) {
                compiler.freeRegister = 0;
                compiler.compileBranch(*node.children[i].get());
                compiler.add(OP_RESOLVE, 0x0, target);
                if (i < node.children.size() - 1) {
                    compiler.add(OP_MOV, 0x0, 0x1);
                    target = 0x1;
                }
            }
        }
    }

    void FilterNodeType::compile(Compiler& compiler, const Node& node) const {
        if (!userCompileFunction) {
            for (size_t i = 0; i < node.children.size();  ++i) {
                compiler.compileBranch(*node.children[i].get());
                if (i == 0)
                    compiler.add(OP_PUSH, compiler.freeRegister - 1);
            }
            compiler.add(OP_MOVINT, compiler.freeRegister, node.children.size());
            compiler.add(OP_PUSH, compiler.freeRegister);
            compiler.add(OP_CALL, 0x0, (long long)this);
            compiler.freeRegister = 1;
        } else
            userCompileFunction(LiquidCompiler{&compiler}, LiquidNode{const_cast<Node*>(&node)}, userData);
    }

    void Context::PassthruNode::compile(Compiler& compiler, const Node& node) const {
        if (!userCompileFunction) {
            for (auto& child : node.children)
                compiler.compileBranch(*child.get());
        } else
            userCompileFunction(LiquidCompiler{&compiler}, LiquidNode{const_cast<Node*>(&node)}, userData);
    }

    void NodeType::compile(Compiler& compiler, const Node& node) const {
        if (!userCompileFunction) {
            for (auto& child : node.children)
                compiler.compileBranch(*child.get());
            compiler.add(OP_CALL, 0x0, (long long)this);
            compiler.freeRegister = 1;
        } else
            userCompileFunction(LiquidCompiler{&compiler}, LiquidNode{const_cast<Node*>(&node)}, userData);
    }
}
