/**
 * @file parser.c
 * @brief Syntax analyser
 * @author Oleksandr Teslja (xtesljo00)
 */

#include "parser.h"
#include "precedence_parser.h"
#include "scanner.h"
#include <stdlib.h>
#include <string.h>

// frees the string of a token if it still has one
static void token_free(Token* token)
{
    if (token->type == TOK_ID || token->type == TOK_STRING_LITERAL) {
        free(token->data.str_val);
        token->data.str_val = NULL;
    }
}

// adds an indentation level onto the top of the stack
static ParseResult indent_push(Parser* p, unsigned int indent)
{
    // add room for one more indent level
    unsigned int* stack = realloc(p->indent_stack, (p->indent_count + 1) * sizeof(unsigned int));

    if (stack == NULL) {
        return PARSE_INTERNAL_ERROR;
    }

    stack[p->indent_count] = indent;
    p->indent_stack = stack;
    p->indent_count++;

    return PARSE_OK;
}

// adds the size of one parameter list to the end of a sizes array
static ParseResult size_list_push(size_t** sizes, size_t* count, size_t size)
{
    size_t* new_sizes = realloc(*sizes, (*count + 1) * sizeof(size_t));

    if (new_sizes == NULL) {
        return PARSE_INTERNAL_ERROR;
    }

    new_sizes[*count] = size;
    *sizes = new_sizes;
    (*count)++;

    return PARSE_OK;
}

ParseResult parser_init(Parser* p)
{
    *p = (Parser) { 0 };
    p->at_line_start = true;
    return indent_push(p, 0);
}

void parser_destroy(Parser* p)
{
    token_free(&p->current);

    if (p->has_lookahead) {
        token_free(&p->lookahead);
    }

    free(p->indent_stack);
    *p = (Parser) { 0 };

    // free scanner interpolation states, even after parsing errors
    interpolation_free();
}

ParseResult next_token(Parser* p)
{
    // remembering the type of the previous token, so a statement that ends with a block (DEDENT) does not need
    // an EOL after it
    p->previous_type = p->current.type;
    token_free(&p->current);

    // if we have a stored lookahead token, use that instead of asking the scanner for a new one
    if (p->has_lookahead) {
        p->current = p->lookahead;
        p->has_lookahead = false;
    } else {
        ParseResult result = (ParseResult)scanner_get_token(&p->current);
        if (result != PARSE_OK) {
            return result;
        }
    }

    if (p->current.type == TOK_EOL) {
        p->at_line_start = true;
        return PARSE_OK;
    }

    if (p->current.type == TOK_EOF) {
        // EOL token needs to be filled (if missing) before closing out the file with EOF
        if (!p->at_line_start) {
            p->lookahead = p->current;
            p->has_lookahead = true;
            p->current = (Token) { .type = TOK_EOL, .line = p->lookahead.line };
            p->at_line_start = true;
            return PARSE_OK;
        }

        // setting indent lvl to 0 for EOF will cause the parser to fill the remaining DEDENTs
        p->current.indent = 0;
    }

    if (!p->at_line_start && p->current.lf_before == true && p->current.indent <= p->indent_stack[p->indent_count - 1]) {
        return PARSE_SYNTAX_ERROR;
    }

    if (!p->at_line_start) {
        return PARSE_OK;
    }

    // compare indentation level of the current token with the top of the stack
    unsigned int top = p->indent_stack[p->indent_count - 1];

    if (p->current.indent > top) {
        p->at_line_start = false; // INDENTs only come one at a time, so we toggle this off

        ParseResult result = indent_push(p, p->current.indent);
        if (result != PARSE_OK) {
            return result;
        }

        p->current.lf_before = false;
        // store the real token as lookahead and return an INDENT token
        p->lookahead = p->current;
        p->has_lookahead = true;

        p->current = (Token) { .type = TOK_INDENT, .line = p->lookahead.line, .indent = p->lookahead.indent };
        return PARSE_OK;
    }

    if (p->current.indent < top) {
        // DEDENTs may come more than one at a time, so we need to handle it with a counter
        p->indent_count--;

        // indentation between two open blocks
        if (p->indent_stack[p->indent_count - 1] < p->current.indent) {
            return PARSE_SYNTAX_ERROR;
        }

        // store the real token as lookahead and return a DEDENT token
        p->lookahead = p->current;
        p->has_lookahead = true;

        p->current = (Token) { .type = TOK_DEDENT, .line = p->lookahead.line, .indent = p->lookahead.indent };
        return PARSE_OK;
    }

    // same indentation as the current block
    p->at_line_start = false;
    return PARSE_OK;
}

