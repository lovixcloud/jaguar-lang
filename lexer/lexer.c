/* ============================================================================
 * Jaguar Compiler - Lexer Implementation
 * ========================================================================== */

#include "../include/jaguar.h"

static const char *keywords[] = {
    "var", "fixed", "fun", "async", "class", "public", "private",
    "enum", "struct", "vector", "matrix", "data", "list", "MixedList",
    "if", "elif", "else", "loop", "do", "while", "for-in", "iterate",
    "in", "return", "import", "from", "export", "live", "file", "dir",
    "true", "false", NULL
};

static TokenType keyword_type(const char *lexeme) {
    for (int i = 0; keywords[i] != NULL; i++) {
        if (strcmp(lexeme, keywords[i]) == 0) {
            switch (i) {
                case 0: return TOK_VAR;
                case 1: return TOK_FIXED;
                case 2: return TOK_FUN;
                case 3: return TOK_ASYNC;
                case 4: return TOK_CLASS;
                case 5: return TOK_PUBLIC;
                case 6: return TOK_PRIVATE;
                case 7: return TOK_ENUM;
                case 8: return TOK_STRUCT;
                case 9: return TOK_VECTOR;
                case 10: return TOK_MATRIX;
                case 11: return TOK_DATA;
                case 12: return TOK_LIST;
                case 13: return TOK_MIXEDLIST;
                case 14: return TOK_IF;
                case 15: return TOK_ELIF;
                case 16: return TOK_ELSE;
                case 17: return TOK_LOOP;
                case 18: return TOK_DO;
                case 19: return TOK_WHILE;
                case 20: return TOK_FORIN;
                case 21: return TOK_ITERATE;
                case 22: return TOK_IN;
                case 23: return TOK_RETURN;
                case 24: return TOK_IMPORT;
                case 25: return TOK_FROM;
                case 26: return TOK_EXPORT;
                case 27: return TOK_LIVE;
                case 28: return TOK_FILE;
                case 29: return TOK_DIR;
                case 30: return TOK_TRUE;
                case 31: return TOK_FALSE;
            }
        }
    }
    return TOK_IDENT;
}

Compiler *compiler_create(void) {
    Compiler *comp = (Compiler *)calloc(1, sizeof(Compiler));
    comp->token_capacity = 1024;
    comp->tokens = (Token *)calloc(comp->token_capacity, sizeof(Token));
    comp->line = 1;
    comp->column = 1;
    comp->symbols = (SymbolTable *)calloc(1, sizeof(SymbolTable));
    comp->symbols->capacity = 256;
    comp->symbols->symbols = (Symbol **)calloc(comp->symbols->capacity, sizeof(Symbol *));
    comp->import_files = (char **)calloc(JAG_MAX_IMPORTS, sizeof(char *));
    return comp;
}

void compiler_free(Compiler *comp) {
    if (!comp) return;
    free(comp->source);
    for (int i = 0; i < comp->token_count; i++) {
        free(comp->tokens[i].lexeme);
    }
    free(comp->tokens);
    /* Free symbols */
    for (int i = 0; i < comp->symbols->count; i++) {
        Symbol *sym = comp->symbols->symbols[i];
        free(sym->name);
        free(sym->type->name);
        if (sym->type->element_type) free(sym->type->element_type);
        free(sym->type);
        free(sym);
    }
    free(comp->symbols->symbols);
    free(comp->symbols);
    for (int i = 0; i < comp->import_count; i++) {
        free(comp->import_files[i]);
    }
    free(comp->import_files);
    free(comp);
}

static void add_token(Compiler *comp, TokenType type, const char *start, int len) {
    if (comp->token_count >= comp->token_capacity) {
        comp->token_capacity *= 2;
        comp->tokens = (Token *)realloc(comp->tokens, comp->token_capacity * sizeof(Token));
    }
    Token *tok = &comp->tokens[comp->token_count++];
    tok->type = type;
    tok->lexeme = (char *)malloc(len + 1);
    strncpy(tok->lexeme, start, len);
    tok->lexeme[len] = '\0';
    tok->line = comp->line;
    tok->column = comp->column - len;
}

static char peek(Compiler *comp) {
    if (comp->pos >= comp->source_len) return '\0';
    return comp->source[comp->pos];
}

static char advance(Compiler *comp) {
    char c = peek(comp);
    comp->pos++;
    if (c == '\n') {
        comp->line++;
        comp->column = 1;
    } else {
        comp->column++;
    }
    return c;
}

