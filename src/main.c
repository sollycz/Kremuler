#include "code_generator.h"
#include "parser.h"
#include "semantic.h"

int main(void)
{
    Parser p;
    AstNode* ast = NULL;

    ParseResult result = parser_init(&p);
    if (result != PARSE_OK) {
        return result; // 99
    }

    result = parse_program(&p, &ast);
    parser_destroy(&p); // always, even after an error
    if (result != PARSE_OK) {
        return result; // 1 / 2 / 99, ast stays NULL
    }

    stab* st = stab_new();
    semantic_init(st);
    sem_res_t r = semantic_analyse(ast);
    if (r != SEM_OK) {
        return r;
    }
    generate_code(ast, st);

    stab_free(st);
    ast_free(ast);
    return 0;
}