ParseResult lookahead_token(Parser* p, TokenType* type)
{
    if (!p->has_lookahead) {
        ParseResult result = (ParseResult)scanner_get_token(&p->lookahead);

        if (result != PARSE_OK) {
            return result;
        }

        p->has_lookahead = true;
    }

    *type = p->lookahead.type;
    return PARSE_OK;
}

// checks that the current token has the given type and moves to the next one
static ParseResult expect(Parser* p, TokenType type)
{
    if (p->current.type != type) {
        return PARSE_SYNTAX_ERROR;
    }
    return next_token(p);
}

// declaration
static ParseResult parse_block(Parser* p, AstNode** out);
static ParseResult parse_if(Parser* p, AstNode** out);
static ParseResult parse_statement_simple(Parser* p, AstNode** out);

// parses an if used as a value, its else branch is required (CONTROL)
ParseResult parse_if_expression(Parser* p, AstNode** out)
{
    AstNode* node = NULL;
    ParseResult result = parse_if(p, &node);
    if (result != PARSE_OK) {
        return result;
    }

    // every if in an "else if" chain needs its own else
    AstNode* current = node;
    while (current != NULL) {
        AstNode* else_block = current->as.if_statement.else_block;
        if (else_block == NULL) {
            ast_free(node);
            return PARSE_SYNTAX_ERROR;
        }

        // "else if" is stored as a block with the nested if as its only statement
        AstNodeList* statements = &else_block->as.block.statements;
        if (statements->count == 1 && statements->items[0]->node_kind == AST_IF) {
            current = statements->items[0];
        } else {
            current = NULL;
        }
    }

    ast_mark_if_expression(node);

    *out = node;
    return PARSE_OK;
}

// parses an if expression or leaves it as an expression (CONTROL)
// <value> -> <if_expression> | <expression>
// <if_expression> -> TOK_KW_IF <expression> TOK_KW_THEN <branch> TOK_KW_ELSE <else_expression>
// <else_expression> -> <if_expression> | <branch>
static ParseResult parse_value(Parser* p, AstNode** out)
{
    if (p->current.type == TOK_KW_IF) {
        return parse_if_expression(p, out);
    }

    return parse_expression(p, out);
}

// <type> -> TOK_KW_INT | TOK_KW_STRING | TOK_KW_UNIT | TOK_KW_DOUBLE | TOK_KW_BOOLEAN
static ParseResult parse_type(Parser* p, DataType* type)
{
    switch (p->current.type) {
    case TOK_KW_INT:
        *type = TYPE_INT;
        break;
    case TOK_KW_STRING:
        *type = TYPE_STRING;
        break;
    case TOK_KW_UNIT:
        *type = TYPE_UNIT;
        break;
    case TOK_KW_DOUBLE:
        *type = TYPE_DOUBLE;
        break;
    case TOK_KW_BOOLEAN:
        *type = TYPE_BOOL;
        break;
    default:
        return PARSE_SYNTAX_ERROR;
    }

    return next_token(p);
}

// <optional_type> -> TOK_COLON <type> | epsilon
static ParseResult parse_optional_type(Parser* p, DataType* type)
{
    if (p->current.type == TOK_COLON) {
        ParseResult result;

        result = expect(p, TOK_COLON);
        if (result != PARSE_OK) {
            return result;
        }

        return parse_type(p, type);
    } else {
        *type = TYPE_UNKNOWN;
        return PARSE_OK;
    }
}

// <parameter> -> TOK_ID TOK_COLON <type>
static ParseResult parse_parameter(Parser* p, AstNode** out)
{
    // TOK_ID
    if (p->current.type != TOK_ID) {
        return PARSE_SYNTAX_ERROR;
    }

    AstNode* node = ast_new(AST_PARAMETER, p->current.line);
    if (node == NULL) {
        return PARSE_INTERNAL_ERROR;
    }

    node->as.parameter.name = p->current.data.str_val;
    p->current.data.str_val = NULL;

    ParseResult result;
    result = next_token(p);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    // TOK_COLON
    result = expect(p, TOK_COLON);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    // <type>
    result = parse_type(p, &node->as.parameter.type);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    (*out) = node;
    return PARSE_OK;
}

