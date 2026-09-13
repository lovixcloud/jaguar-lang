/* ============================================================================
 * Jaguar Compiler - Main CLI and Live Mode Implementation
 * ========================================================================== */

#include "include/jaguar.h"

#ifdef __linux__
#include <sys/inotify.h>
#endif

static void print_version(void) {
    printf("jag version 0.1.0\n");
    printf("Jaguar Programming Language Compiler\n");
}

static void print_help(void) {
    printf("Usage: jag [options] <file.jag>\n\n");
    printf("Commands:\n");
    printf("  jag <file.jag>              Compile and run once (interpreter)\n");
    printf("  jag run <file.jag>          Same as above\n");
    printf("  jag -live=1 <file.jag>      Live-reload mode\n");
    printf("  jag build <file.jag> [-o out]  Native compile to executable\n");
    printf("  jag check <file.jag>        Typecheck only, no execution\n");
    printf("  jag --version               Print version\n");
    printf("  jag --help                  Print this help\n\n");
    printf("Options:\n");
    printf("  -live=1                     Enable live reload mode\n");
    printf("  -o <output>                 Output file for build command\n");
}

static int run_file(const char *filename, bool live_mode) {
    char *source = read_file(filename);
    if (!source) {
        fprintf(stderr, "Error: Cannot read file '%s'\n", filename);
        return 1;
    }
    
    Compiler *comp = compiler_create();
    comp->current_file = (char *)filename;
    comp->live_mode = live_mode;
    
    /* Lex */
    if (lex(comp, source) != 0) {
        fprintf(stderr, "Lexical error in %s\n", filename);
        compiler_free(comp);
        free(source);
        return 1;
    }
    
    /* Check for live = "1"; as first statement */
    if (!live_mode && comp->token_count >= 5) {
        Token *t0 = &comp->tokens[0];
        Token *t1 = &comp->tokens[1];
        Token *t2 = &comp->tokens[2];
        Token *t3 = &comp->tokens[3];
        Token *t4 = &comp->tokens[4];
        
        if (t0->type == TOK_LIVE && t1->type == TOK_ASSIGN &&
            t2->type == TOK_STRING && strcmp(t2->lexeme, "\"1\"") == 0 &&
            t3->type == TOK_SEMICOLON) {
            live_mode = true;
        }
    }
    
    /* Parse */
    ASTNode *ast = parse(comp);
    if (!ast || comp->has_errors) {
        fprintf(stderr, "Parse error in %s\n", filename);
        compiler_free(comp);
        free_ast(ast);
        free(source);
        return 1;
    }
    
    /* Typecheck */
    if (!typecheck(comp, ast)) {
        fprintf(stderr, "Type check failed in %s\n", filename);
        compiler_free(comp);
        free_ast(ast);
        free(source);
        return 1;
    }
    
    /* Execute with VM backend */
    Bytecode *bc = bytecode_create();
    compile_to_bytecode(ast, bc);
    
    VM *vm = vm_create();
    vm->live_mode = live_mode;
    vm_execute(vm, bc);
    
    vm_free(vm);
    bytecode_free(bc);
    free_ast(ast);
    compiler_free(comp);
    free(source);
    
    return 0;
}

static int check_file(const char *filename) {
    char *source = read_file(filename);
    if (!source) {
        fprintf(stderr, "Error: Cannot read file '%s'\n", filename);
        return 1;
    }
    
    Compiler *comp = compiler_create();
    comp->current_file = (char *)filename;
    
    /* Lex */
    if (lex(comp, source) != 0) {
        fprintf(stderr, "Lexical error in %s\n", filename);
        compiler_free(comp);
        free(source);
        return 1;
    }
    
    /* Parse */
    ASTNode *ast = parse(comp);
    if (!ast || comp->has_errors) {
        fprintf(stderr, "Parse error in %s\n", filename);
        compiler_free(comp);
        free_ast(ast);
        free(source);
        return 1;
    }
    
    /* Typecheck */
    if (!typecheck(comp, ast)) {
        fprintf(stderr, "Type check failed in %s\n", filename);
        compiler_free(comp);
        free_ast(ast);
        free(source);
        return 1;
    }
    
    printf("OK: %s passed lex, parse, and typecheck\n", filename);
    
    free_ast(ast);
    compiler_free(comp);
    free(source);
    
    return 0;
}

