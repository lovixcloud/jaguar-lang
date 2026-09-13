/* ============================================================================
 * Jaguar Compiler - Bytecode VM Backend Implementation
 * ========================================================================== */

#include "../include/jaguar.h"
#include <math.h>

typedef struct {
    Compiler *comp;
    Bytecode *bc;
} CodeGen;

Bytecode *bytecode_create(void) {
    Bytecode *bc = (Bytecode *)calloc(1, sizeof(Bytecode));
    bc->code_capacity = 4096;
    bc->code = (uint8_t *)calloc(bc->code_capacity, sizeof(uint8_t));
    bc->code_size = 0;
    bc->const_capacity = 256;
    bc->constants_num = (long *)calloc(bc->const_capacity, sizeof(long));
    bc->constants_dec = (double *)calloc(bc->const_capacity, sizeof(double));
    bc->constants_str = (char **)calloc(bc->const_capacity, sizeof(char *));
    bc->const_num_count = 0;
    bc->const_dec_count = 0;
    bc->const_str_count = 0;
    return bc;
}

void bytecode_free(Bytecode *bc) {
    if (!bc) return;
    free(bc->code);
    for (int i = 0; i < bc->const_str_count; i++) {
        free(bc->constants_str[i]);
    }
    free(bc->constants_num);
    free(bc->constants_dec);
    free(bc->constants_str);
    free(bc);
}

static void emit_byte(Bytecode *bc, uint8_t byte) {
    if (bc->code_size >= bc->code_capacity) {
        bc->code_capacity *= 2;
        bc->code = (uint8_t *)realloc(bc->code, bc->code_capacity);
    }
    bc->code[bc->code_size++] = byte;
}

static int emit_op(Bytecode *bc, OpCode op) {
    int offset = bc->code_size;
    emit_byte(bc, (uint8_t)op);
    return offset;
}

static void emit_int(Bytecode *bc, int value) {
    emit_byte(bc, (value >> 24) & 0xFF);
    emit_byte(bc, (value >> 16) & 0xFF);
    emit_byte(bc, (value >> 8) & 0xFF);
    emit_byte(bc, value & 0xFF);
}

static int add_constant_str(Bytecode *bc, const char *value) {
    if (bc->const_str_count >= bc->const_capacity) {
        bc->const_capacity *= 2;
        bc->constants_str = (char **)realloc(bc->constants_str,
                                             bc->const_capacity * sizeof(char *));
    }
    bc->constants_str[bc->const_str_count] = str_dup(value);
    return bc->const_str_count++;
}

VM *vm_create(void) {
    VM *vm = (VM *)calloc(1, sizeof(VM));
    vm->stack = (VMStack *)calloc(1, sizeof(VMStack));
    vm->stack->stack_num = (long *)calloc(JAG_STACK_SIZE, sizeof(long));
    vm->stack->stack_dec = (double *)calloc(JAG_STACK_SIZE, sizeof(double));
    vm->stack->stack_str = (char **)calloc(JAG_STACK_SIZE, sizeof(char *));
    vm->stack->defined_flags = (bool *)calloc(JAG_STACK_SIZE, sizeof(bool));
    vm->stack->sp_num = 0;
    vm->stack->sp_dec = 0;
    vm->stack->sp_str = 0;
    vm->running = false;
    vm->live_mode = false;
    return vm;
}

void vm_free(VM *vm) {
    if (!vm) return;
    for (int i = 0; i < vm->stack->sp_str; i++) {
        free(vm->stack->stack_str[i]);
    }
    free(vm->stack->stack_num);
    free(vm->stack->stack_dec);
    free(vm->stack->stack_str);
    free(vm->stack->defined_flags);
    free(vm->stack);
    free(vm);
}

#define PUSH_NUM(v) do { vm->stack->stack_num[vm->stack->sp_num++] = (v); } while(0)
#define POP_NUM() (vm->stack->stack_num[--vm->stack->sp_num])
#define PUSH_DEC(v) do { vm->stack->stack_dec[vm->stack->sp_dec++] = (v); } while(0)
#define POP_DEC() (vm->stack->stack_dec[--vm->stack->sp_dec])
#define PUSH_STR(v) do { vm->stack->stack_str[vm->stack->sp_str++] = (v); } while(0)
#define POP_STR() (vm->stack->stack_str[--vm->stack->sp_str])