static char peek_next(Compiler *comp) {
    if (comp->pos + 1 >= comp->source_len) return '\0';
    return comp->source[comp->pos + 1];
}

static bool match(Compiler *comp, char expected) {
    if (peek(comp) != expected) return false;
    advance(comp);
    return true;
}

static void skip_whitespace(Compiler *comp) {
    for (;;) {
        char c = peek(comp);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(comp);
        } else if (c == '/' && peek_next(comp) == '/') {
            /* Line comment */
            while (peek(comp) != '\n' && peek(comp) != '\0') {
                advance(comp);
            }
        } else if (c == '/' && peek_next(comp) == '*') {
            /* Block comment */
            advance(comp);
            advance(comp);
            while (!(peek(comp) == '*' && peek_next(comp) == '/') && peek(comp) != '\0') {
                advance(comp);
            }
            if (peek(comp) != '\0') {
                advance(comp);
                advance(comp);
            }
        } else {
            break;
        }
    }
}

static void scan_string(Compiler *comp, char quote) {
    int start = comp->pos;
    while (peek(comp) != quote && peek(comp) != '\0') {
        if (peek(comp) == '\\' && peek_next(comp) != '\0') {
            advance(comp);
        }
        advance(comp);
    }
    if (peek(comp) == '\0') {
        /* Unterminated string - error handled in parser */
    }
    advance(comp); /* closing quote */
    add_token(comp, TOK_STRING, &comp->source[start - 1], comp->pos - start + 1);
}

static void scan_number(Compiler *comp) {
    int start = comp->pos - 1;
    bool is_decimal = false;
    bool is_scifi = false;
    
    /* Scan integer part */
    while (isdigit(peek(comp))) {
        advance(comp);
    }
    
    /* Check for decimal point */
    if (peek(comp) == '.' && isdigit(peek_next(comp))) {
        is_decimal = true;
        advance(comp);
        while (isdigit(peek(comp))) {
            advance(comp);
        }
    }
    
    /* Check for scientific notation */
    if (toupper(peek(comp)) == 'E') {
        is_scifi = true;
        advance(comp);
        if (peek(comp) == '+' || peek(comp) == '-') {
            advance(comp);
        }
        while (isdigit(peek(comp))) {
            advance(comp);
        }
    }
    
    int len = comp->pos - start;
    if (is_scifi) {
        add_token(comp, TOK_SCIFI, &comp->source[start], len);
    } else if (is_decimal) {
        add_token(comp, TOK_DECIMAL, &comp->source[start], len);
    } else {
        add_token(comp, TOK_NUMBER, &comp->source[start], len);
    }
}

static void scan_identifier(Compiler *comp) {
    int start = comp->pos - 1;
    while (isalnum(peek(comp)) || peek(comp) == '_') {
        advance(comp);
    }
    int len = comp->pos - start;
    TokenType type = keyword_type(&comp->source[start]);
    add_token(comp, type, &comp->source[start], len);
}

