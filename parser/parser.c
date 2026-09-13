/* ============================================================================
 * Jaguar Compiler - Parser Implementation
 * ========================================================================== */

#include "../include/jaguar.h"

typedef struct {
    Compiler *comp;
    int current;
} Parser;

static Token *current(Parser *p) {
    return &p->comp->tokens[p->current];
}

static Token *previous(Parser *p) {
    if (p->current <= 0) return &p->comp->tokens[0];
    return &p->comp->tokens[p->current - 1];
}

static Token *advance(Parser *p) {
    p->current++;
    return current(p);
}

static bool check(Parser *p, TokenType type) {
    return current(p)->type == type;
}

static bool match_token(Parser *p, TokenType type) {
    if (!check(p, type)) return false;
    advance(p);
    return true;
}

static bool consume(Parser *p, TokenType type, const char *msg) {
    if (check(p, type)) {
        advance(p);
        return true;
    }
    print_token_error(p->comp, p->current, msg);
    return false;
}

static void synchronize(Parser *p) {
    advance(p);
    while (!check(p, TOK_EOF)) {
        if (previous(p)->type == TOK_SEMICOLON) return;
        switch (current(p)->type) {
            case TOK_VAR:
            case TOK_FIXED:
            case TOK_FUN:
            case TOK_CLASS:
            case TOK_ENUM:
            case TOK_STRUCT:
            case TOK_IF:
            case TOK_LOOP:
            case TOK_DO:
            case TOK_FORIN:
            case TOK_ITERATE:
            case TOK_RETURN:
            case TOK_IMPORT:
            case TOK_EXPORT:
                return;
            default:
                break;
        }
        advance(p);
    }
}

ASTNode *create_node(NodeType type, int line, int column) {
    ASTNode *node = (ASTNode *)calloc(1, sizeof(ASTNode));
    node->type = type;
    node->line = line;
    node->column = column;
    node->child_count = 0;
    node->children = NULL;
    node->next = NULL;
    node->is_fixed = false;
    node->is_async = false;
    node->is_public = false;
    return node;
}

static void add_child(ASTNode *parent, ASTNode *child) {
    if (!parent || !child) return;
    parent->child_count++;
    parent->children = (ASTNode **)realloc(parent->children, 
                                           parent->child_count * sizeof(ASTNode *));
    parent->children[parent->child_count - 1] = child;
}

void free_ast(ASTNode *node) {
    if (!node) return;
    for (int i = 0; i < node->child_count; i++) {
        free_ast(node->children[i]);
    }
    free(node->children);
    free(node->value_str);
    free(node->type_annotation);
    free(node);
}

static ASTNode *expression(Parser *p);
ASTNode *parse_statement(Parser *p);

static ASTNode *parse_identifier(Parser *p) {
    if (!check(p, TOK_IDENT)) return NULL;
    Token *tok = current(p);
    ASTNode *node = create_node(NODE_IDENTIFIER, tok->line, tok->column);
    node->value_str = str_dup(tok->lexeme);
    advance(p);
    return node;
}

static ASTNode *parse_literal(Parser *p) {
    Token *tok = current(p);
    
    switch (tok->type) {
        case TOK_NUMBER: {
            ASTNode *node = create_node(NODE_LITERAL, tok->line, tok->column);
            node->value_num = atol(tok->lexeme);
            node->type_annotation = str_dup("num");
            advance(p);
            return node;
        }
        case TOK_DECIMAL: {
            ASTNode *node = create_node(NODE_LITERAL, tok->line, tok->column);
            node->value_dec = atof(tok->lexeme);
            node->type_annotation = str_dup("decimal");
            advance(p);
            return node;
        }
        case TOK_SCIFI: {
            ASTNode *node = create_node(NODE_LITERAL, tok->line, tok->column);
            /* Parse scientific notation */
            node->value_dec = strtod(tok->lexeme, NULL);
            node->type_annotation = str_dup("scifi");
            advance(p);
            return node;
        }
        case TOK_STRING: {
            ASTNode *node = create_node(NODE_LITERAL, tok->line, tok->column);
            /* Remove quotes */
            int len = strlen(tok->lexeme);
            if (len >= 2) {
                node->value_str = (char *)malloc(len - 1);
                strncpy(node->value_str, tok->lexeme + 1, len - 2);
                node->value_str[len - 2] = '\0';
            } else {
                node->value_str = str_dup("");
            }
            node->type_annotation = str_dup("string");
            advance(p);
            return node;
        }
        case TOK_TRUE:
        case TOK_FALSE: {
            ASTNode *node = create_node(NODE_LITERAL, tok->line, tok->column);
            node->value_num = (tok->type == TOK_TRUE) ? 1 : 0;
            node->type_annotation = str_dup("bool");
            advance(p);
            return node;
        }
        default:
            return NULL;
    }
}

