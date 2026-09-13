/* ============================================================================
 * Jaguar Compiler - Type Checker Implementation
 * ========================================================================== */

#include "../include/jaguar.h"

TypeInfo *create_type(TypeKind kind, const char *name, const char *elem_type) {
    TypeInfo *type = (TypeInfo *)calloc(1, sizeof(TypeInfo));
    type->kind = kind;
    type->name = str_dup(name ? name : "unknown");
    type->element_type = elem_type ? str_dup(elem_type) : NULL;
    type->is_generic = (elem_type != NULL);
    return type;
}

SymbolTable *symbol_table_create(void) {
    SymbolTable *table = (SymbolTable *)calloc(1, sizeof(SymbolTable));
    table->capacity = 256;
    table->symbols = (Symbol **)calloc(table->capacity, sizeof(Symbol *));
    table->count = 0;
    table->scope_level = 0;
    return table;
}

Symbol *symbol_add(SymbolTable *table, const char *name, SymbolKind kind, TypeInfo *type) {
    if (!table || !name) return NULL;
    
    /* Check for duplicate in current scope */
    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->symbols[i]->name, name) == 0 &&
            table->symbols[i]->scope_level == table->scope_level) {
            return table->symbols[i]; /* Duplicate */
        }
    }
    
    if (table->count >= table->capacity) {
        table->capacity *= 2;
        table->symbols = (Symbol **)realloc(table->symbols, 
                                            table->capacity * sizeof(Symbol *));
    }
    
    Symbol *sym = (Symbol *)calloc(1, sizeof(Symbol));
    sym->name = str_dup(name);
    sym->kind = kind;
    sym->type = type;
    sym->scope_level = table->scope_level;
    sym->is_assigned = false;
    sym->is_exported = false;
    
    table->symbols[table->count++] = sym;
    return sym;
}

Symbol *symbol_lookup(SymbolTable *table, const char *name) {
    if (!table || !name) return NULL;
    
    /* Search from most recent to oldest */
    for (int i = table->count - 1; i >= 0; i--) {
        if (strcmp(table->symbols[i]->name, name) == 0) {
            return table->symbols[i];
        }
    }
    return NULL;
}

static TypeKind type_from_name(const char *name) {
    if (!name) return TYPE_UNKNOWN;
    if (strcmp(name, "string") == 0) return TYPE_STRING;
    if (strcmp(name, "num") == 0) return TYPE_NUM;
    if (strcmp(name, "decimal") == 0) return TYPE_DECIMAL;
    if (strcmp(name, "bool") == 0) return TYPE_BOOL;
    if (strcmp(name, "scifi") == 0) return TYPE_SCIFI;
    if (strcmp(name, "data") == 0) return TYPE_DATA;
    if (strncmp(name, "list", 4) == 0) return TYPE_LIST;
    if (strcmp(name, "MixedList") == 0) return TYPE_MIXEDLIST;
    if (strncmp(name, "vector", 6) == 0) return TYPE_VECTOR;
    if (strncmp(name, "matrix", 6) == 0) return TYPE_MATRIX;
    return TYPE_UNKNOWN;
}

static bool types_match(TypeInfo *a, TypeInfo *b) {
    if (!a || !b) return false;
    if (a->kind != b->kind) return false;
    if (a->is_generic && b->is_generic) {
        if (!a->element_type || !b->element_type) return false;
        return strcmp(a->element_type, b->element_type) == 0;
    }
    return true;
}

typedef struct {
    Compiler *comp;
    SymbolTable *scope;
    int errors;
    char *current_function;
    TypeKind return_type;
} TypeChecker;

static void typecheck_node(TypeChecker *tc, ASTNode *node);

static void report_error(TypeChecker *tc, ASTNode *node, const char *msg) {
    fprintf(stderr, "Type error at %s:%d:%d: %s\n",
            tc->comp->current_file ? tc->comp->current_file : "<stdin>",
            node->line, node->column, msg);
    tc->errors++;
    tc->comp->has_errors = true;
}

static void typecheck_block(TypeChecker *tc, ASTNode *block) {
    if (!block || block->type != NODE_BLOCK) return;
    
    tc->scope->scope_level++;
    
    for (int i = 0; i < block->child_count; i++) {
        typecheck_node(tc, block->children[i]);
    }
    
    tc->scope->scope_level--;
}

