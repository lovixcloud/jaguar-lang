/* ============================================================================
 * Jaguar Compiler - C Backend (Native Code Generation)
 * ========================================================================== */

#include "../include/jaguar.h"

#include <stdarg.h>
typedef struct {
    FILE *out;
    int indent;
    Compiler *comp;
} CGen;

static void cgen_indent(CGen *cg) {
    for (int i = 0; i < cg->indent; i++) {
        fprintf(cg->out, "    ");
    }
}

static void cgen_line(CGen *cg, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    cgen_indent(cg);
    vfprintf(cg->out, fmt, args);
    fprintf(cg->out, "\n");
    va_end(args);
}

static void cgen_emit(CGen *cg, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(cg->out, fmt, args);
    va_end(args);
}

static void cgen_node(CGen *cg, ASTNode *node);

static void cgen_block(CGen *cg, ASTNode *block) {
    if (!block || block->type != NODE_BLOCK) return;
    
    cgen_emit(cg, "{\n");
    cg->indent++;
    for (int i = 0; i < block->child_count; i++) {
        cgen_node(cg, block->children[i]);
    }
    cg->indent--;
    cgen_indent(cg);
    cgen_emit(cg, "}\n");
}

static void cgen_expr(CGen *cg, ASTNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_LITERAL:
            if (strcmp(node->type_annotation, "string") == 0) {
                cgen_emit(cg, "\"%s\"", node->value_str ? node->value_str : "");
            } else if (strcmp(node->type_annotation, "num") == 0) {
                cgen_emit(cg, "%ld", node->value_num);
            } else if (strcmp(node->type_annotation, "decimal") == 0) {
                cgen_emit(cg, "%g", node->value_dec);
            } else if (strcmp(node->type_annotation, "bool") == 0) {
                cgen_emit(cg, "%s", node->value_num ? "true" : "false");
            } else {
                cgen_emit(cg, "0");
            }
            break;
            
        case NODE_IDENTIFIER:
            cgen_emit(cg, "%s", node->value_str);
            break;
            
        case NODE_BINARY_OP:
            cgen_emit(cg, "(");
            cgen_expr(cg, node->children[0]);
            cgen_emit(cg, " %s ", node->value_str);
            cgen_expr(cg, node->children[1]);
            cgen_emit(cg, ")");
            break;
            
        case NODE_UNARY_OP:
            cgen_emit(cg, "(%s", node->value_str);
            cgen_expr(cg, node->children[0]);
            cgen_emit(cg, ")");
            break;
            
        case NODE_RANGE_OP:
            /* value <<< (low, high) */
            cgen_emit(cg, "(");
            cgen_expr(cg, node->children[0]);
            cgen_emit(cg, " >= ");
            cgen_expr(cg, node->children[1]);
            cgen_emit(cg, " && ");
            cgen_expr(cg, node->children[0]);
            cgen_emit(cg, " <= ");
            cgen_expr(cg, node->children[2]);
            cgen_emit(cg, ")");
            break;
            
        default:
            cgen_emit(cg, "0");
            break;
    }
}