static ASTNode *parse_primary(Parser *p) {
    if (match_token(p, TOK_LPAREN)) {
        ASTNode *expr = expression(p);
        consume(p, TOK_RPAREN, "Expected ')' after expression");
        return expr;
    }
    
    ASTNode *lit = parse_literal(p);
    if (lit) return lit;
    
    ASTNode *ident = parse_identifier(p);
    if (ident) return ident;
    
    /* Handle live.on, live.in, live.deg, file.*, dir.* */
    if (match_token(p, TOK_LIVE)) {
        if (match_token(p, TOK_DOT)) {
            Token *method = current(p);
            if (check(p, TOK_IDENT)) {
                ASTNode *call = create_node(NODE_FUNCTION_CALL, method->line, method->column);
                call->value_str = str_dup(method->lexeme);
                advance(p);
                
                if (consume(p, TOK_LPAREN, "Expected '(' after method name")) {
                    /* Parse arguments */
                    while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
                        ASTNode *arg = expression(p);
                        if (arg) add_child(call, arg);
                        if (!match_token(p, TOK_COMMA)) break;
                    }
                    consume(p, TOK_RPAREN, "Expected ')' after arguments");
                }
                return call;
            }
        }
    }
    
    if (match_token(p, TOK_FILE)) {
        if (match_token(p, TOK_DOT)) {
            Token *method = current(p);
            if (check(p, TOK_IDENT)) {
                ASTNode *node = create_node(NODE_FILE_OP, method->line, method->column);
                node->value_str = str_dup(method->lexeme);
                advance(p);
                
                if (match_token(p, TOK_LPAREN)) {
                    while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
                        ASTNode *arg = expression(p);
                        if (arg) add_child(node, arg);
                        if (!match_token(p, TOK_COMMA)) break;
                    }
                    consume(p, TOK_RPAREN, "Expected ')'");
                }
                return node;
            }
        }
    }
    
    if (match_token(p, TOK_DIR)) {
        if (match_token(p, TOK_DOT)) {
            Token *method = current(p);
            if (check(p, TOK_IDENT)) {
                ASTNode *node = create_node(NODE_DIR_OP, method->line, method->column);
                node->value_str = str_dup(method->lexeme);
                advance(p);
                
                if (match_token(p, TOK_LPAREN)) {
                    while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
                        ASTNode *arg = expression(p);
                        if (arg) add_child(node, arg);
                        if (!match_token(p, TOK_COMMA)) break;
                    }
                    consume(p, TOK_RPAREN, "Expected ')'");
                }
                return node;
            }
        }
    }
    
    return NULL;
}

static ASTNode *parse_unary(Parser *p) {
    if (match_token(p, TOK_MINUS)) {
        ASTNode *node = create_node(NODE_UNARY_OP, previous(p)->line, previous(p)->column);
        node->value_str = str_dup("-");
        add_child(node, parse_unary(p));
        return node;
    }
    return parse_primary(p);
}

static ASTNode *parse_multiplicative(Parser *p) {
    ASTNode *left = parse_unary(p);
    
    while (match_token(p, TOK_STAR) || match_token(p, TOK_SLASH) || 
           match_token(p, TOK_PERCENT) || match_token(p, TOK_STARSTAR)) {
        TokenType op = previous(p)->type;
        ASTNode *node = create_node(NODE_BINARY_OP, previous(p)->line, previous(p)->column);
        node->value_str = str_dup(op == TOK_STAR ? "*" :
                                  op == TOK_SLASH ? "/" :
                                  op == TOK_PERCENT ? "%" : "**");
        add_child(node, left);
        add_child(node, parse_unary(p));
        left = node;
    }
    
    return left;
}