int lex(Compiler *comp, const char *source) {
    comp->source = str_dup(source);
    comp->source_len = strlen(source);
    comp->pos = 0;
    comp->line = 1;
    comp->column = 1;
    comp->token_count = 0;
    comp->is_first_statement = true;
    
    while (comp->pos < comp->source_len) {
        skip_whitespace(comp);
        
        if (comp->pos >= comp->source_len) break;
        
        char c = advance(comp);
        
        switch (c) {
            case '(': add_token(comp, TOK_LPAREN, "(", 1); break;
            case ')': add_token(comp, TOK_RPAREN, ")", 1); break;
            case '{': add_token(comp, TOK_LBRACE, "{", 1); break;
            case '}': add_token(comp, TOK_RBRACE, "}", 1); break;
            case '[': add_token(comp, TOK_LBRACKET, "[", 1); break;
            case ']': add_token(comp, TOK_RBRACKET, "]", 1); break;
            case ',': add_token(comp, TOK_COMMA, ",", 1); break;
            case '.': add_token(comp, TOK_DOT, ".", 1); break;
            case ';': add_token(comp, TOK_SEMICOLON, ";", 1); break;
            case ':': add_token(comp, TOK_COLON, ":", 1); break;
            case '?': add_token(comp, TOK_QUESTION, "?", 1); break;
            
            case '+':
                if (match(comp, '=')) {
                    add_token(comp, TOK_PLUS_EQ, "+=", 2);
                } else {
                    add_token(comp, TOK_PLUS, "+", 1);
                }
                break;
                
            case '-':
                if (match(comp, '>')) {
                    /* Arrow for function return type - treat as special */
                    add_token(comp, TOK_MINUS, "->", 2);
                } else if (match(comp, '=')) {
                    add_token(comp, TOK_MINUS_EQ, "-=", 2);
                } else {
                    add_token(comp, TOK_MINUS, "-", 1);
                }
                break;
                
            case '*':
                if (match(comp, '*')) {
                    if (match(comp, '=')) {
                        add_token(comp, TOK_STARSTAR_EQ, "**=", 3);
                    } else {
                        add_token(comp, TOK_STARSTAR, "**", 2);
                    }
                } else if (match(comp, '=')) {
                    add_token(comp, TOK_STAR_EQ, "*=", 2);
                } else {
                    add_token(comp, TOK_STAR, "*", 1);
                }
                break;
                
            case '/':
                if (match(comp, '=')) {
                    add_token(comp, TOK_SLASH_EQ, "/=", 2);
                } else {
                    add_token(comp, TOK_SLASH, "/", 1);
                }
                break;
                
            case '%':
                if (match(comp, '=')) {
                    add_token(comp, TOK_PERCENT_EQ, "%=", 2);
                } else {
                    add_token(comp, TOK_PERCENT, "%", 1);
                }
                break;
                
            case '>':
                if (match(comp, '>')) {
                    add_token(comp, TOK_SHR, ">>", 2);
                } else if (match(comp, '=')) {
                    add_token(comp, TOK_GTE, ">=", 2);
                } else {
                    add_token(comp, TOK_GT, ">", 1);
                }
                break;
                
            case '<':
                if (match(comp, '<')) {
                    if (match(comp, '<')) {
                        /* <<< range operator */
                        add_token(comp, TOK_RANGE, "<<<", 3);
                    } else {
                        add_token(comp, TOK_SHL, "<<", 2);
                    }
                } else if (match(comp, '=')) {
                    add_token(comp, TOK_LTE, "<=", 2);
                } else {
                    add_token(comp, TOK_LT, "<", 1);
                }
                break;
                
            case '=':
                if (match(comp, '=')) {
                    add_token(comp, TOK_EQEQ, "==", 2);
                } else {
                    add_token(comp, TOK_ASSIGN, "=", 1);
                }
                break;
                
            case '!':
                if (match(comp, '=')) {
                    add_token(comp, TOK_NEQ, "!=", 2);
                } else {
                    /* Treat ! as unary not */
                    add_token(comp, TOK_IDENT, "!", 1);
                }
                break;
                
            case '"':
            case '\'':
                scan_string(comp, c);
                break;
                
            default:
                if (isdigit(c)) {
                    scan_number(comp);
                } else if (isalpha(c) || c == '_') {
                    scan_identifier(comp);
                } else {
                    /* Unknown character - skip */
                }
                break;
        }
        
        /* Track if this is the first statement for live mode detection */
        if (comp->is_first_statement && comp->token_count > 0) {
            comp->is_first_statement = false;
        }
    }
    
    /* Add EOF token */
    add_token(comp, TOK_EOF, "", 0);
    comp->tokens[comp->token_count - 1].line = comp->line;
    comp->tokens[comp->token_count - 1].column = comp->column;
    
    return 0;
}

void print_token_error(Compiler *comp, int token_idx, const char *msg) {
    if (token_idx < 0 || token_idx >= comp->token_count) return;
    Token *tok = &comp->tokens[token_idx];
    fprintf(stderr, "Error at %s:%d:%d: %s (token: '%s')\n",
            comp->current_file ? comp->current_file : "<stdin>",
            tok->line, tok->column, msg, tok->lexeme);
    comp->has_errors = true;
}

char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *buffer = (char *)malloc(size + 1);
    if (!buffer) {
        fclose(f);
        return NULL;
    }
    
    size_t read = fread(buffer, 1, size, f);
    buffer[read] = '\0';
    fclose(f);
    
    return buffer;
}

char *str_dup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *dup = (char *)malloc(len + 1);
    if (dup) {
        memcpy(dup, s, len + 1);
    }
    return dup;
}

void error_at(const char *file, int line, int col, const char *msg) {
    fprintf(stderr, "Error at %s:%d:%d: %s\n", file, line, col, msg);
}