static void cgen_node(CGen *cg, ASTNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_PROGRAM:
            for (int i = 0; i < node->child_count; i++) {
                cgen_node(cg, node->children[i]);
            }
            break;
            
        case NODE_VAR_DECL:
            cgen_line(cg, "%s %s;", node->type_annotation, node->value_str);
            if (node->child_count > 0) {
                cgen_indent(cg);
                cgen_emit(cg, "%s = ", node->value_str);
                cgen_expr(cg, node->children[0]);
                cgen_emit(cg, ";\n");
            }
            break;
            
        case NODE_FIXED_DECL:
            cgen_line(cg, "const %s %s = ", node->type_annotation, node->value_str);
            if (node->child_count > 0) {
                cgen_expr(cg, node->children[0]);
            } else {
                cgen_emit(cg, "0");
            }
            cgen_emit(cg, ";\n");
            break;
            
        case NODE_ASSIGNMENT:
            cgen_indent(cg);
            cgen_expr(cg, node->children[0]);
            cgen_emit(cg, " = ");
            cgen_expr(cg, node->children[1]);
            cgen_emit(cg, ";\n");
            break;
            
        case NODE_IF_STMT:
            cgen_indent(cg);
            cgen_emit(cg, "if (");
            cgen_expr(cg, node->children[0]);
            cgen_emit(cg, ") ");
            cgen_block(cg, node->children[1]);
            
            /* Check for elif/else */
            for (int i = 2; i < node->child_count; i++) {
                ASTNode *elif_or_else = node->children[i];
                if (elif_or_else->type == NODE_IF_STMT) {
                    cgen_indent(cg);
                    cgen_emit(cg, "else if (");
                    cgen_expr(cg, elif_or_else->children[0]);
                    cgen_emit(cg, ") ");
                    cgen_block(cg, elif_or_else->children[1]);
                } else {
                    cgen_indent(cg);
                    cgen_emit(cg, "else ");
                    cgen_block(cg, elif_or_else);
                }
            }
            break;
            
        case NODE_LOOP_STMT:
            cgen_indent(cg);
            cgen_emit(cg, "while (");
            cgen_expr(cg, node->children[0]);
            cgen_emit(cg, ") ");
            cgen_block(cg, node->children[1]);
            break;
            
        case NODE_DO_LOOP_STMT:
            cgen_indent(cg);
            cgen_emit(cg, "do ");
            cgen_block(cg, node->children[0]);
            cgen_emit(cg, " while (");
            cgen_expr(cg, node->children[1]);
            cgen_emit(cg, ");\n");
            break;
            
        case NODE_FORIN_STMT:
            cgen_line(cg, "/* for-in loop - simplified */");
            break;
            
        case NODE_ITERATE_STMT:
            cgen_line(cg, "/* iterate - simplified */");
            break;
            
        case NODE_FUNCTION_DEF:
            cgen_line(cg, "%s %s(", 
                     node->type_annotation ? node->type_annotation : "void",
                     node->value_str);
            /* Parameters */
            int param_count = node->child_count - 1;
            for (int i = 0; i < param_count; i++) {
                ASTNode *param = node->children[i];
                if (param->type == NODE_VAR_DECL) {
                    if (i > 0) cgen_emit(cg, ", ");
                    cgen_emit(cg, "%s %s", param->type_annotation, param->value_str);
                }
            }
            cgen_emit(cg, ") ");
            if (node->child_count > 0) {
                cgen_block(cg, node->children[node->child_count - 1]);
            }
            break;
            
        case NODE_FUNCTION_CALL:
            if (strcmp(node->value_str, "on") == 0) {
                cgen_indent(cg);
                cgen_emit(cg, "printf(\"%%s\\n\", ");
                if (node->child_count > 0) {
                    cgen_expr(cg, node->children[0]);
                } else {
                    cgen_emit(cg, "\"\"");
                }
                cgen_emit(cg, ");\n");
            } else if (strcmp(node->value_str, "in") == 0) {
                cgen_indent(cg);
                cgen_emit(cg, "char _input_buf[1024]; fgets(_input_buf, sizeof(_input_buf), stdin);\n");
            } else if (strcmp(node->value_str, "deg") == 0) {
                cgen_indent(cg);
                cgen_emit(cg, "/* live.deg check */\n");
            }
            break;
            
        case NODE_RETURN_STMT:
            cgen_indent(cg);
            cgen_emit(cg, "return");
            if (node->child_count > 0) {
                cgen_emit(cg, " ");
                cgen_expr(cg, node->children[0]);
            }
            cgen_emit(cg, ";\n");
            break;
            
        case NODE_CLASS_DEF:
        case NODE_STRUCT_DEF:
            cgen_line(cg, "/* class/struct %s - deferred */", node->value_str);
            break;
            
        case NODE_ENUM_DEF:
            cgen_indent(cg);
            cgen_emit(cg, "enum %s { ", node->value_str);
            for (int i = 0; i < node->child_count; i++) {
                if (i > 0) cgen_emit(cg, ", ");
                cgen_emit(cg, "%s", node->children[i]->value_str);
            }
            cgen_emit(cg, " };\n");
            break;
            
        case NODE_IMPORT_STMT:
            cgen_line(cg, "/* import \"%s\" - handled separately */", node->value_str);
            break;
            
        case NODE_EXPORT_STMT:
            cgen_line(cg, "/* export %s */", node->value_str);
            break;
            
        case NODE_BLOCK:
            cgen_block(cg, node);
            break;
            
        default:
            break;
    }
}

int compile_to_c(ASTNode *ast, const char *output_path) {
    FILE *out = fopen(output_path, "w");
    if (!out) {
        fprintf(stderr, "Error: Cannot open output file '%s'\n", output_path);
        return -1;
    }
    
    /* Write header */
    fprintf(out, "#include <stdio.h>\n");
    fprintf(out, "#include <stdlib.h>\n");
    fprintf(out, "#include <string.h>\n\n");
    
    CGen cg;
    cg.out = out;
    cg.indent = 0;
    cg.comp = NULL;
    
    /* Generate main function wrapper */
    fprintf(out, "int main(int argc, char **argv) {\n");
    cg.indent = 1;
    
    /* Generate code for program body */
    for (int i = 0; i < ast->child_count; i++) {
        ASTNode *stmt = ast->children[i];
        
        /* Skip declarations that should be at file scope */
        if (stmt->type == NODE_VAR_DECL || stmt->type == NODE_FIXED_DECL ||
            stmt->type == NODE_FUNCTION_DEF || stmt->type == NODE_CLASS_DEF ||
            stmt->type == NODE_STRUCT_DEF || stmt->type == NODE_ENUM_DEF) {
            /* Move to file scope in full impl */
            continue;
        }
        
        cgen_node(&cg, stmt);
    }
    
    cg.indent = 0;
    fprintf(out, "    return 0;\n");
    fprintf(out, "}\n");
    
    fclose(out);
    return 0;
}
