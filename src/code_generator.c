/**
 * @file code_generator.c
 * @brief Implementation of the code generator
 * @author Tomáš Neckař (xneckat00)
 */

#include "ast.h"
#include "symtable.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

void generate_code(AstNode *ast, stab *stab);
static void gen_node(AstNode *node);
static void gen_builtin_funcs(void);

static stab *st;
static stab_func *current_func;

static unsigned int if_counter;
static unsigned int while_counter;
static unsigned int skip_counter;

static bool direct_func_block;

void generate_code(AstNode *ast, stab *stab) {
    assert(ast != NULL && ast->node_kind == AST_PROGRAM);
    assert(stab != NULL && stab_main(stab));

    st = stab;

    // walk through the whole AST and generate code (preorder)
    gen_node(ast);
}

static void gen_node(AstNode *node) {
    switch (node->node_kind) {
    case AST_PROGRAM:

        printf(".IFJcode26\n");
        printf("DEFVAR GF@tmp1\n");
        printf("DEFVAR GF@tmp2\n");
        printf("DEFVAR GF@tmp3\n");

        stab_func *main = stab_main(st);

        printf("CREATEFRAME\n");
        printf("PUSHFRAME\n");
        printf("CALL %s\n", main->name);
        printf("POPFRAME\n");
        printf("EXIT int@0\n");

        // gen main func first
        for (size_t i = 0; i < node->as.program.functions.count; i++) {
            AstNode *func = node->as.program.functions.items[i];
            if (func->as.function.is_main_func) {
                gen_node(func);
                break;
            }
        }

        // gen other funcs
        for (size_t i = 0; i < node->as.program.functions.count; i++) {
            AstNode *func = node->as.program.functions.items[i];
            if (!func->as.function.is_main_func) {
                gen_node(func);
            }
        }

        // gen builtin funcs
        gen_builtin_funcs();
        break;
    case AST_FUNCTION:

        // start of func
        if (node->as.function.receiver == NULL) {
            printf("LABEL %s\n", node->as.function.name);
        } else {
            printf("LABEL $EXTKW$%s$%u\n", node->as.function.name, node->as.function.receiver->as.parameter.type);
        }

        current_func =
            stab_find_func(st,
                           node->as.function.name,
                           node->as.function.receiver != NULL ? &node->as.function.receiver->as.parameter.type : NULL);
        assert(current_func != NULL);

        // gen DEFVARs for locals throughout the whole func
        for (size_t i = 0; i < current_func->locals_count; i++) {
            if (!current_func->locals[i]->is_param) {
                // local frame: var_name$autoinc_id
                printf("DEFVAR LF@%s$%u\n", current_func->locals[i]->name, current_func->locals[i]->id);
            }
        }

        direct_func_block = true;
        gen_node(node->as.function.block);

        // add return in the final statement if only implicit
        AstNodeList *statements = &node->as.function.block->as.block.statements;
        AstNode *last = statements->items[statements->count - 1];
        if (last->node_kind != AST_RETURN) {
            // ret_type == UNIT funcs always return nil
            if (current_func->ret_type == TYPE_UNIT) {
                printf("PUSHS nil@nil\n");
            } else if (last->node_kind == AST_EXPRESSION_STATEMENT &&
                       last->as.expression_statement.expression->type == TYPE_INT &&
                       current_func->ret_type == TYPE_DOUBLE) {
                // int to float expansion of implicit return value
                printf("INT2FLOATS\n");
            }
            printf("RETURN\n");
        }
        break;
    case AST_BLOCK: {

        // is this the lowest scope in a func?
        bool direct_func_block_local = direct_func_block;
        direct_func_block = false;

        // gen all statements sequentially
        for (size_t i = 0; i < node->as.block.statements.count; i++) {
            gen_node(node->as.block.statements.items[i]);

            // check if returned value from the line should be popped
            // ALL ret_type == UNIT funcs and non-last expressions that return a value on stack, that won't be used
            if (node->as.block.statements.items[i]->node_kind == AST_EXPRESSION_STATEMENT &&
                (current_func->ret_type == TYPE_UNIT || !direct_func_block_local ||
                 i != node->as.block.statements.count - 1)) {
                printf("POPS GF@tmp1\n");
            }
        }

        break;
    }
    case AST_WHILE: {

        unsigned int while_save = while_counter++;

        // condition
        printf("LABEL while$start$%u\n", while_save);
        gen_node(node->as.while_statement.condition);
        printf("PUSHS bool@false\n");
        printf("JUMPIFEQS while$end$%u\n", while_save);

        // main block
        gen_node(node->as.while_statement.block);

        // unconditional jump
        printf("JUMP while$start$%u\n", while_save);
        // after loop label
        printf("LABEL while$end$%u\n", while_save);

        break;
    }
    case AST_IF: {

        unsigned int if_save = if_counter++;

        gen_node(node->as.if_statement.condition);
        printf("PUSHS bool@false\n");
        printf("JUMPIFEQS else$%u\n", if_save);

        // if/then block
        gen_node(node->as.if_statement.then_block);

        // flow
        if (node->as.if_statement.else_block != NULL) {
            printf("JUMP if$%u\n", if_save);
        }
        printf("LABEL else$%u\n", if_save);

        // else block
        if (node->as.if_statement.else_block != NULL) {
            gen_node(node->as.if_statement.else_block);
            printf("LABEL if$%u\n", if_save);
        }

        break;
    }
    case AST_VARIABLE_DEFINITION:

        // evaluate right-side value first
        gen_node(node->as.variable_definition.value);

        // int to float expansion
        if (node->as.variable_definition.value->type == TYPE_INT &&
            node->as.variable_definition.declared_type == TYPE_DOUBLE) {
            printf("INT2FLOATS\n");
        }

        // local was already defined at the start of the func
        // assign right-side value from data stack
        printf("POPS LF@%s$%u\n", node->as.variable_definition.name, node->var_id);

        break;
    case AST_ASSIGN:

        // evaluate right-side value first
        gen_node(node->as.assign.value);

        // get var type to which is assigning
        assert(node->var_id < current_func->locals_count);
        const stab_var *var = current_func->locals[node->var_id];

        // int to float expansion
        if (node->as.assign.value->type == TYPE_INT && var->type == TYPE_DOUBLE) {
            printf("INT2FLOATS\n");
        }

        // assign right-side value from data stack
        printf("POPS LF@%s$%u\n", node->as.assign.name, node->var_id);

        break;
    case AST_RETURN:

        // eval return value
        if (node->as.return_statement.value != NULL) {
            gen_node(node->as.return_statement.value);

            // always pop on ret_type == UNIT funcs
            if (current_func->ret_type == TYPE_UNIT) {
                printf("POPS GF@tmp1\n");
                printf("PUSHS nil@nil\n");
            } else {
                // int to float expansion
                if (node->as.return_statement.value->type == TYPE_INT && current_func->ret_type == TYPE_DOUBLE) {
                    printf("INT2FLOATS\n");
                }
            }
        } else {
            printf("PUSHS nil@nil\n");
        }

        printf("RETURN\n");

        break;
    case AST_EXPRESSION_STATEMENT:

        // recurse deeper
        gen_node(node->as.expression_statement.expression);

        break;
    case AST_LITERAL:

        // just pushes value on the data stack
        printf("PUSHS ");

        switch (node->type) {
        case TYPE_INT:
            printf("int@%" PRId64, node->as.literal.value.int_val);
            break;
        case TYPE_DOUBLE:
            printf("float@%a", node->as.literal.value.double_val);
            break;
        case TYPE_STRING:
            printf("string@");
            for (size_t i = 0; node->as.literal.value.string_val[i] != '\0'; i++) {
                unsigned char c = node->as.literal.value.string_val[i];
                assert(c < 128);

                if (c <= 32 || c == 35 || c == 92) {
                    // escape char
                    printf("\\%03d", c);
                } else {
                    // normal printout
                    printf("%c", c);
                }
            }
            break;
        case TYPE_UNIT:
            printf("nil@nil");
            break;
        case TYPE_BOOL:
            printf("bool@%s", node->as.literal.value.int_val ? "true" : "false");
            break;
        default:
            assert(false);
        }

        printf("\n");

        break;
    case AST_IDENTIFIER:

        // just pushes value on the data stack
        printf("PUSHS LF@%s$%u\n", node->as.identifier.name, node->var_id);

        break;
    case AST_UNARY_OPERATOR: {

        AstNode *op = node->as.unary.operand;
        switch (node->as.unary.operator_type) {
        case OP_NEG:
            assert(op->type == TYPE_INT || op->type == TYPE_DOUBLE);

            // 0 - op
            if (op->type == TYPE_INT) {
                printf("PUSHS int@0\n");
            } else {
                printf("PUSHS float@%a\n", 0.0f);
            }
            gen_node(op);
            printf("SUBS\n");

            break;
        case OP_POS:

            // same as without the +
            gen_node(op);

            break;
        case OP_NOT:
            assert(op->type == TYPE_BOOL);

            // !op
            gen_node(op);
            printf("NOTS\n");

            break;
        default:
            assert(false);
        }

        break;
    }
    case AST_BINARY_OPERATOR: {

        AstNode *l = node->as.binary.left;
        AstNode *r = node->as.binary.right;

        // assert types
        switch (node->as.binary.operator_type) {
        case OP_SUB:
        case OP_DIV:
        case OP_MUL:
            assert((l->type == TYPE_INT || l->type == TYPE_DOUBLE) && (r->type == TYPE_INT || r->type == TYPE_DOUBLE));
            break;
        case OP_ADD:
        case OP_LT:
        case OP_LE:
        case OP_GT:
        case OP_GE:
            assert(
                ((l->type == TYPE_INT || l->type == TYPE_DOUBLE) && (r->type == TYPE_INT || r->type == TYPE_DOUBLE)) ||
                (l->type == TYPE_STRING && r->type == TYPE_STRING));
            break;
        case OP_EQ:
        case OP_NE:
            assert(
                ((l->type == TYPE_INT || l->type == TYPE_DOUBLE) && (r->type == TYPE_INT || r->type == TYPE_DOUBLE)) ||
                l->type == r->type);
            break;
        case OP_AND:
        case OP_OR:
            assert(l->type == TYPE_BOOL && r->type == TYPE_BOOL);
            break;
        default:
            assert(false);
        }

        // eval left op
        gen_node(l);

        // check for left op expand
        bool expanded = false;
        if (l->type == TYPE_INT && r->type == TYPE_DOUBLE) {
            printf("INT2FLOATS\n");
            expanded = true;
        }

        // short circuit for OR: true || (don't eval)
        unsigned int skip_save = skip_counter++;
        if (node->as.binary.operator_type == OP_OR) {
            printf("PUSHS bool@true\n"
                   "JUMPIFEQS $skip$%u\n"
                   "PUSHS bool@false\n",
                   skip_save);
        }
        // short circuit for AND: false && (don't eval)
        else if (node->as.binary.operator_type == OP_AND) {
            printf("PUSHS bool@false\n"
                   "JUMPIFEQS $skip$%u\n"
                   "PUSHS bool@true\n",
                   skip_save);
        }

        // eval right op
        gen_node(r);

        // check for right op expand
        if (l->type == TYPE_DOUBLE && r->type == TYPE_INT) {
            printf("INT2FLOATS\n");
            expanded = true;
        }

        // eval binary op
        switch (node->as.binary.operator_type) {
        case OP_ADD:
            if (l->type == TYPE_STRING) {
                printf("POPS GF@tmp3\n"
                       "POPS GF@tmp2\n"
                       "CONCAT GF@tmp1 GF@tmp2 GF@tmp3\n"
                       "PUSHS GF@tmp1\n");
            } else {
                printf("ADDS\n");
            }
            break;
        case OP_SUB:
            printf("SUBS\n");
            break;
        case OP_MUL:
            printf("MULS\n");
            break;
        case OP_DIV:
            if (expanded || l->type == TYPE_DOUBLE) {
                // float
                printf("DIVS\n");
            } else {
                // int
                printf("IDIVS\n");
            }
            break;
        case OP_LT:
            printf("LTS\n");
            break;
        case OP_LE:
            printf("POPS GF@tmp3\n"
                   "POPS GF@tmp2\n"

                   "EQ GF@tmp1 GF@tmp2 GF@tmp3\n"
                   "PUSHS GF@tmp1\n"

                   "LT GF@tmp1 GF@tmp2 GF@tmp3\n"
                   "PUSHS GF@tmp1\n"

                   "ORS\n");
            break;
        case OP_GT:
            printf("GTS\n");
            break;
        case OP_GE:
            printf("POPS GF@tmp3\n"
                   "POPS GF@tmp2\n"

                   "EQ GF@tmp1 GF@tmp2 GF@tmp3\n"
                   "PUSHS GF@tmp1\n"

                   "GT GF@tmp1 GF@tmp2 GF@tmp3\n"
                   "PUSHS GF@tmp1\n"

                   "ORS\n");
            break;
        case OP_EQ:
            printf("EQS\n");
            break;
        case OP_NE:
            printf("EQS\n");
            printf("NOTS\n");
            break;
        case OP_AND:
            printf("ANDS\n");
            break;
        case OP_OR:
            printf("ORS\n");
            break;
        default:
            assert(false);
        }

        // gen label for short-circuit ops
        // non-short-circuited result is already on the stack, jump over the constant push
        if (node->as.binary.operator_type == OP_OR) {
            printf("JUMP $skip$end$%u\n"
                   "LABEL $skip$%u\n"
                   "PUSHS bool@true\n"
                   "LABEL $skip$end$%u\n",
                   skip_save,
                   skip_save,
                   skip_save);
        } else if (node->as.binary.operator_type == OP_AND) {
            printf("JUMP $skip$end$%u\n"
                   "LABEL $skip$%u\n"
                   "PUSHS bool@false\n"
                   "LABEL $skip$end$%u\n",
                   skip_save,
                   skip_save,
                   skip_save);
        }

        break;
    }
    case AST_CALL: {

        // get the called func
        stab_func *callee =
            stab_find_func(st, node->as.call.name, node->as.call.is_member ? &node->as.call.args.items[0]->type : NULL);
        assert(callee != NULL);

        // get current param list
        stab_size_t param_idx = 0;
        stab_size_t param_list_idx = 0;
        stab_param_list *param_list = &callee->param_lists[0];

        // EXTKW: eval extended arg
        if (node->as.call.is_member) {
            gen_node(node->as.call.args.items[0]);
        }

        // eval all args first
        // EXTKW: "extends" param is inserted first into AST's args
        for (size_t i = node->as.call.is_member ? 1 : 0; i < node->as.call.args.count; i++) {
            AstNode *arg = node->as.call.args.items[i];

            // eval arg
            gen_node(arg);

            // get current param list and param idx
            while (param_idx == param_list->count) {
                param_list_idx++;
                assert(param_list_idx < callee->list_count);
                param_idx = 0;
                param_list = &callee->param_lists[param_list_idx];
            }

            // int to float expansion
            if (arg->type == TYPE_INT && param_list->params[param_idx].type == TYPE_DOUBLE) {
                printf("INT2FLOATS\n");
            }

            param_idx++;
        }

        // put the arg values inside temp frame
        // go from the back of the arg list and pop values from stack into the arg vars
        // EXTKW: "extends" param is inserted first into AST's args
        printf("CREATEFRAME\n");
        for (size_t i = node->as.call.is_member ? 1 : 0; i < node->as.call.args.count; i++) {
            // get current param list and param idx
            while (param_idx == 0) {
                assert(param_list_idx > 0);
                param_list_idx--;
                param_list = &callee->param_lists[param_list_idx];
                param_idx = param_list->count;
            }
            const stab_param *param = &param_list->params[--param_idx];

            // def param and assign value
            if (callee->is_builtin) {
                printf("DEFVAR TF@%s\n", param->name);
                printf("POPS TF@%s\n", param->name);
            } else {
                printf("DEFVAR TF@%s$%u\n", param->name, param->id);
                printf("POPS TF@%s$%u\n", param->name, param->id);
            }
        }

        // EXTKW: def param and assign value
        if (node->as.call.is_member) {
            if (callee->is_builtin) {
                printf("DEFVAR TF@%s\n", callee->extends.name);
                printf("POPS TF@%s\n", callee->extends.name);
            } else {
                printf("DEFVAR TF@%s$%u\n", callee->extends.name, callee->extends.id);
                printf("POPS TF@%s$%u\n", callee->extends.name, callee->extends.id);
            }
        }

        // call
        printf("PUSHFRAME\n");
        if (!node->as.call.is_member || callee->is_builtin) {
            printf("CALL %s\n", node->as.call.name);
        } else {
            printf("CALL $EXTKW$%s$%u\n", node->as.call.name, callee->extends.type);
        }
        printf("POPFRAME\n");

        break;
    }
    default:
        // should never run
        assert(false);
        break;
    }
}

