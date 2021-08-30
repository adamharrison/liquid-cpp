#include <cstring>
#include <cstdlib>

#include "compiler.h"
#include "context.h"

namespace Liquid {

    // Instructions are
    // 1 byte op-code.
    // 3 byte for register designation.
    // 8 byte for operand.
    // 4/12 bytes per instruction total.

    // This should probably be changed out, but am super lazy at present.
    size_t hash(const char* s, int len) {
        size_t h = 5381;
        int c;
        const char* end = s + len;
        while (((c = *s++) && s <= end))
            h = ((h << 5) + h) + c;
        return h;
    }

    size_t operandSize(OPCode opcode) {
        switch (opcode) {
            case OP_EXIT:
            case OP_OUTPUT:
            case OP_ADD:
            case OP_SUB:
            case OP_PUSH:
            case OP_MOVNIL:
            case OP_INVERT:
            case OP_EQL:
            case OP_PUSHBUFFER:
            case OP_POPBUFFER:
                return 0;
            default:
                return sizeof(void*);
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
            case OP_ADD:
                return "OP_ADD";
            case OP_SUB:
                return "OP_SUB";
            case OP_LENGTH:
                return "OP_LENGTH";
            case OP_EQL:
                return "OP_EQL";
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
            case OP_JMPTRUE:
                return "OP_JMPTRUE";
            case OP_CALL:
                return "OP_CALL";
            case OP_RESOLVE:
                return "OP_RESOLVE";
            case OP_ITERATE:
                return "OP_ITERATE";
            case OP_INVERT:
                return "OP_INVERT";
            case OP_PUSHBUFFER:
                return "OP_PUSHBUFFER";
            case OP_POPBUFFER:
                return "OP_POPBUFFER";
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
        assert(!operandSize(opcode));
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
        assert(operandSize(opcode));
        return offset;
    }

    void Compiler::modify(int offset, OPCode opcode, int target, long long operand) {
        *((int*)&code[offset]) = (opcode & 0xFF) | ((target << 8) & 0xFFFF00);
        *((long long*)&code[offset+sizeof(int)]) = operand;
        assert(operandSize(opcode));
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
                case OP_JMPTRUE:
                case OP_ITERATE:
                    *((long long*)&program.code[i]) += program.codeOffset;
                break;
                default:
                break;
            }
            if (operandSize(instruction))
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
            sprintf(buffer, "0x%08x %-14s REG%02d", (int)i, getSymbolicOpcode((OPCode)(program.code[i] & 0xFF)), instruction >> 8);
            result.append(buffer);
            i += sizeof(int);
            if (operandSize((OPCode)(instruction & 0xFF))) {
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
        stackPointer = stackBlock;
    }
    Interpreter::Interpreter(const Context& context, LiquidVariableResolver resolver) : Renderer(context, resolver) {
        stackPointer = stackBlock;
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
                case Register::Type::VARIABLE: {
                    localPointer -= sizeof(unsigned int) + sizeof(void*);
                    if (idx == i)
                        return Node(Variable(localPointer));
                } break;
                case Register::Type::EXTRA_LONG_STRING:
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
        assert(stackPointer > stackBlock);
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
            case Variant::Type::NIL:
                reg.type = Register::Type::NIL;
            break;
            case Variant::Type::STRING:
                pushRegister(reg, node.variant.s);
            break;
            case Variant::Type::STRING_VIEW:
                assert(false);
            break;
            case Variant::Type::BOOL:
                reg.type = Register::Type::BOOL;
                reg.b = node.variant.b;
            break;
            case Variant::Type::ARRAY:
            case Variant::Type::POINTER:
            case Variant::Type::VARIABLE:
                assert(false);
            break;
        }
    }

    void Interpreter::pushRegister(Register& reg, const string& str) {
        reg.type = Register::Type::SHORT_STRING;
        assert(str.size() < SHORT_STRING_SIZE);
        memcpy(reg.buffer, str.data(), str.size());
        reg.buffer[str.size()] = 0;
        reg.length = str.size();
    }

    void Interpreter::pushRegister(Register& reg, string&& str) {
        reg.type = Register::Type::SHORT_STRING;
        assert(str.size() < SHORT_STRING_SIZE);
        memcpy(reg.buffer, str.data(), str.size());
        reg.buffer[str.size()] = 0;
        reg.length = str.size();
    }