// creates a deep copy of a parameter node, so every extension method owns its own receiver
// (returns NULL on failure)
static AstNode* parameter_copy(const AstNode* parameter)
{
    AstNode* copy = ast_new(AST_PARAMETER, parameter->line);
    if (copy == NULL) {
        return NULL;
    }

    size_t length = strlen(parameter->as.parameter.name) + 1;
    copy->as.parameter.name = malloc(length);
    if (copy->as.parameter.name == NULL) {
        ast_free(copy);
        return NULL;
    }

    memcpy(copy->as.parameter.name, parameter->as.parameter.name, length);
    copy->as.parameter.type = parameter->as.parameter.type;

    return copy;
}

// <parameters> -> <parameter> <parameter_next> | epsilon
// <parameter_next> -> TOK_COMMA <parameter> <parameter_next> | epsilon
static ParseResult parse_parameters(Parser* p, AstNodeList* parameters)
{
    // epsilon
    if (p->current.type != TOK_ID) {
        return PARSE_OK;
    }

    // parameter(s)
    while (true) {
        AstNode* parameter = NULL;
        ParseResult result;

        result = parse_parameter(p, &parameter);
        if (result != PARSE_OK) {
            return result;
        }

        bool successful_push;
        successful_push = ast_list_push(parameters, parameter);

        if (successful_push == false) {
            ast_free(parameter);
            return PARSE_INTERNAL_ERROR;
        }

        // TOK_COMMA
        if (p->current.type != TOK_COMMA) {
            break;
        }

        result = next_token(p);
        if (result != PARSE_OK) {
            return result;
        }

        // allow a trailing comma
        if (p->current.type == TOK_RIGHT_PARENTHESIS && p->current.lf_before == true) {
            break;
        }
    }

    return PARSE_OK;
}

// <variable> -> <variable_type> TOK_ID <optional_type> TOK_ASSIGN <value>
// <variable_type> -> TOK_KW_VAR | TOK_KW_VAL
static ParseResult parse_variable(Parser* p, AstNode** out)
{
    ParseResult result;

    AstNode* node = ast_new(AST_VARIABLE_DEFINITION, p->current.line);
    if (node == NULL) {
        return PARSE_INTERNAL_ERROR;
    }

    // val or var?
    bool is_mutable = (p->current.type == TOK_KW_VAR);
    node->as.variable_definition.is_mutable = is_mutable;
    result = next_token(p);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    // TOK_ID
    if (p->current.type != TOK_ID) {
        ast_free(node);
        return PARSE_SYNTAX_ERROR;
    }
    node->as.variable_definition.name = p->current.data.str_val;
    p->current.data.str_val = NULL;
    result = next_token(p);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    // <optional_type>
    result = parse_optional_type(p, &node->as.variable_definition.declared_type);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    // TOK_ASSIGN
    result = expect(p, TOK_ASSIGN);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    // <expression>
    result = parse_value(p, &node->as.variable_definition.value);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    *out = node;
    return PARSE_OK;
}

// <assign> -> TOK_ID TOK_ASSIGN <value>
static ParseResult parse_assign(Parser* p, AstNode** out)
{
    ParseResult result;

    AstNode* node = ast_new(AST_ASSIGN, p->current.line);
    if (node == NULL) {
        return PARSE_INTERNAL_ERROR;
    }

    // TOK_ID
    if (p->current.type != TOK_ID) {
        ast_free(node);
        return PARSE_SYNTAX_ERROR;
    }
    node->as.assign.name = p->current.data.str_val;
    p->current.data.str_val = NULL;
    result = next_token(p);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    // TOK_ASSIGN
    result = expect(p, TOK_ASSIGN);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    // <expression>
    result = parse_value(p, &node->as.assign.value);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    *out = node;
    return PARSE_OK;
}

// <return> -> TOK_KW_RETURN <optional_value>
// <optional_value> -> <value> | epsilon
static ParseResult parse_return(Parser* p, AstNode** out)
{
    ParseResult result;
    result = expect(p, TOK_KW_RETURN);
    if (result != PARSE_OK) {
        return result;
    }

    AstNode* node = ast_new(AST_RETURN, p->current.line);
    if (node == NULL) {
        return PARSE_INTERNAL_ERROR;
    }

    if (p->current.type == TOK_EOL || p->current.type == TOK_SEMICOLON || p->current.type == TOK_KW_ELSE) {
        node->as.return_statement.value = NULL;
    } else {
        result = parse_value(p, &node->as.return_statement.value);
        if (result != PARSE_OK) {
            ast_free(node);
            return result;
        }
    }

    *out = node;
    return PARSE_OK;
}