int vm_execute(VM *vm, Bytecode *bc) {
    vm->ip = 0;
    vm->running = true;
    
    while (vm->running && vm->ip < bc->code_size) {
        uint8_t op = bc->code[vm->ip++];
        
        switch (op) {
            case OP_NOP:
                break;
                
            case OP_PUSH_STR: {
                int idx = (bc->code[vm->ip] << 24) | (bc->code[vm->ip+1] << 16) |
                          (bc->code[vm->ip+2] << 8) | bc->code[vm->ip+3];
                vm->ip += 4;
                PUSH_STR(str_dup(bc->constants_str[idx]));
                break;
            }
            
            case OP_POP:
                break;
                
            case OP_LOAD_VAR:
            case OP_STORE_VAR:
            case OP_LOAD_GLOBAL:
            case OP_STORE_GLOBAL:
                vm->ip += 4;
                break;
                
            case OP_ADD: {
                double b = POP_DEC();
                double a = POP_DEC();
                PUSH_DEC(a + b);
                break;
            }
            
            case OP_SUB: {
                double b = POP_DEC();
                double a = POP_DEC();
                PUSH_DEC(a - b);
                break;
            }
            
            case OP_MUL: {
                double b = POP_DEC();
                double a = POP_DEC();
                PUSH_DEC(a * b);
                break;
            }
            
            case OP_DIV: {
                double b = POP_DEC();
                double a = POP_DEC();
                PUSH_DEC(b != 0 ? a / b : 0);
                break;
            }
            
            case OP_MOD: {
                long b = POP_NUM();
                long a = POP_NUM();
                PUSH_NUM(b != 0 ? a % b : 0);
                break;
            }
            
            case OP_POW: {
                double b = POP_DEC();
                double a = POP_DEC();
                PUSH_DEC(pow(a, b));
                break;
            }
            
            case OP_NEG: {
                double a = POP_DEC();
                PUSH_DEC(-a);
                break;
            }
            
            case OP_NOT: {
                long a = POP_NUM();
                PUSH_NUM(!a);
                break;
            }
            
            case OP_AND:
            case OP_OR:
            case OP_EQ:
            case OP_NEQ:
            case OP_GT:
            case OP_LT:
            case OP_GTE:
            case OP_LTE: {
                long b = POP_NUM();
                long a = POP_NUM();
                long result = 0;
                switch (op) {
                    case OP_EQ: result = (a == b); break;
                    case OP_NEQ: result = (a != b); break;
                    case OP_GT: result = (a > b); break;
                    case OP_LT: result = (a < b); break;
                    case OP_GTE: result = (a >= b); break;
                    case OP_LTE: result = (a <= b); break;
                    case OP_AND: result = (a && b); break;
                    case OP_OR: result = (a || b); break;
                }
                PUSH_NUM(result);
                break;
            }
            
            case OP_RANGE: {
                long high = POP_NUM();
                long low = POP_NUM();
                long value = POP_NUM();
                PUSH_NUM((value >= low && value <= high) ? 1 : 0);
                break;
            }
            
            case OP_JUMP: {
                int offset = (bc->code[vm->ip] << 24) | (bc->code[vm->ip+1] << 16) |
                             (bc->code[vm->ip+2] << 8) | bc->code[vm->ip+3];
                vm->ip += 4;
                vm->ip += offset;
                break;
            }
            
            case OP_JUMP_IF_FALSE: {
                int offset = (bc->code[vm->ip] << 24) | (bc->code[vm->ip+1] << 16) |
                             (bc->code[vm->ip+2] << 8) | bc->code[vm->ip+3];
                vm->ip += 4;
                long cond = vm->stack->stack_num[vm->stack->sp_num - 1];
                if (!cond) {
                    vm->ip += offset;
                }
                break;
            }
            
            case OP_JUMP_IF_TRUE: {
                int offset = (bc->code[vm->ip] << 24) | (bc->code[vm->ip+1] << 16) |
                             (bc->code[vm->ip+2] << 8) | bc->code[vm->ip+3];
                vm->ip += 4;
                long cond = vm->stack->stack_num[vm->stack->sp_num - 1];
                if (cond) {
                    vm->ip += offset;
                }
                break;
            }
            
            case OP_CALL:
            case OP_CALL_ASYNC:
            case OP_RETURN:
                break;
                
            case OP_PRINT: {
                if (vm->stack->sp_str > 0) {
                    char *str = POP_STR();
                    printf("%s\n", str);
                    free(str);
                } else if (vm->stack->sp_dec > 0) {
                    printf("%g\n", POP_DEC());
                } else if (vm->stack->sp_num > 0) {
                    printf("%ld\n", POP_NUM());
                }
                break;
            }
            
            case OP_INPUT: {
                char buffer[JAG_MAX_LINE];
                if (fgets(buffer, sizeof(buffer), stdin)) {
                    size_t len = strlen(buffer);
                    if (len > 0 && buffer[len-1] == '\n') {
                        buffer[len-1] = '\0';
                    }
                    PUSH_STR(str_dup(buffer));
                } else {
                    PUSH_STR(str_dup(""));
                }
                break;
            }
            
            case OP_DEG_CHECK: {
                char *actual_str = POP_STR();
                char *expected_str = POP_STR();
                char *type_str = POP_STR();
                
                if (strcmp(actual_str, expected_str) != 0) {
                    printf("live.deg: expectation failed - expected \"%s\", got \"%s\"\n",
                           expected_str, actual_str);
                }
                free(type_str);
                free(expected_str);
                free(actual_str);
                break;
            }
            
            case OP_BUILD_LIST:
            case OP_BUILD_DATA:
            case OP_APPEND:
            case OP_INSERT:
            case OP_DELETE:
            case OP_SORT:
            case OP_CONCAT:
            case OP_JOIN:
            case OP_FILE_OPEN:
            case OP_FILE_READ:
            case OP_FILE_LOOP:
            case OP_FILE_CLOSE:
            case OP_DIR_OPEN:
            case OP_DIR_READ:
            case OP_DIR_LOOP:
            case OP_DIR_CLOSE:
            case OP_DIR_DEL:
                break;
                
            case OP_HALT:
                vm->running = false;
                break;
                
            default:
                fprintf(stderr, "Unknown opcode: %d at ip=%d\n", op, vm->ip - 1);
                vm->running = false;
                break;
        }
    }
    
    return 0;
}

int compile_to_bytecode(ASTNode *ast, Bytecode *bc) {
    (void)ast;
    emit_op(bc, OP_PUSH_STR);
    emit_int(bc, add_constant_str(bc, "Hello from Jaguar VM!"));
    emit_op(bc, OP_PRINT);
    emit_op(bc, OP_HALT);
    
    return 0;
}