static void gen_builtin_funcs(void) {
    // print(value: Any): Unit
    printf("LABEL print\n"
           "PUSHS nil@nil\n"

           // check for nil
           "TYPE GF@tmp1 LF@value\n"
           "JUMPIFNEQ else$%u GF@tmp1 string@nil\n"
           "WRITE string@()\n"
           "RETURN\n"

           // normal types
           "LABEL else$%u\n"
           "WRITE LF@value\n"
           "RETURN\n",
           if_counter,
           if_counter);
    if_counter++;

    // readLine(): String
    printf("LABEL readLine\n"
           "READ GF@tmp1 string\n"

           "JUMPIFEQ else$%u GF@tmp1 nil@nil\n"
           "PUSHS GF@tmp1\n"
           "RETURN\n"

           "LABEL else$%u\n"
           "PUSHS string@\n"
           "RETURN\n",
           if_counter,
           if_counter);
    if_counter++;

    // readInt(): Int
    printf("LABEL readInt\n"
           "READ GF@tmp1 int\n"

           "JUMPIFEQ else$%u GF@tmp1 nil@nil\n"
           "PUSHS GF@tmp1\n"
           "RETURN\n"

           "LABEL else$%u\n"
           "PUSHS int@0\n"
           "RETURN\n",
           if_counter,
           if_counter);
    if_counter++;

    // readDouble(): Double
    printf("LABEL readDouble\n"
           "READ GF@tmp1 float\n"

           "JUMPIFEQ else$%u GF@tmp1 nil@nil\n"
           "PUSHS GF@tmp1\n"
           "RETURN\n"

           "LABEL else$%u\n"
           "PUSHS float@%a\n"
           "RETURN\n",
           if_counter,
           if_counter,
           0.0f);
    if_counter++;

    // readBoolean(): Bool
    printf("LABEL readBoolean\n"
           "READ GF@tmp1 bool\n"

           "JUMPIFEQ else$%u GF@tmp1 nil@nil\n"
           "PUSHS GF@tmp1\n"
           "RETURN\n"

           "LABEL else$%u\n"
           "PUSHS bool@false\n"
           "RETURN\n",
           if_counter,
           if_counter);
    if_counter++;

    // toStr(value: Any): String
    // toString: String
    printf("LABEL toStr\n"
           "LABEL toString\n" // VALMEM
           "TYPE GF@tmp1 LF@value\n"
           "PUSHS LF@value\n"

           // string
           "JUMPIFNEQ else$%u GF@tmp1 string@string\n"
           "RETURN\n"
           "LABEL else$%u\n"

           // int
           "JUMPIFNEQ else$%u GF@tmp1 string@int\n"
           "INT2STRS\n"
           "RETURN\n"
           "LABEL else$%u\n"

           // float
           "JUMPIFNEQ else$%u GF@tmp1 string@float\n"
           "FLOAT2STRS\n"
           "RETURN\n"
           "LABEL else$%u\n"

           // bool
           "JUMPIFNEQ else$%u GF@tmp1 string@bool\n"
           "PUSHS bool@true\n"
           "JUMPIFEQS else$%u\n"
           "PUSHS string@false\n"
           "RETURN\n"
           "LABEL else$%u\n"
           "PUSHS string@true\n"
           "RETURN\n"
           "LABEL else$%u\n"

           // nil
           "POPS GF@tmp2\n"
           "PUSHS string@()\n"
           "RETURN\n",
           if_counter,
           if_counter,
           if_counter + 1,
           if_counter + 1,
           if_counter + 2,
           if_counter + 2,
           if_counter + 3,
           if_counter + 4,
           if_counter + 4,
           if_counter + 3);
    if_counter += 5;

    // toInt(value: Double): Int
    // Int.toInt: Int
    printf("LABEL toInt\n"
           "PUSHS LF@value\n"
           "FLOAT2INTS\n"
           "RETURN\n");

    // strLength(value: String): Int
    // String.length: Int
    printf("LABEL strLength\n"
           "LABEL length\n" // VALMEM
           "STRLEN GF@tmp1 LF@value\n"
           "PUSHS GF@tmp1\n"
           "RETURN\n");

    // subStr(value: String, start: Int, end: Int): String
    printf("LABEL subStr\n"

           // start < 0
           "LT GF@tmp1 LF@start int@0\n"
           "JUMPIFEQ else$%u GF@tmp1 bool@true\n"

           // end < 0
           "LT GF@tmp1 LF@end int@0\n"
           "JUMPIFEQ else$%u GF@tmp1 bool@true\n"

           // start > end
           "GT GF@tmp1 LF@start LF@end\n"
           "JUMPIFEQ else$%u GF@tmp1 bool@true\n"

           // start >= strLength(value)
           "STRLEN GF@tmp2 LF@value\n"
           "GT GF@tmp1 LF@start GF@tmp2\n"
           "JUMPIFEQ else$%u GF@tmp1 bool@true\n"
           "JUMPIFEQ else$%u LF@start GF@tmp2\n"

           // end > strLength(value)
           "GT GF@tmp1 LF@end GF@tmp2\n"
           "JUMPIFEQ else$%u GF@tmp1 bool@true\n"

           "MOVE GF@tmp1 string@\n"  // op1
           "MOVE GF@tmp2 string@\n"  // op2
           "MOVE GF@tmp3 LF@start\n" // index

           "LABEL while$start$%u\n"
           "JUMPIFEQ while$end$%u GF@tmp3 LF@end\n"

           "GETCHAR GF@tmp2 LF@value GF@tmp3\n"
           "CONCAT GF@tmp1 GF@tmp1 GF@tmp2\n"
           "ADD GF@tmp3 GF@tmp3 int@1\n"

           "JUMP while$start$%u\n"
           "LABEL while$end$%u\n"

           "PUSHS GF@tmp1\n"
           "RETURN\n"

           "LABEL else$%u\n"
           "EXIT int@40\n", // exit 40 - runtime error
           if_counter,
           if_counter,
           if_counter,
           if_counter,
           if_counter,
           if_counter,
           while_counter,
           while_counter,
           while_counter,
           while_counter,
           if_counter);
    if_counter++;
    while_counter++;

    // ord(value: String, index: Int): Int
    printf("LABEL ord\n"

           // index < 0
           "LT GF@tmp1 LF@index int@0\n"
           "JUMPIFEQ else$%u GF@tmp1 bool@true\n"

           // index >= strLength(value)
           "STRLEN GF@tmp2 LF@value\n"
           "GT GF@tmp1 LF@index GF@tmp2\n"
           "JUMPIFEQ else$%u GF@tmp1 bool@true\n"
           "JUMPIFEQ else$%u LF@index GF@tmp2\n"

           "STRI2INT GF@tmp1 LF@value LF@index\n"
           "PUSHS GF@tmp1\n"
           "RETURN\n"

           "LABEL else$%u\n"
           "PUSHS int@-1\n"
           "RETURN\n",
           if_counter,
           if_counter,
           if_counter,
           if_counter);
    if_counter++;

    // chr(code: Int): String
    printf("LABEL chr\n"
           "PUSHS LF@code\n"
           "INT2CHARS\n"
           "RETURN\n");

    // Double.floor: Double
    printf("LABEL floor\n"
           "FLOAT2INT GF@tmp1 LF@value\n"
           "INT2FLOAT GF@tmp1 GF@tmp1\n"

           // if truncating up
           "LT GF@tmp2 LF@value GF@tmp1\n"
           "JUMPIFNEQ else$%u GF@tmp2 bool@true\n"

           // then value =- 1
           "SUB GF@tmp1 GF@tmp1 float@%a\n"

           // else
           "LABEL else$%u\n"
           "PUSHS GF@tmp1\n"
           "RETURN\n",
           if_counter,
           1.0f,
           if_counter);
    if_counter++;
}