// <expression> => [PRECEDENCE PARSER]
static ParseResult parse_expression_statement(Parser* p, AstNode** out)
{
    ParseResult result;
    AstNode* node = ast_new(AST_EXPRESSION_STATEMENT, p->current.line);
    if (node == NULL) {
        return PARSE_INTERNAL_ERROR;
    }

    result = parse_value(p, &node->as.expression_statement.expression);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    *out = node;
    return PARSE_OK;
}

// <while> -> TOK_KW_WHILE <expression> TOK_KW_DO TOK_EOL <block>
static ParseResult parse_while(Parser* p, AstNode** out)
{
    ParseResult result;

    AstNode* node = ast_new(AST_WHILE, p->current.line);
    if (node == NULL) {
        return PARSE_INTERNAL_ERROR;
    }

    // TOK_KW_WHILE
    result = expect(p, TOK_KW_WHILE);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    // <expression>
    result = parse_expression(p, &node->as.while_statement.condition);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    // TOK_KW_DO
    result = expect(p, TOK_KW_DO);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    // TOK_EOL
    result = expect(p, TOK_EOL);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    // <block>
    result = parse_block(p, &node->as.while_statement.block);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    *out = node;
    return PARSE_OK;
}

// parses the body of an if branch: an indented block, or one statement on the same line (CONTROL)
// <branch> -> TOK_EOL <block> | <statement_simple>
static ParseResult parse_branch(Parser* p, AstNode** out)
{
    ParseResult result;

    // TOK_EOL <block>
    if (p->current.type == TOK_EOL) {
        result = next_token(p);
        if (result != PARSE_OK) {
            return result;
        }

        return parse_block(p, out);
    }

    // one statement on the same line, wrapped into a block
    AstNode* block = ast_new(AST_BLOCK, p->current.line);
    if (block == NULL) {
        return PARSE_INTERNAL_ERROR;
    }

    AstNode* statement = NULL;
    result = parse_statement_simple(p, &statement);
    if (result != PARSE_OK) {
        ast_free(block);
        return result;
    }

    if (ast_list_push(&block->as.block.statements, statement) == false) {
        ast_free(statement);
        ast_free(block);
        return PARSE_INTERNAL_ERROR;
    }

    *out = block;
    return PARSE_OK;
}

// <else_next> -> <branch> | <if>
static ParseResult parse_else_next(Parser* p, AstNode** out)
{
    ParseResult result;

    if (p->current.type != TOK_KW_IF) {
        return parse_branch(p, out);
    }

    AstNode* block = ast_new(AST_BLOCK, p->current.line);
    if (block == NULL) {
        return PARSE_INTERNAL_ERROR;
    }

    AstNode* nested = NULL;
    result = parse_if(p, &nested);
    if (result != PARSE_OK) {
        ast_free(block);
        return result;
    }

    bool successful_push;
    successful_push = ast_list_push(&block->as.block.statements, nested);
    if (successful_push == false) {
        ast_free(nested);
        ast_free(block);
        return PARSE_INTERNAL_ERROR;
    }

    *out = block;
    return PARSE_OK;
}

// <else> -> TOK_KW_ELSE <else_next> | epsilon
static ParseResult parse_else(Parser* p, AstNode** out)
{
    ParseResult result;

    if (p->current.type == TOK_KW_ELSE) {
        result = next_token(p);
        if (result != PARSE_OK) {
            return result;
        }

        return parse_else_next(p, out);
    }

    return PARSE_OK;
}

// <if> -> TOK_KW_IF <expression> TOK_KW_THEN <branch> <else>
static ParseResult parse_if(Parser* p, AstNode** out)
{
    ParseResult result;

    AstNode* node = ast_new(AST_IF, p->current.line);
    if (node == NULL) {
        return PARSE_INTERNAL_ERROR;
    }

    // TOK_KW_IF
    result = expect(p, TOK_KW_IF);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    // <expression>
    result = parse_expression(p, &node->as.if_statement.condition);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    // TOK_KW_THEN
    result = expect(p, TOK_KW_THEN);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    // then branch: a block or one statement on the same line
    result = parse_branch(p, &node->as.if_statement.then_block);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    // after a one line then branch, the else may be on the next line, aligned with the if
    if (p->current.type == TOK_EOL) {
        TokenType next;
        result = lookahead_token(p, &next);
        if (result != PARSE_OK) {
            ast_free(node);
            return result;
        }

        if (next == TOK_KW_ELSE && p->lookahead.indent == p->indent_stack[p->indent_count - 1]) {
            result = next_token(p);
            if (result != PARSE_OK) {
                ast_free(node);
                return result;
            }
        }
    }

    // <else>
    result = parse_else(p, &node->as.if_statement.else_block);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    *out = node;
    return PARSE_OK;
}