    string Interpreter::renderTemplate(const Program& prog, Variable store) {
        string result;
        renderTemplate(prog, store, +[](const char* chunk, size_t len, void* data) {
            static_cast<string*>(data)->append(chunk, len);
        }, &result);
        return result;
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
            OPCode opCode = (OPCode)(instruction & 0xFF);
            switch (opCode) {
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
                    registers[target].pointer = nullptr;
                } break;
                case OP_EQL: {
                    bool isEqual = false;
                    if (registers[target].type == registers[0].type) {
                        switch (registers[0].type) {
                            case Register::Type::INT:
                                isEqual = registers[target].i == registers[0].i;
                            break;
                            case Register::Type::SHORT_STRING:
                                isEqual = registers[target].length == registers[0].length && strncmp(registers[target].buffer, registers[0].buffer, registers[0].length) == 0;
                            break;
                            case Register::Type::NIL:
                                isEqual = true;
                            break;
                            case Register::Type::BOOL:
                                isEqual = registers[target].b == registers[0].b;
                            break;
                            case Register::Type::FLOAT:
                                isEqual = registers[target].f == registers[0].f;
                            break;
                            case Register::Type::VARIABLE:
                                isEqual = registers[target].pointer == registers[0].pointer;
                            break;
                            case Register::Type::LONG_STRING:
                            case Register::Type::EXTRA_LONG_STRING:
                                assert(false);
                            break;
                        }
                    }
                    registers[0].type = Register::Type::BOOL;
                    registers[0].b = isEqual;
                } break;
                case OP_ADD: {
                    if (registers[0].type == Register::Type::INT && registers[target].type == Register::Type::INT)
                        registers[0].i += registers[target].i;
                } break;
                case OP_SUB: {
                    if (registers[0].type == Register::Type::INT && registers[target].type == Register::Type::INT)
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
                    unsigned int argCount = (unsigned int)registers[target].i;
                    pushRegister(registers[0], ((NodeType*)operand)->render(*this, node, store));
                    popStack(argCount);
                } break;
                case OP_RESOLVE: {
                    operand = *((long long*)instructionPointer); instructionPointer += 2;
                    Variable var = operand == -1 || registers[operand].pointer == nullptr ? (void*)store : registers[operand].pointer;
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
                                reg.type = Register::Type::SHORT_STRING;
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
                case OP_ASSIGN: {
                    operand = *((long long*)instructionPointer); instructionPointer += 2;
                    Variable hash = registers[target].pointer ? registers[target].pointer : store.pointer;
                    Variable varvalue;
                    Register& value = registers[operand];
                    Register& key = registers[0];
                    switch (value.type) {
                        case Register::Type::INT:
                            inject(varvalue, value.i);
                        break;
                        case Register::Type::SHORT_STRING:
                            inject(varvalue, string(value.buffer, value.length));
                        break;
                        case Register::Type::NIL:
                            inject(varvalue, nullptr);
                        break;
                        case Register::Type::BOOL:
                            inject(varvalue, value.b);
                        break;
                        case Register::Type::FLOAT:
                            inject(varvalue, value.f);
                        break;
                        case Register::Type::VARIABLE:
                            varvalue = value.pointer;
                        break;
                        case Register::Type::LONG_STRING:
                        case Register::Type::EXTRA_LONG_STRING:
                            assert(false);
                        break;
                    }
                    switch (key.type) {
                        case Register::Type::INT:
                            variableResolver.setArrayVariable(*this, hash, key.i, varvalue);
                        break;
                        case Register::Type::SHORT_STRING:
                            variableResolver.setDictionaryVariable(*this, hash, key.buffer, varvalue);
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
                } break;
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
                    if (buffers.size()) {
                        buffers.top().append((const char*)&code[operand+sizeof(unsigned int)], len);
                    } else
                        callback((const char*)&code[operand+sizeof(unsigned int)], len, data);
                } break;
                case OP_INVERT: {
                    bool isTrue = false;
                    switch (registers[target].type) {
                        case Register::Type::INT:
                            isTrue = registers[target].i;
                        break;
                        case Register::Type::BOOL:
                            isTrue = registers[target].b;
                        break;
                        case Register::Type::SHORT_STRING:
                            isTrue = registers[target].length > 0;
                        break;
                        case Register::Type::NIL:
                            isTrue = false;
                        break;
                        case Register::Type::FLOAT:
                            isTrue = registers[target].f;
                        break;
                        case Register::Type::LONG_STRING:
                        case Register::Type::EXTRA_LONG_STRING:
                        case Register::Type::VARIABLE:
                        break;
                    }
                    registers[target].type = Register::Type::BOOL;
                    registers[target].b = !isTrue;
                } break;
                case OP_OUTPUT: {
                    // This could potentially be made *way* more efficient.
                    auto output = [this, data, callback](const char* str, size_t len){
                        if (buffers.size())
                            buffers.top().append(str, len);
                        else
                            callback(str, len, data);
                    };
                    switch (registers[target].type) {
                        case Register::Type::INT: {
                            char buffer[32];
                            itoa(registers[target].i, buffer);
                            output(buffer, strlen(buffer));
                        } break;
                        case Register::Type::BOOL:
                            if (registers[target].b)
                                output("true", 4);
                            else
                                output("false", 5);
                        break;
                        case Register::Type::SHORT_STRING:
                            output(registers[target].buffer, registers[target].length);
                        break;
                        case Register::Type::FLOAT: {
                            char buffer[32];
                            size_t len = sprintf(buffer, "%g", registers[target].f);
                            output(buffer, len);
                        } break;
                        case Register::Type::VARIABLE: {
                            if (registers[target].pointer) {
                                long long len = variableResolver.getStringLength(LiquidRenderer { this }, registers[target].pointer);
                                if (len > 0) {
                                    if (len < 4096) {
                                        char buffer[4096];
                                        variableResolver.getString(LiquidRenderer { this }, registers[target].pointer, buffer);
                                        output(buffer, len);
                                    } else {
                                        vector<char> buffer;
                                        buffer.resize(len);
                                        variableResolver.getString(LiquidRenderer { this }, registers[target].pointer, &buffer[0]);
                                        output(buffer.data(), len);
                                    }
                                }
                            }
                        } break;
                        case Register::Type::NIL: break;
                        case Register::Type::LONG_STRING:
                        case Register::Type::EXTRA_LONG_STRING:
                            assert(false);
                        break;
                    }
                } break;
                case OP_JMPTRUE: {
                case OP_JMPFALSE:
                    bool isTrue = true;
                    operand = *((long long*)instructionPointer); instructionPointer += 2;
                    switch (registers[target].type) {
                        case Register::Type::INT:
                            isTrue = registers[target].i;
                        break;
                        case Register::Type::BOOL:
                            isTrue = registers[target].b;
                        break;
                        case Register::Type::SHORT_STRING:
                            isTrue = registers[target].length > 0;
                        break;
                        case Register::Type::NIL:
                            isTrue = false;
                        break;
                        case Register::Type::FLOAT:
                            isTrue = registers[target].f;
                        break;
                        case Register::Type::VARIABLE:
                            isTrue = registers[target].pointer;
                        break;
                        case Register::Type::LONG_STRING:
                        case Register::Type::EXTRA_LONG_STRING:
                        break;
                    }
                    if (opCode == OP_JMPFALSE)
                        isTrue = !isTrue;
                    if (isTrue)
                        instructionPointer = reinterpret_cast<const unsigned int*>(&code[operand]);
                } break;
                case OP_PUSHBUFFER: {
                    buffers.push(string());
                } break;
                case OP_POPBUFFER: {
                    pushRegister(registers[target], move(buffers.top()));
                    buffers.pop();
                } break;
                case OP_EXIT:
                    assert(stackPointer == stackBlock);
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
        stackPointer = stackBlock;
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
        compiler.add(OP_CALL, compiler.freeRegister, (long long)this);
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
        compiler.freeRegister = 0;
        compiler.compileBranch(*node.children[0].get()->children[0].get());
        compiler.add(OP_OUTPUT, 0x0);
        compiler.freeRegister = 0;
    }


    void Context::VariableNode::compile(Compiler& compiler, const Node& node) const {
        long long target = -1;

        if (node.children.size() > 0 && node.children[0]->variant.type == Variant::Type::STRING) {
            auto it = compiler.dropFrames.find(node.children[0]->variant.s);
            if (it != compiler.dropFrames.end() && it->second.size() > 0) {
                it->second.back().first(compiler, it->second.back().second, node);
                return;
            }
        }
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

    void FilterNodeType::compile(Compiler& compiler, const Node& node) const {
        if (!userCompileFunction) {
            for (int i = node.children.size() - 1; i >= 0; --i) {
                compiler.compileBranch(*node.children[i].get());
                if (i == 0)
                    compiler.add(OP_PUSH, compiler.freeRegister - 1);
            }
            compiler.add(OP_MOVINT, compiler.freeRegister, node.children.size());
            compiler.add(OP_CALL, compiler.freeRegister, (long long)this);
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