static ASTNode *parse_additive(Parser *p) {
    ASTNode *left = parse_multiplicative(p);
    
    while (match_token(p, TOK_PLUS) || match_token(p, TOK_MINUS)) {
        TokenType op = previous(p)->type;
        ASTNode *node = create_node(NODE_BINARY_OP, previous(p)->line, previous(p)->column);
        node->value_str = str_dup(op == TOK_PLUS ? "+" : "-");
        add_child(node, left);
        add_child(node, parse_multiplicative(p));
        left = node;
    }
    
    return left;
}

static ASTNode *parse_comparison(Parser *p) {
    ASTNode *left = parse_additive(p);
    
    while (match_token(p, TOK_GT) || match_token(p, TOK_LT) ||
           match_token(p, TOK_GTE) || match_token(p, TOK_LTE) ||
           match_token(p, TOK_EQEQ) || match_token(p, TOK_NEQ)) {
        TokenType op = previous(p)->type;
        ASTNode *node = create_node(NODE_BINARY_OP, previous(p)->line, previous(p)->column);
        node->value_str = str_dup(op == TOK_GT ? ">" :
                                  op == TOK_LT ? "<" :
                                  op == TOK_GTE ? ">=" :
                                  op == TOK_LTE ? "<=" :
                                  op == TOK_EQEQ ? "==" : "!=");
        add_child(node, left);
        add_child(node, parse_additive(p));
        left = node;
    }
    
    return left;
}

static ASTNode *parse_range(Parser *p) {
    ASTNode *left = parse_comparison(p);
    
    if (match_token(p, TOK_RANGE)) {
        ASTNode *node = create_node(NODE_RANGE_OP, previous(p)->line, previous(p)->column);
        add_child(node, left);
        
        /* Parse (low, high) */
        if (consume(p, TOK_LPAREN, "Expected '(' after <<<")) {
            ASTNode *low = expression(p);
            if (low) add_child(node, low);
            consume(p, TOK_COMMA, "Expected ',' in range");
            ASTNode *high = expression(p);
            if (high) add_child(node, high);
            consume(p, TOK_RPAREN, "Expected ')' after range");
        }
        return node;
    }
    
    return left;
}

static ASTNode *parse_ternary(Parser *p) {
    ASTNode *cond = parse_range(p);
    
    if (match_token(p, TOK_QUESTION)) {
        ASTNode *node = create_node(NODE_IF_STMT, previous(p)->line, previous(p)->column);
        add_child(node, cond);
        add_child(node, expression(p));
        consume(p, TOK_COLON, "Expected ':' in ternary");
        add_child(node, expression(p));
        return node;
    }
    
    return cond;
}

static ASTNode *expression(Parser *p) {
    return parse_ternary(p);
}

static ASTNode *parse_block(Parser *p);

static ASTNode *parse_if_stmt(Parser *p) {
    Token *tok = previous(p);
    ASTNode *node = create_node(NODE_IF_STMT, tok->line, tok->column);
    
    consume(p, TOK_LPAREN, "Expected '(' after if");
    add_child(node, expression(p));
    consume(p, TOK_RPAREN, "Expected ')' after condition");
    
    add_child(node, parse_block(p));
    
    /* Check for elif or else */
    while (match_token(p, TOK_ELIF)) {
        ASTNode *elif_node = create_node(NODE_IF_STMT, previous(p)->line, previous(p)->column);
        consume(p, TOK_LPAREN, "Expected '(' after elif");
        add_child(elif_node, expression(p));
        consume(p, TOK_RPAREN, "Expected ')' after condition");
        add_child(elif_node, parse_block(p));
        add_child(node, elif_node);
    }
    
    if (match_token(p, TOK_ELSE)) {
        add_child(node, parse_block(p));
    }
    
    return node;
}

static ASTNode *parse_loop_stmt(Parser *p) {
    Token *tok = previous(p);
    ASTNode *node = create_node(NODE_LOOP_STMT, tok->line, tok->column);
    
    consume(p, TOK_LPAREN, "Expected '(' after loop");
    add_child(node, expression(p));
    consume(p, TOK_RPAREN, "Expected ')' after condition");
    
    add_child(node, parse_block(p));
    return node;
}