// checks that the current token is the identifier with the given name (to, until, by are not keywords)
static bool is_identifier(Parser* p, const char* name)
{
    return p->current.type == TOK_ID && strcmp(p->current.data.str_val, name) == 0;
}

// parses the generator and guards of a for loop
// (on error the caller needs to free the node)
// <for_generator> -> TOK_ID TOK_CONTROL_ARROW <expression> <to_or_until> <expression> <for_optional_by>
// <for_header> -> <for_generator> <for_guard_list>
// <for_header_multiline> -> TOK_EOL TOK_INDENT <for_generator> TOK_EOL <for_guard_list_multiline> TOK_DEDENT
static ParseResult parse_for_header(Parser* p, AstNode* node, bool multiline)
{
    ParseResult result;

    // TOK_ID (the control variable)
    if (p->current.type != TOK_ID) {
        return PARSE_SYNTAX_ERROR;
    }
    node->as.for_statement.var_name = p->current.data.str_val;
    p->current.data.str_val = NULL;

    result = next_token(p);
    if (result != PARSE_OK) {
        return result;
    }

    // TOK_CONTROL_ARROW
    result = expect(p, TOK_CONTROL_ARROW);
    if (result != PARSE_OK) {
        return result;
    }

    // from
    result = parse_expression(p, &node->as.for_statement.from);
    if (result != PARSE_OK) {
        return result;
    }

    // <to_or_until>
    if (is_identifier(p, "to")) {
        node->as.for_statement.inclusive = true;
    } else if (is_identifier(p, "until")) {
        node->as.for_statement.inclusive = false;
    } else {
        return PARSE_SYNTAX_ERROR;
    }

    result = next_token(p);
    if (result != PARSE_OK) {
        return result;
    }

    result = parse_expression(p, &node->as.for_statement.to);
    if (result != PARSE_OK) {
        return result;
    }

    // <for_optional_by>
    if (is_identifier(p, "by")) {
        result = next_token(p);
        if (result != PARSE_OK) {
            return result;
        }

        result = parse_expression(p, &node->as.for_statement.step);
        if (result != PARSE_OK) {
            return result;
        }
    }

    // <for_guard_list>
    while (true) {
        // multi line: the generator and every guard are on their own line
        if (multiline) {
            result = expect(p, TOK_EOL);
            if (result != PARSE_OK) {
                return result;
            }
        }

        if (p->current.type != TOK_KW_IF) {
            break;
        }

        result = next_token(p);
        if (result != PARSE_OK) {
            return result;
        }

        AstNode* guard = NULL;
        result = parse_expression(p, &guard);
        if (result != PARSE_OK) {
            return result;
        }

        if (ast_list_push(&node->as.for_statement.guards, guard) == false) {
            ast_free(guard);
            return PARSE_INTERNAL_ERROR;
        }
    }

    return PARSE_OK;
}

// <for> -> TOK_KW_FOR <for_start> TOK_KW_DO <branch>
// <for_start> -> <for_header> | <for_header_multiline>
// <for_optional_by> -> TOK_ID"by" <expression> | epsilon
// <for_guard_list> -> TOK_KW_IF <expression> <for_guard_list> | epsilon
// <for_guard_list_multiline> -> TOK_KW_IF <expression> TOK_EOL <for_guard_list_multiline> | epsilon
// <to_or_until> -> TOK_ID"to" | TOK_ID"until"
static ParseResult parse_for(Parser* p, AstNode** out)
{
    ParseResult result;

    AstNode* node = ast_new(AST_FOR, p->current.line);
    if (node == NULL) {
        return PARSE_INTERNAL_ERROR;
    }

    // TOK_KW_FOR
    result = expect(p, TOK_KW_FOR);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    if (p->current.type == TOK_EOL) {
        // multi line header: TOK_EOL TOK_INDENT <header lines> TOK_DEDENT, then do aligned with for
        result = next_token(p);
        if (result == PARSE_OK) {
            result = expect(p, TOK_INDENT);
        }
        if (result == PARSE_OK) {
            result = parse_for_header(p, node, true);
        }
        if (result == PARSE_OK) {
            result = expect(p, TOK_DEDENT);
        }
    } else {
        // one line header
        result = parse_for_header(p, node, false);
    }

    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    // TOK_KW_DO
    result = expect(p, TOK_KW_DO);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    // body: a block or one statement on the same line
    result = parse_branch(p, &node->as.for_statement.block);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    *out = node;
    return PARSE_OK;
}