static int build_file(const char *filename, const char *output) {
    char *source = read_file(filename);
    if (!source) {
        fprintf(stderr, "Error: Cannot read file '%s'\n", filename);
        return 1;
    }
    
    Compiler *comp = compiler_create();
    comp->current_file = (char *)filename;
    
    /* Lex */
    if (lex(comp, source) != 0) {
        fprintf(stderr, "Lexical error in %s\n", filename);
        compiler_free(comp);
        free(source);
        return 1;
    }
    
    /* Parse */
    ASTNode *ast = parse(comp);
    if (!ast || comp->has_errors) {
        fprintf(stderr, "Parse error in %s\n", filename);
        compiler_free(comp);
        free_ast(ast);
        free(source);
        return 1;
    }
    
    /* Typecheck */
    if (!typecheck(comp, ast)) {
        fprintf(stderr, "Type check failed in %s\n", filename);
        compiler_free(comp);
        free_ast(ast);
        free(source);
        return 1;
    }
    
    /* Generate C code */
    char temp_c[JAG_MAX_PATH];
    snprintf(temp_c, sizeof(temp_c), "/tmp/jag_%d.c", getpid());
    
    if (compile_to_c(ast, temp_c) != 0) {
        fprintf(stderr, "Code generation failed\n");
        compiler_free(comp);
        free_ast(ast);
        free(source);
        return 1;
    }
    
    /* Compile with host C compiler */
    char cmd[JAG_MAX_PATH * 2];
    const char *cc = getenv("CC");
    if (!cc) cc = "gcc";
    
    snprintf(cmd, sizeof(cmd), "%s -o %s %s -lm", cc, output, temp_c);
    int result = system(cmd);
    
    /* Clean up temp file */
    remove(temp_c);
    
    if (result == 0) {
        printf("Built: %s -> %s\n", filename, output);
    } else {
        fprintf(stderr, "C compilation failed\n");
    }
    
    free_ast(ast);
    compiler_free(comp);
    free(source);
    
    return (result == 0) ? 0 : 1;
}

#ifdef __linux__
int run_live_mode_linux(const char *filename) {
    int fd = inotify_init();
    if (fd < 0) {
        fprintf(stderr, "Error: inotify not available\n");
        return run_file(filename, true);
    }
    
    int wd = inotify_add_watch(fd, filename, IN_MODIFY | IN_CLOSE_WRITE);
    if (wd < 0) {
        fprintf(stderr, "Error: Cannot watch file '%s'\n", filename);
        close(fd);
        return run_file(filename, true);
    }
    
    printf("Live mode enabled for %s (Ctrl+C to stop)\n", filename);
    
    /* Initial run */
    run_file(filename, true);
    
    /* Watch for changes */
    char buffer[4096] __attribute__((aligned(8)));
    
    while (1) {
        int len = read(fd, buffer, sizeof(buffer));
        if (len < 0) break;
        
        int i = 0;
        while (i < len) {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];
            
            if (event->mask & (IN_MODIFY | IN_CLOSE_WRITE)) {
                printf("\n--- File changed, reloading ---\n");
                
                /* Small delay to let file write complete */
                usleep(50000);
                
                run_file(filename, true);
            }
            
            i += sizeof(struct inotify_event) + event->len;
        }
    }
    
    inotify_rm_watch(fd, wd);
    close(fd);
    
    return 0;
}
#else
int run_live_mode_linux(const char *filename) {
    /* Fallback: just run once without watching */
    printf("Live mode: file watching not available on this platform\n");
    return run_file(filename, true);
}
#endif

int run_live_mode(const char *filename) {
    return run_live_mode_linux(filename);
}

int cli_main(int argc, char **argv) {
    if (argc < 2) {
        print_help();
        return 1;
    }
    
    /* Handle flags */
    bool live_mode = false;
    const char *output_file = NULL;
    const char *command = NULL;
    const char *filename = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        }
        if (strncmp(argv[i], "-live=", 6) == 0) {
            live_mode = (strcmp(argv[i] + 6, "1") == 0);
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (strcmp(argv[i], "run") == 0 ||
                   strcmp(argv[i], "build") == 0 ||
                   strcmp(argv[i], "check") == 0) {
            command = argv[i];
        } else if (argv[i][0] != '-') {
            filename = argv[i];
        }
    }
    
    if (!filename) {
        fprintf(stderr, "Error: No input file specified\n");
        print_help();
        return 1;
    }
    
    /* Check file extension */
    size_t len = strlen(filename);
    if (len < 4 || (strcmp(filename + len - 4, ".jag") != 0 &&
                    strcmp(filename + len - 3, ".ja") != 0)) {
        fprintf(stderr, "Warning: Expected .jag or .ja file extension\n");
    }
    
    /* Execute command */
    if (command && strcmp(command, "build") == 0) {
        const char *out = output_file ? output_file : "./a.out";
        return build_file(filename, out);
    }
    
    if (command && strcmp(command, "check") == 0) {
        return check_file(filename);
    }
    
    if (live_mode || (command && strcmp(command, "run") == 0)) {
        if (live_mode) {
            return run_live_mode(filename);
        } else {
            return run_file(filename, false);
        }
    }
    
    /* Default: run once */
    return run_file(filename, false);
}

int main(int argc, char **argv) {
    return cli_main(argc, argv);
}