static ASTNode *parse_do_loop_stmt(Parser *p) {
    Token *tok = previous(p);
    ASTNode *node = create_node(NODE_DO_LOOP_STMT, tok->line, tok->column);
    
    add_child(node, parse_block(p));
    
    consume(p, TOK_WHILE, "Expected 'while' after do loop body");
    consume(p, TOK_LPAREN, "Expected '(' after while");
    add_child(node, expression(p));
    consume(p, TOK_RPAREN, "Expected ')' after condition");
    
    return node;
}

static ASTNode *parse_forin_stmt(Parser *p) {
    Token *tok = previous(p);
    ASTNode *node = create_node(NODE_FORIN_STMT, tok->line, tok->column);
    
    consume(p, TOK_LPAREN, "Expected '(' after for-in");
    
    /* Parse variable declaration or identifier */
    if (match_token(p, TOK_VAR)) {
        Token *name_tok = current(p);
        if (check(p, TOK_IDENT)) {
            ASTNode *var_decl = create_node(NODE_VAR_DECL, name_tok->line, name_tok->column);
            var_decl->value_str = str_dup(name_tok->lexeme);
            advance(p);
            add_child(node, var_decl);
        }
    } else if (check(p, TOK_IDENT)) {
        ASTNode *ident = parse_identifier(p);
        if (ident) add_child(node, ident);
    }
    
    consume(p, TOK_IN, "Expected 'in' in for-in loop");
    add_child(node, expression(p));
    consume(p, TOK_RPAREN, "Expected ')' after for-in");
    
    add_child(node, parse_block(p));
    return node;
}

static ASTNode *parse_iterate_stmt(Parser *p) {
    Token *tok = previous(p);
    ASTNode *node = create_node(NODE_ITERATE_STMT, tok->line, tok->column);
    
    consume(p, TOK_LPAREN, "Expected '(' after iterate");
    add_child(node, expression(p));
    consume(p, TOK_RPAREN, "Expected ')' after iterate");
    
    add_child(node, parse_block(p));
    return node;
}

static ASTNode *parse_var_decl(Parser *p, bool is_fixed) {
    Token *tok = previous(p);
    NodeType type = is_fixed ? NODE_FIXED_DECL : NODE_VAR_DECL;
    ASTNode *node = create_node(type, tok->line, tok->column);
    node->is_fixed = is_fixed;
    
    Token *name_tok = current(p);
    if (!check(p, TOK_IDENT)) {
        print_token_error(p->comp, p->current, "Expected variable name");
        return NULL;
    }
    node->value_str = str_dup(name_tok->lexeme);
    advance(p);
    
    /* Type annotation */
    consume(p, TOK_COLON, "Expected ':' after variable name");
    Token *type_tok = current(p);
    if (check(p, TOK_IDENT)) {
        node->type_annotation = str_dup(type_tok->lexeme);
        advance(p);
    } else {
        print_token_error(p->comp, p->current, "Expected type name");
        return NULL;
    }
    
    /* Optional initialization */
    if (match_token(p, TOK_ASSIGN)) {
        add_child(node, expression(p));
    } else if (is_fixed) {
        print_token_error(p->comp, p->current, "fixed must be initialized");
    }
    
    return node;
}

static ASTNode *parse_function_def(Parser *p, bool is_async, bool is_public) {
    Token *tok = previous(p);
    ASTNode *node = create_node(NODE_FUNCTION_DEF, tok->line, tok->column);
    node->is_async = is_async;
    node->is_public = is_public;
    
    Token *name_tok = current(p);
    if (!check(p, TOK_IDENT)) {
        print_token_error(p->comp, p->current, "Expected function name");
        return NULL;
    }
    node->value_str = str_dup(name_tok->lexeme);
    advance(p);
    
    consume(p, TOK_LPAREN, "Expected '(' after function name");
    
    /* Parse parameters */
    while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
        Token *param_name_tok = current(p);
        if (check(p, TOK_IDENT)) {
            ASTNode *param = create_node(NODE_VAR_DECL, param_name_tok->line, param_name_tok->column);
            param->value_str = str_dup(param_name_tok->lexeme);
            advance(p);
            
            consume(p, TOK_COLON, "Expected ':' after parameter name");
            Token *type_tok = current(p);
            if (check(p, TOK_IDENT)) {
                param->type_annotation = str_dup(type_tok->lexeme);
                advance(p);
            }
            add_child(node, param);
        }
        if (!match_token(p, TOK_COMMA)) break;
    }
    
    consume(p, TOK_RPAREN, "Expected ')' after parameters");
    
    /* Optional return type */
    if (match_token(p, TOK_MINUS)) {
        if (match_token(p, TOK_GT)) {
            Token *ret_type_tok = current(p);
            if (check(p, TOK_IDENT)) {
                /* Store return type in value_str with special marker */
                node->type_annotation = str_dup(ret_type_tok->lexeme);
                advance(p);
            }
        }
    }
    
    add_child(node, parse_block(p));
    return node;
}