// <statement_simple> -> <variable> | <assign> | <return> | <expression> | <if>
static ParseResult parse_statement_simple(Parser* p, AstNode** out)
{
    ParseResult result;

    switch (p->current.type) {
    case TOK_KW_VAR:
        return parse_variable(p, out);
    case TOK_KW_VAL:
        return parse_variable(p, out);
    case TOK_KW_RETURN:
        return parse_return(p, out);
    case TOK_KW_FOR:
        return parse_for(p, out);
    case TOK_KW_IF:
        return parse_if(p, out);
    case TOK_ID: {
        TokenType next;
        result = lookahead_token(p, &next);
        if (result != PARSE_OK) {
            return result;
        }

        if (next == TOK_ASSIGN) {
            return parse_assign(p, out);
        }
    }
    // fall through - not an assignment, so its an expresssion statement
    default:
        return parse_expression_statement(p, out);
    }
}

// <statement> -> <statement_simple> <statement_end> | <while>
static ParseResult parse_statement(Parser* p, AstNode** out)
{
    if (p->current.type == TOK_KW_WHILE) {
        return parse_while(p, out);
    }

    // an if that ends with a one line branch needs a statement end, one that ends with a block does not (CONTROL)
    ParseResult result;
    if (p->current.type == TOK_KW_IF) {
        result = parse_if(p, out);
    } else {
        result = parse_statement_simple(p, out);
    }
    if (result != PARSE_OK) {
        return result;
    }

    // <statement_end> -> TOK_EOL | TOK_SEMICOLON | TOK_SEMICOLON TOK_EOL | epsilon
    if (p->current.type == TOK_EOL || p->current.type == TOK_SEMICOLON) {
        bool was_semicolon = (p->current.type == TOK_SEMICOLON);

        result = next_token(p);
        if (result != PARSE_OK) {
            ast_free(*out);
            return result;
        }

        // a fix to allow for both a ; and EOL
        if (was_semicolon && p->current.type == TOK_EOL) {
            result = next_token(p);
            if (result != PARSE_OK) {
                ast_free(*out);
                return result;
            }
        }
    } else if (p->previous_type != TOK_DEDENT) {
        // a statement that ended with a block (e.g. "val x = if ... else" + block) doesnt need a TOK_EOL (CONTROL)
        ast_free(*out);
        return PARSE_SYNTAX_ERROR;
    }

    return PARSE_OK;
}

// <block> -> INDENT <statement> <statement_next> DEDENT
static ParseResult parse_block(Parser* p, AstNode** out)
{
    ParseResult result;

    // INDENT
    result = expect(p, TOK_INDENT);
    if (result != PARSE_OK) {
        return result;
    }

    // statement(s)
    AstNode* block = ast_new(AST_BLOCK, p->current.line);
    if (block == NULL) {
        return PARSE_INTERNAL_ERROR;
    }

    do {
        AstNode* statement = NULL;
        result = parse_statement(p, &statement);
        if (result != PARSE_OK) {
            ast_free(block);
            return result;
        }

        bool successful_push;
        successful_push = ast_list_push(&block->as.block.statements, statement);
        if (successful_push == false) {
            ast_free(block);
            ast_free(statement);
            return PARSE_INTERNAL_ERROR;
        }

    } while (p->current.type != TOK_DEDENT);

    // DEDENT
    result = expect(p, TOK_DEDENT);
    if (result != PARSE_OK) {
        ast_free(block);
        return result;
    }

    *out = block;
    return PARSE_OK;
}

// <function_body> -> <value> <function_body_end> | TOK_EOL <block>
// <function_body_end> -> TOK_EOL | epsilon
static ParseResult parse_function_body(Parser* p, AstNode** out)
{
    ParseResult result;

    // TOK_EOL <block>
    if (p->current.type == TOK_EOL) {
        result = next_token(p);
        if (result != PARSE_OK) {
            return result;
        }

        return parse_block(p, out);
    }

    // <expression> TOK_EOL
    AstNode* block = ast_new(AST_BLOCK, p->current.line);
    if (block == NULL) {
        return PARSE_INTERNAL_ERROR;
    }

    AstNode* ret = ast_new(AST_RETURN, p->current.line);
    if (ret == NULL) {
        ast_free(block);
        return PARSE_INTERNAL_ERROR;
    }

    bool successful_push;
    successful_push = ast_list_push(&block->as.block.statements, ret);
    if (successful_push == false) {
        ast_free(block);
        ast_free(ret);
        return PARSE_INTERNAL_ERROR;
    }

    result = parse_value(p, &ret->as.return_statement.value);
    if (result != PARSE_OK) {
        ast_free(block);
        return result;
    }

    // TOK_EOL, not needed if the value ended with a block (CONTROL)
    if (p->current.type == TOK_EOL) {
        result = next_token(p);
        if (result != PARSE_OK) {
            ast_free(block);
            return result;
        }
    } else if (p->previous_type != TOK_DEDENT) {
        ast_free(block);
        return PARSE_SYNTAX_ERROR;
    }

    *out = block;
    return PARSE_OK;
}

