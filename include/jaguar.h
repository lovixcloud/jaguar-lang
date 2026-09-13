#ifndef JAGUAR_H
#define JAGUAR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>

#ifdef __linux__
#include <sys/inotify.h>
#endif

/* ============================================================================
 * Configuration & Constants
 * ========================================================================== */

#define JAG_MAX_PATH 4096
#define JAG_MAX_LINE 8192
#define JAG_MAX_IMPORTS 64
#define JAG_STACK_SIZE 4096
#define JAG_HEAP_SIZE (1024 * 1024)
#define JAG_MAX_VARS 1024
#define JAG_MAX_FUNCTIONS 256
#define JAG_MAX_CLASSES 128
#define JAG_MAX_ENUMS 64
#define JAG_MAX_STRUCTS 64

/* ============================================================================
 * Token Types (Lexer)
 * ========================================================================== */

typedef enum {
    TOK_EOF = 0,
    TOK_ERROR,
    
    /* Literals */
    TOK_NUMBER,
    TOK_DECIMAL,
    TOK_SCIFI,
    TOK_STRING,
    TOK_TRUE,
    TOK_FALSE,
    
    /* Identifiers & Keywords */
    TOK_IDENT,
    TOK_VAR,
    TOK_FIXED,
    TOK_FUN,
    TOK_ASYNC,
    TOK_CLASS,
    TOK_PUBLIC,
    TOK_PRIVATE,
    TOK_ENUM,
    TOK_STRUCT,
    TOK_VECTOR,
    TOK_MATRIX,
    TOK_DATA,
    TOK_LIST,
    TOK_MIXEDLIST,
    TOK_IF,
    TOK_ELIF,
    TOK_ELSE,
    TOK_LOOP,
    TOK_DO,
    TOK_WHILE,
    TOK_FORIN,
    TOK_ITERATE,
    TOK_IN,
    TOK_RETURN,
    TOK_IMPORT,
    TOK_FROM,
    TOK_EXPORT,
    TOK_LIVE,
    TOK_FILE,
    TOK_DIR,
    
    /* Operators */
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_PERCENT,
    TOK_STARSTAR,
    TOK_PLUS_EQ,
    TOK_MINUS_EQ,
    TOK_STAR_EQ,
    TOK_SLASH_EQ,
    TOK_PERCENT_EQ,
    TOK_STARSTAR_EQ,
    TOK_GT,
    TOK_LT,
    TOK_GTE,
    TOK_LTE,
    TOK_EQEQ,
    TOK_NEQ,
    TOK_RANGE,        /* <<< range operator */
    TOK_SHL,          /* << bitwise shift left */
    TOK_SHR,          /* >> bitwise shift right */
    TOK_ASSIGN,
    TOK_DOT,
    TOK_COMMA,
    TOK_COLON,
    TOK_SEMICOLON,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_QUESTION,
    
    /* Template interpolation */
    TOK_TEMPLATE_VAR,   /* {{var}} */
    TOK_TEMPLATE_EXPR,  /* {expr} */
    TOK_TEMPLATE_ANY,   /* ${...} */
    
} TokenType;

typedef struct {
    TokenType type;
    char *lexeme;
    int line;
    int column;
    union {
        long num_value;
        double dec_value;
        double scifi_value;
    };
} Token;

/* ============================================================================
 * AST Node Types
 * ========================================================================== */

typedef enum {
    NODE_PROGRAM,
    NODE_VAR_DECL,
    NODE_FIXED_DECL,
    NODE_ASSIGNMENT,
    NODE_BINARY_OP,
    NODE_UNARY_OP,
    NODE_RANGE_OP,       /* <<< operator */
    NODE_LITERAL,
    NODE_IDENTIFIER,
    NODE_IF_STMT,
    NODE_LOOP_STMT,
    NODE_DO_LOOP_STMT,
    NODE_FORIN_STMT,
    NODE_ITERATE_STMT,
    NODE_FUNCTION_DEF,
    NODE_FUNCTION_CALL,
    NODE_RETURN_STMT,
    NODE_CLASS_DEF,
    NODE_STRUCT_DEF,
    NODE_ENUM_DEF,
    NODE_VECTOR_DEF,
    NODE_MATRIX_DEF,
    NODE_DATA_LITERAL,
    NODE_LIST_LITERAL,
    NODE_MIXEDLIST_LITERAL,
    NODE_MEMBER_ACCESS,
    NODE_METHOD_CALL,
    NODE_IMPORT_STMT,
    NODE_EXPORT_STMT,
    NODE_BLOCK,
    NODE_TEMPLATE_STRING,
    NODE_LIVE_ON,
    NODE_LIVE_IN,
    NODE_LIVE_DEG,
    NODE_FILE_OP,
    NODE_DIR_OP,
} NodeType;

typedef struct ASTNode {
    NodeType type;
    int line;
    int column;
    char *value_str;
    long value_num;
    double value_dec;
    struct ASTNode **children;
    int child_count;
    char *type_annotation;
    bool is_fixed;
    bool is_async;
    bool is_public;
    struct ASTNode *next;
} ASTNode;

/* ============================================================================
 * Type System
 * ========================================================================== */

typedef enum {
    TYPE_VOID,
    TYPE_STRING,
    TYPE_NUM,
    TYPE_DECIMAL,
    TYPE_BOOL,
    TYPE_SCIFI,
    TYPE_DATA,
    TYPE_LIST,
    TYPE_MIXEDLIST,
    TYPE_VECTOR,
    TYPE_MATRIX,
    TYPE_ENUM,
    TYPE_STRUCT,
    TYPE_CLASS,
    TYPE_FUNCTION,
    TYPE_UNKNOWN,
} TypeKind;