static ASTNode *parse_class_def(Parser *p, bool is_public) {
    Token *tok = previous(p);
    ASTNode *node = create_node(NODE_CLASS_DEF, tok->line, tok->column);
    node->is_public = is_public;
    
    Token *name_tok = current(p);
    if (!check(p, TOK_IDENT)) {
        print_token_error(p->comp, p->current, "Expected class name");
        return NULL;
    }
    node->value_str = str_dup(name_tok->lexeme);
    advance(p);
    
    add_child(node, parse_block(p));
    return node;
}

static ASTNode *parse_struct_def(Parser *p) {
    Token *tok = previous(p);
    ASTNode *node = create_node(NODE_STRUCT_DEF, tok->line, tok->column);
    
    Token *name_tok = current(p);
    if (!check(p, TOK_IDENT)) {
        print_token_error(p->comp, p->current, "Expected struct name");
        return NULL;
    }
    node->value_str = str_dup(name_tok->lexeme);
    advance(p);
    
    add_child(node, parse_block(p));
    return node;
}

static ASTNode *parse_enum_def(Parser *p) {
    Token *tok = previous(p);
    ASTNode *node = create_node(NODE_ENUM_DEF, tok->line, tok->column);
    
    Token *name_tok = current(p);
    if (!check(p, TOK_IDENT)) {
        print_token_error(p->comp, p->current, "Expected enum name");
        return NULL;
    }
    node->value_str = str_dup(name_tok->lexeme);
    advance(p);
    
    consume(p, TOK_LBRACE, "Expected '{' after enum name");
    
    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        Token *member_tok = current(p);
        if (check(p, TOK_IDENT)) {
            ASTNode *member = create_node(NODE_IDENTIFIER, member_tok->line, member_tok->column);
            member->value_str = str_dup(member_tok->lexeme);
            add_child(node, member);
            advance(p);
        }
        if (!match_token(p, TOK_COMMA)) break;
    }
    
    consume(p, TOK_RBRACE, "Expected '}' after enum members");
    return node;
}

static ASTNode *parse_import_stmt(Parser *p) {
    Token *tok = previous(p);
    ASTNode *node = create_node(NODE_IMPORT_STMT, tok->line, tok->column);
    
    /* Check for "import data from" syntax */
    if (check(p, TOK_IDENT) && strcmp(current(p)->lexeme, "data") == 0) {
        node->value_str = str_dup("data");
        advance(p);
        consume(p, TOK_FROM, "Expected 'from' after 'data'");
    }
    
    Token *path_tok = current(p);
    if (check(p, TOK_STRING)) {
        /* Remove quotes */
        int len = strlen(path_tok->lexeme);
        if (len >= 2) {
            node->value_str = (char *)malloc(len - 1);
            strncpy(node->value_str, path_tok->lexeme + 1, len - 2);
            node->value_str[len - 2] = '\0';
        }
        advance(p);
    }
    
    return node;
}

static ASTNode *parse_export_stmt(Parser *p) {
    Token *tok = previous(p);
    ASTNode *node = create_node(NODE_EXPORT_STMT, tok->line, tok->column);
    
    Token *ident_tok = current(p);
    if (check(p, TOK_IDENT)) {
        node->value_str = str_dup(ident_tok->lexeme);
        advance(p);
    }
    
    return node;
}

static ASTNode *parse_return_stmt(Parser *p) {
    Token *tok = previous(p);
    ASTNode *node = create_node(NODE_RETURN_STMT, tok->line, tok->column);
    
    if (!check(p, TOK_SEMICOLON) && !check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        add_child(node, expression(p));
    }
    
    return node;
}