// <function> -> TOK_KW_DEF TOK_ID <parameter_list> <optional_type> TOK_ASSIGN <function_body>
// <parameter_list> -> TOK_LEFT_PARENTHESIS <parameters> TOK_RIGHT_PARENTHESIS <parameter_list> | epsilon
static ParseResult parse_function(Parser* p, AstNode** out)
{
    ParseResult result;

    // TOK_KW_DEF
    result = expect(p, TOK_KW_DEF);
    if (result != PARSE_OK) {
        return result;
    }

    // TOK_ID
    if (p->current.type != TOK_ID) {
        return PARSE_SYNTAX_ERROR;
    }

    AstNode* node = ast_new(AST_FUNCTION, p->current.line);
    if (node == NULL) {
        return PARSE_INTERNAL_ERROR;
    }

    node->as.function.name = p->current.data.str_val;
    p->current.data.str_val = NULL;

    result = next_token(p);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    while (p->current.type == TOK_LEFT_PARENTHESIS) {
        size_t count_before = node->as.function.parameters.count;

        // TOK_LEFT_PARENTHESIS
        result = next_token(p);
        if (result != PARSE_OK) {
            ast_free(node);
            return result;
        }

        // <parameters>
        result = parse_parameters(p, &node->as.function.parameters);
        if (result != PARSE_OK) {
            ast_free(node);
            return result;
        }

        // TOK_RIGHT_PARENTHESIS
        result = expect(p, TOK_RIGHT_PARENTHESIS);
        if (result != PARSE_OK) {
            ast_free(node);
            return result;
        }

        // calculate and update the list size
        size_t list_size = node->as.function.parameters.count - count_before;
        result = size_list_push(&node->as.function.list_sizes, &node->as.function.list_count, list_size);
        if (result != PARSE_OK) {
            ast_free(node);
            return result;
        }
    }

    // <optional_type>
    result = parse_optional_type(p, &node->as.function.return_type);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    // TOK_ASSIGN
    result = expect(p, TOK_ASSIGN);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    // <function_body>
    result = parse_function_body(p, &node->as.function.block);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    *out = node;
    return PARSE_OK;
}

// <function_main> -> TOK_AT TOK_ID <optional_eol> <function>
static ParseResult parse_function_main(Parser* p, AstNode** out)
{
    ParseResult result;

    // TOK_AT
    unsigned int at_line = p->current.line;
    unsigned int at_column = p->current.column;
    result = expect(p, TOK_AT);
    if (result != PARSE_OK) {
        return result;
    }

    // TOK_ID
    if (p->current.line != at_line || p->current.column != at_column + 1) {
        return PARSE_LEXICAL_ERROR;
    }
    if (p->current.type != TOK_ID) {
        return PARSE_SYNTAX_ERROR;
    }
    if (strcmp(p->current.data.str_val, "main") != 0) {
        return PARSE_SYNTAX_ERROR;
    }
    result = next_token(p);
    if (result != PARSE_OK) {
        return result;
    }

    // <optional_eol>
    if (p->current.type == TOK_EOL) {
        result = next_token(p);
        if (result != PARSE_OK) {
            return result;
        }
    }

    result = parse_function(p, out);
    if (result != PARSE_OK) {
        return result;
    }

    (*out)->as.function.is_main_func = true;
    return PARSE_OK;
}

// parses one method of an extension, gives it a copy of the receiver and adds it to the function list (EXTKW)
static ParseResult parse_extension_method(Parser* p, AstNodeList* functions, const AstNode* receiver)
{
    AstNode* method = NULL;
    ParseResult result = parse_function(p, &method);
    if (result != PARSE_OK) {
        return result;
    }

    method->as.function.receiver = parameter_copy(receiver);
    if (method->as.function.receiver == NULL) {
        ast_free(method);
        return PARSE_INTERNAL_ERROR;
    }

    if (ast_list_push(functions, method) == false) {
        ast_free(method);
        return PARSE_INTERNAL_ERROR;
    }

    return PARSE_OK;
}