typedef struct {
    TypeKind kind;
    char *name;
    char *element_type;  /* for list<T>, vector<T>, matrix<T> */
    bool is_generic;
} TypeInfo;

/* ============================================================================
 * Symbol Table
 * ========================================================================== */

typedef enum {
    SYM_VAR,
    SYM_FIXED,
    SYM_FUNCTION,
    SYM_CLASS,
    SYM_STRUCT,
    SYM_ENUM,
    SYM_PARAMETER,
} SymbolKind;

typedef struct Symbol {
    char *name;
    SymbolKind kind;
    TypeInfo *type;
    int scope_level;
    bool is_assigned;
    bool is_exported;
    struct Symbol *next;
} Symbol;

typedef struct {
    Symbol **symbols;
    int count;
    int capacity;
    int scope_level;
} SymbolTable;

/* ============================================================================
 * Bytecode VM (Backend A)
 * ========================================================================== */

typedef enum {
    OP_NOP,
    OP_PUSH_NUM,
    OP_PUSH_DEC,
    OP_PUSH_STR,
    OP_PUSH_BOOL,
    OP_POP,
    OP_LOAD_VAR,
    OP_STORE_VAR,
    OP_LOAD_GLOBAL,
    OP_STORE_GLOBAL,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_POW,
    OP_NEG,
    OP_NOT,
    OP_AND,
    OP_OR,
    OP_EQ,
    OP_NEQ,
    OP_GT,
    OP_LT,
    OP_GTE,
    OP_LTE,
    OP_RANGE,        /* <<< operator */
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_JUMP_IF_TRUE,
    OP_CALL,
    OP_CALL_ASYNC,
    OP_RETURN,
    OP_PRINT,
    OP_INPUT,
    OP_DEG_CHECK,
    OP_BUILD_LIST,
    OP_BUILD_DATA,
    OP_APPEND,
    OP_INSERT,
    OP_DELETE,
    OP_SORT,
    OP_CONCAT,
    OP_JOIN,
    OP_FILE_OPEN,
    OP_FILE_READ,
    OP_FILE_LOOP,
    OP_FILE_CLOSE,
    OP_DIR_OPEN,
    OP_DIR_READ,
    OP_DIR_LOOP,
    OP_DIR_CLOSE,
    OP_DIR_DEL,
    OP_HALT,
} OpCode;

typedef struct {
    uint8_t *code;
    int code_size;
    int code_capacity;
    long *constants_num;
    double *constants_dec;
    char **constants_str;
    int const_num_count;
    int const_dec_count;
    int const_str_count;
    int const_capacity;
} Bytecode;

typedef struct {
    long *stack_num;
    double *stack_dec;
    char **stack_str;
    int sp_num;
    int sp_dec;
    int sp_str;
    bool *defined_flags;
    TypeInfo *type_stack;
} VMStack;

typedef struct {
    Bytecode *bytecode;
    VMStack *stack;
    SymbolTable *globals;
    int ip;
    bool running;
    bool live_mode;
    char *source_file;
} VM;

/* ============================================================================
 * File Watcher (Live Mode)
 * ========================================================================== */

typedef struct {
    char **files;
    int count;
    time_t *mtimes;
} WatchList;

/* ============================================================================
 * Compiler Context
 * ========================================================================== */

typedef struct {
    char *source;
    int source_len;
    int pos;
    int line;
    int column;
    Token *tokens;
    int token_count;
    int token_capacity;
    ASTNode *ast;
    SymbolTable *symbols;
    bool has_errors;
    char *current_file;
    char **import_files;
    int import_count;
    bool live_mode;
    bool is_first_statement;
} Compiler;

/* ============================================================================
 * Function Declarations
 * ========================================================================== */

/* Lexer */
Compiler *compiler_create(void);
void compiler_free(Compiler *comp);
int lex(Compiler *comp, const char *source);
void print_token_error(Compiler *comp, int token_idx, const char *msg);

/* Parser */
ASTNode *parse(Compiler *comp);
ASTNode *create_node(NodeType type, int line, int column);
void free_ast(ASTNode *node);

/* Type Checker */
bool typecheck(Compiler *comp, ASTNode *ast);
TypeInfo *create_type(TypeKind kind, const char *name, const char *elem_type);
Symbol *symbol_add(SymbolTable *table, const char *name, SymbolKind kind, TypeInfo *type);
Symbol *symbol_lookup(SymbolTable *table, const char *name);

/* Bytecode Backend (VM) */
Bytecode *bytecode_create(void);
void bytecode_free(Bytecode *bc);
VM *vm_create(void);
void vm_free(VM *vm);
int vm_execute(VM *vm, Bytecode *bc);
int compile_to_bytecode(ASTNode *ast, Bytecode *bc);

/* C Backend (Native) */
int compile_to_c(ASTNode *ast, const char *output_path);

/* Live Mode */
int run_live_mode(const char *filename);

/* CLI */
int cli_main(int argc, char **argv);

/* Utilities */
char *read_file(const char *path);
void write_file(const char *path, const char *content);
char *str_dup(const char *s);
void error_at(const char *file, int line, int col, const char *msg);

#endif /* JAGUAR_H */