static void typecheck_node(TypeChecker *tc, ASTNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_PROGRAM: {
            for (int i = 0; i < node->child_count; i++) {
                typecheck_node(tc, node->children[i]);
            }
            break;
        }
        
        case NODE_VAR_DECL: {
            Symbol *existing = symbol_lookup(tc->scope, node->value_str);
            if (existing && existing->scope_level == tc->scope->scope_level) {
                report_error(tc, node, "duplicate variable declaration");
                break;
            }
            
            TypeInfo *type = create_type(type_from_name(node->type_annotation),
                                         node->type_annotation, NULL);
            
            /* Check for scifi type on var (should be fixed) */
            if (type->kind == TYPE_SCIFI) {
                report_error(tc, node, "scifi literals must be declared as fixed");
            }
            
            Symbol *sym = symbol_add(tc->scope, node->value_str, SYM_VAR, type);
            
            /* Check initialization */
            if (node->child_count > 0) {
                typecheck_node(tc, node->children[0]);
                if (sym) sym->is_assigned = true;
            }
            break;
        }
        
        case NODE_FIXED_DECL: {
            /* fixed must be initialized */
            if (node->child_count == 0) {
                report_error(tc, node, "fixed must be initialized at declaration");
                break;
            }
            
            TypeInfo *type = create_type(type_from_name(node->type_annotation),
                                         node->type_annotation, NULL);
            Symbol *sym = symbol_add(tc->scope, node->value_str, SYM_FIXED, type);
            
            typecheck_node(tc, node->children[0]);
            if (sym) sym->is_assigned = true;
            break;
        }
        
        case NODE_ASSIGNMENT: {
            if (node->child_count < 2) break;
            
            ASTNode *target = node->children[0];
            if (target->type == NODE_IDENTIFIER) {
                Symbol *sym = symbol_lookup(tc->scope, target->value_str);
                if (!sym) {
                    report_error(tc, target, "undefined variable");
                } else if (sym->kind == SYM_FIXED) {
                    report_error(tc, target, "cannot reassign fixed variable");
                } else {
                    sym->is_assigned = true;
                }
            }
            
            typecheck_node(tc, node->children[1]);
            break;
        }
        
        case NODE_LITERAL:
            /* Literals are already typed during parsing */
            break;
        
        case NODE_IDENTIFIER: {
            Symbol *sym = symbol_lookup(tc->scope, node->value_str);
            if (!sym) {
                report_error(tc, node, "undefined variable");
            } else if (!sym->is_assigned) {
                report_error(tc, node, "variable used before assignment");
            }
            break;
        }
        
        case NODE_BINARY_OP:
        case NODE_UNARY_OP: {
            for (int i = 0; i < node->child_count; i++) {
                typecheck_node(tc, node->children[i]);
            }
            break;
        }
        
        case NODE_RANGE_OP: {
            /* <<< operator: value <<< (low, high) */
            if (node->child_count != 3) {
                report_error(tc, node, "range operator requires 3 operands");
                break;
            }
            for (int i = 0; i < node->child_count; i++) {
                typecheck_node(tc, node->children[i]);
            }
            break;
        }
        
        case NODE_IF_STMT: {
            /* First child is condition, rest are blocks/elifs */
            if (node->child_count > 0) {
                typecheck_node(tc, node->children[0]);
            }
            for (int i = 1; i < node->child_count; i++) {
                typecheck_node(tc, node->children[i]);
            }
            break;
        }
        
        case NODE_LOOP_STMT:
        case NODE_DO_LOOP_STMT: {
            for (int i = 0; i < node->child_count; i++) {
                typecheck_node(tc, node->children[i]);
            }
            break;
        }
        
        case NODE_FORIN_STMT: {
            /* Enter new scope for loop variable */
            tc->scope->scope_level++;
            
            if (node->child_count > 0) {
                ASTNode *var_or_ident = node->children[0];
                if (var_or_ident->type == NODE_VAR_DECL) {
                    TypeInfo *type = create_type(TYPE_UNKNOWN, "unknown", NULL);
                    symbol_add(tc->scope, var_or_ident->value_str, SYM_VAR, type);
                }
            }
            
            for (int i = 0; i < node->child_count; i++) {
                typecheck_node(tc, node->children[i]);
            }
            
            tc->scope->scope_level--;
            break;
        }
        
        case NODE_ITERATE_STMT: {
            tc->scope->scope_level++;
            for (int i = 0; i < node->child_count; i++) {
                typecheck_node(tc, node->children[i]);
            }
            tc->scope->scope_level--;
            break;
        }
        
        case NODE_FUNCTION_DEF: {
            tc->scope->scope_level++;
            tc->current_function = node->value_str;
            
            /* Add parameters to scope */
            for (int i = 0; i < node->child_count - 1; i++) {
                ASTNode *param = node->children[i];
                if (param->type == NODE_VAR_DECL) {
                    TypeInfo *type = create_type(type_from_name(param->type_annotation),
                                                 param->type_annotation, NULL);
                    symbol_add(tc->scope, param->value_str, SYM_PARAMETER, type);
                }
            }
            
            /* Typecheck body (last child) */
            if (node->child_count > 0) {
                ASTNode *body = node->children[node->child_count - 1];
                typecheck_block(tc, body);
            }
            
            tc->current_function = NULL;
            tc->scope->scope_level--;
            break;
        }
        
        case NODE_FUNCTION_CALL: {
            /* Handle live.on, live.in, live.deg specially */
            if (strcmp(node->value_str, "deg") == 0) {
                /* live.deg must have exactly 3 arguments */
                if (node->child_count != 3) {
                    report_error(tc, node, "live.deg requires exactly 3 arguments");
                }
            }
            for (int i = 0; i < node->child_count; i++) {
                typecheck_node(tc, node->children[i]);
            }
            break;
        }
        
        case NODE_METHOD_CALL: {
            for (int i = 0; i < node->child_count; i++) {
                typecheck_node(tc, node->children[i]);
            }
            break;
        }
        
        case NODE_RETURN_STMT: {
            if (node->child_count > 0) {
                typecheck_node(tc, node->children[0]);
            }
            break;
        }
        
        case NODE_CLASS_DEF: {
            tc->scope->scope_level++;
            Symbol *sym = symbol_add(tc->scope, node->value_str, SYM_CLASS,
                                     create_type(TYPE_CLASS, node->value_str, NULL));
            sym->is_exported = node->is_public;
            
            /* Process class members */
            if (node->child_count > 0) {
                ASTNode *body = node->children[0];
                for (int i = 0; i < body->child_count; i++) {
                    ASTNode *member = body->children[i];
                    if (member->type == NODE_VAR_DECL) {
                        TypeInfo *type = create_type(type_from_name(member->type_annotation),
                                                     member->type_annotation, NULL);
                        symbol_add(tc->scope, member->value_str, SYM_VAR, type);
                    }
                }
            }
            tc->scope->scope_level--;
            break;
        }
        
        case NODE_STRUCT_DEF: {
            tc->scope->scope_level++;
            symbol_add(tc->scope, node->value_str, SYM_STRUCT,
                      create_type(TYPE_STRUCT, node->value_str, NULL));
            
            if (node->child_count > 0) {
                ASTNode *body = node->children[0];
                for (int i = 0; i < body->child_count; i++) {
                    ASTNode *field = body->children[i];
                    if (field->type == NODE_VAR_DECL || field->type == NODE_IDENTIFIER) {
                        /* Field declaration */
                    }
                }
            }
            tc->scope->scope_level--;
            break;
        }
        
        case NODE_ENUM_DEF: {
            symbol_add(tc->scope, node->value_str, SYM_ENUM,
                      create_type(TYPE_ENUM, node->value_str, NULL));
            
            /* Add enum members as constants */
            for (int i = 0; i < node->child_count; i++) {
                ASTNode *member = node->children[i];
                if (member->type == NODE_IDENTIFIER) {
                    symbol_add(tc->scope, member->value_str, SYM_FIXED,
                              create_type(TYPE_NUM, "num", NULL));
                }
            }
            break;
        }
        
        case NODE_LIST_LITERAL:
        case NODE_MIXEDLIST_LITERAL:
        case NODE_DATA_LITERAL:
        case NODE_VECTOR_DEF:
        case NODE_MATRIX_DEF: {
            for (int i = 0; i < node->child_count; i++) {
                typecheck_node(tc, node->children[i]);
            }
            break;
        }
        
        case NODE_IMPORT_STMT: {
            /* Track import for file watching */
            if (node->value_str && tc->comp->import_count < JAG_MAX_IMPORTS) {
                tc->comp->import_files[tc->comp->import_count++] = str_dup(node->value_str);
            }
            break;
        }
        
        case NODE_EXPORT_STMT: {
            Symbol *sym = symbol_lookup(tc->scope, node->value_str);
            if (sym) {
                sym->is_exported = true;
            }
            break;
        }
        
        case NODE_BLOCK: {
            typecheck_block(tc, node);
            break;
        }
        
        case NODE_FILE_OP:
        case NODE_DIR_OP: {
            for (int i = 0; i < node->child_count; i++) {
                typecheck_node(tc, node->children[i]);
            }
            break;
        }
        
        default: {
            for (int i = 0; i < node->child_count; i++) {
                typecheck_node(tc, node->children[i]);
            }
            break;
        }
    }
}

bool typecheck(Compiler *comp, ASTNode *ast) {
    TypeChecker tc;
    tc.comp = comp;
    tc.scope = comp->symbols;
    tc.errors = 0;
    tc.current_function = NULL;
    
    typecheck_node(&tc, ast);
    
    return tc.errors == 0;
}