// parses an extension with one or more methods for one receiver (EXTKW)
// <extension> -> TOK_KW_EXTENSION TOK_LEFT_PARENTHESIS <parameter> TOK_RIGHT_PARENTHESIS <extension_end>
// <extension_end> -> TOK_EOL TOK_INDENT <extension_method_list> TOK_DEDENT | <function>
// <extension_method_list> -> <function> <extension_method_list> | epsilon
static ParseResult parse_extension(Parser* p, AstNodeList* functions)
{
    ParseResult result;

    // TOK_KW_EXTENSION
    result = expect(p, TOK_KW_EXTENSION);
    if (result != PARSE_OK) {
        return result;
    }
    // TOK_LEFT_PARENTHESIS
    result = expect(p, TOK_LEFT_PARENTHESIS);
    if (result != PARSE_OK) {
        return result;
    }

    // the receiver: exactly one explicitly typed parameter
    AstNode* receiver = NULL;
    result = parse_parameter(p, &receiver);
    if (result != PARSE_OK) {
        return result;
    }

    // TOK_RIGHT_PARENTHESIS
    result = expect(p, TOK_RIGHT_PARENTHESIS);
    if (result != PARSE_OK) {
        ast_free(receiver);
        return result;
    }

    // a single method on the same line
    if (p->current.type == TOK_KW_DEF) {
        result = parse_extension_method(p, functions, receiver);
        ast_free(receiver);
        return result;
    }

    // TOK_EOL TOK_INDENT, then an indented block of methods
    result = expect(p, TOK_EOL);
    if (result == PARSE_OK) {
        result = expect(p, TOK_INDENT);
    }

    while (result == PARSE_OK && p->current.type != TOK_DEDENT) {
        if (p->current.type != TOK_KW_DEF) {
            result = PARSE_SYNTAX_ERROR;
            break;
        }
        result = parse_extension_method(p, functions, receiver);
    }

    // TOK_DEDENT
    if (result == PARSE_OK) {
        result = expect(p, TOK_DEDENT);
    }

    ast_free(receiver);
    return result;
}

// <functions> -> <function> <functions> | <function_main> <functions> | <extension> <functions> | epsilon
static ParseResult parse_functions(Parser* p, AstNodeList* functions)
{
    while (p->current.type == TOK_KW_DEF || p->current.type == TOK_AT || p->current.type == TOK_KW_EXTENSION) {
        AstNode* function = NULL;
        ParseResult result;

        if (p->current.type == TOK_KW_EXTENSION) {
            result = parse_extension(p, functions);
            if (result != PARSE_OK) {
                return result;
            }
            continue;
        }

        if (p->current.type == TOK_KW_DEF) {
            result = parse_function(p, &function);
        } else { // TOK_AT
            result = parse_function_main(p, &function);
        }

        if (result != PARSE_OK) {
            return result;
        }

        bool successful_push;
        successful_push = ast_list_push(functions, function);

        if (successful_push == false) {
            ast_free(function);
            return PARSE_INTERNAL_ERROR;
        }
    }

    return PARSE_OK;
}

// <program> -> TOK_KW_IMPORT TOK_ID TOK_DOT TOK_MULTIPLY TOK_EOL <functions> TOK_EOF
ParseResult parse_program(Parser* p, AstNode** out)
{
    ParseResult result;
    result = next_token(p);
    if (result != PARSE_OK) {
        return result;
    }

    result = expect(p, TOK_KW_IMPORT);
    if (result != PARSE_OK) {
        return result;
    }
    if (p->current.type != TOK_ID) {
        return PARSE_SYNTAX_ERROR;
    }
    if (strcmp(p->current.data.str_val, "ifj26") != 0) {
        return PARSE_SYNTAX_ERROR;
    }

    result = next_token(p);
    if (result != PARSE_OK) {
        return result;
    }

    result = expect(p, TOK_DOT);
    if (result != PARSE_OK) {
        return result;
    }

    result = expect(p, TOK_MULTIPLY);
    if (result != PARSE_OK) {
        return result;
    }

    result = expect(p, TOK_EOL);
    if (result != PARSE_OK) {
        return result;
    }

    AstNode* node = ast_new(AST_PROGRAM, p->current.line);
    if (node == NULL) {
        return PARSE_INTERNAL_ERROR;
    }

    result = parse_functions(p, &node->as.program.functions);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    if (p->current.type != TOK_EOF) {
        ast_free(node);
        return PARSE_SYNTAX_ERROR;
    }

    *out = node;
    return PARSE_OK;
}