ASTNode *parse_statement(Parser *p) {
    if (match_token(p, TOK_VAR)) {
        return parse_var_decl(p, false);
    }
    if (match_token(p, TOK_FIXED)) {
        return parse_var_decl(p, true);
    }
    if (match_token(p, TOK_IF)) {
        return parse_if_stmt(p);
    }
    if (match_token(p, TOK_LOOP)) {
        return parse_loop_stmt(p);
    }
    if (match_token(p, TOK_DO)) {
        return parse_do_loop_stmt(p);
    }
    if (match_token(p, TOK_FORIN)) {
        return parse_forin_stmt(p);
    }
    if (match_token(p, TOK_ITERATE)) {
        return parse_iterate_stmt(p);
    }
    if (match_token(p, TOK_FUN)) {
        return parse_function_def(p, false, false);
    }
    if (match_token(p, TOK_ASYNC)) {
        if (match_token(p, TOK_FUN)) {
            return parse_function_def(p, true, false);
        }
    }
    if (match_token(p, TOK_PUBLIC)) {
        if (match_token(p, TOK_CLASS)) {
            return parse_class_def(p, true);
        }
    }
    if (match_token(p, TOK_PRIVATE)) {
        if (match_token(p, TOK_CLASS)) {
            return parse_class_def(p, false);
        }
    }
    if (match_token(p, TOK_CLASS)) {
        return parse_class_def(p, false);
    }
    if (match_token(p, TOK_STRUCT)) {
        return parse_struct_def(p);
    }
    if (match_token(p, TOK_ENUM)) {
        return parse_enum_def(p);
    }
    if (match_token(p, TOK_IMPORT)) {
        return parse_import_stmt(p);
    }
    if (match_token(p, TOK_EXPORT)) {
        return parse_export_stmt(p);
    }
    if (match_token(p, TOK_RETURN)) {
        return parse_return_stmt(p);
    }
    
    /* Expression statement or assignment */
    ASTNode *expr = expression(p);
    if (expr) {
        if (match_token(p, TOK_ASSIGN)) {
            /* Assignment */
            ASTNode *assign = create_node(NODE_ASSIGNMENT, previous(p)->line, previous(p)->column);
            add_child(assign, expr);
            add_child(assign, expression(p));
            return assign;
        }
        /* Check for method calls like list.append() */
        if (expr->type == NODE_IDENTIFIER && match_token(p, TOK_DOT)) {
            Token *method_tok = current(p);
            if (check(p, TOK_IDENT)) {
                ASTNode *call = create_node(NODE_METHOD_CALL, method_tok->line, method_tok->column);
                call->value_str = str_dup(method_tok->lexeme);
                advance(p);
                add_child(call, expr);
                
                if (match_token(p, TOK_LPAREN)) {
                    while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
                        ASTNode *arg = expression(p);
                        if (arg) add_child(call, arg);
                        if (!match_token(p, TOK_COMMA)) break;
                    }
                    consume(p, TOK_RPAREN, "Expected ')'");
                }
                return call;
            }
        }
        return expr;
    }
    
    return NULL;
}

static ASTNode *parse_block(Parser *p) {
    Token *tok = current(p);
    ASTNode *block = create_node(NODE_BLOCK, tok->line, tok->column);
    
    consume(p, TOK_LBRACE, "Expected '{' to start block");
    
    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        ASTNode *stmt = parse_statement(p);
        if (stmt) add_child(block, stmt);
        if (match_token(p, TOK_SEMICOLON)) {
            /* Statement terminator - optional */
        }
    }
    
    consume(p, TOK_RBRACE, "Expected '}' to end block");
    return block;
}

ASTNode *parse(Compiler *comp) {
    Parser parser;
    parser.comp = comp;
    parser.current = 0;
    
    ASTNode *program = create_node(NODE_PROGRAM, 1, 1);
    
    while (!check(&parser, TOK_EOF)) {
        ASTNode *stmt = parse_statement(&parser);
        if (stmt) add_child(program, stmt);
        if (match_token(&parser, TOK_SEMICOLON)) {
            /* Optional semicolon */
        }
    }
    
    return program;
}
