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

static void gen_node(AstNode *node);
static void gen_program(AstNode *node);
static void gen_function(AstNode *node);
static unsigned count_fors(AstNode *block);
static void gen_block(AstNode *node);
static void gen_while(AstNode *node);
static void gen_for(AstNode *node);
static void gen_if(AstNode *node);
static void gen_variable_definition(AstNode *node);
static void gen_assign(AstNode *node);
static void gen_return(AstNode *node);
static void gen_expression_statement(AstNode *node);
static void gen_literal(AstNode *node);
static void gen_identifier(AstNode *node);
static void gen_unary_operator(AstNode *node);
static void gen_binary_operator(AstNode *node);
static bool is_num(AstNode *node);
static void gen_call(AstNode *node);
static void gen_builtin_funcs(void);

static stab *st;
static stab_func *current_func;
static bool is_func_block;

static unsigned if_counter;
static unsigned for_counter;
static unsigned while_counter;
static unsigned skip_counter;

// entry point of the code generator
// prints IFJcode26 for the whole program to stdout
void generate_code(AstNode *ast, stab *stab) {
    assert(ast != NULL && ast->node_kind == AST_PROGRAM);
    assert(stab != NULL && stab_main(stab));

    st = stab;

    // walk through the whole AST and generate code
    gen_node(ast);
}

// generate current node
// selects which func to call based on node_kind
// uses indirect recursion
static void gen_node(AstNode *node) {
    switch (node->node_kind) {
    case AST_PROGRAM:
        gen_program(node);
        break;
    case AST_FUNCTION:
        gen_function(node);
        break;
    case AST_BLOCK:
        gen_block(node);
        break;
    case AST_WHILE:
        gen_while(node);
        break;
    case AST_FOR:
        gen_for(node);
        break;
    case AST_IF:
        gen_if(node);
        break;
    case AST_VARIABLE_DEFINITION:
        gen_variable_definition(node);
        break;
    case AST_ASSIGN:
        gen_assign(node);
        break;
    case AST_RETURN:
        gen_return(node);
        break;
    case AST_EXPRESSION_STATEMENT:
        gen_expression_statement(node);
        break;
    case AST_LITERAL:
        gen_literal(node);
        break;
    case AST_IDENTIFIER:
        gen_identifier(node);
        break;
    case AST_UNARY_OPERATOR:
        gen_unary_operator(node);
        break;
    case AST_BINARY_OPERATOR:
        gen_binary_operator(node);
        break;
    case AST_CALL:
        gen_call(node);
        break;
    default:
        // should never run
        assert(false);
        break;
    }
}

// generate program header, global vars and recurse gen funcs
static void gen_program(AstNode *node) {
    printf(".IFJcode26\n");
    printf("DEFVAR GF@tmp1\n");
    printf("DEFVAR GF@tmp2\n");
    printf("DEFVAR GF@tmp3\n");
    printf("DEFVAR GF@tmp4\n");

    stab_func *main = stab_main(st);

    printf("CREATEFRAME\n");
    printf("PUSHFRAME\n");
    printf("CALL %s\n", main->name);
    printf("POPFRAME\n");
    printf("EXIT int@0\n");

    // gen funcs
    for (size_t i = 0; i < node->as.program.functions.count; i++) {
        gen_node(node->as.program.functions.items[i]);
    }

    // gen builtin funcs
    gen_builtin_funcs();
}

// generate func label, DEFVARs of all locals and "for" helper vars
// then func body and implicit return if needed
static void gen_function(AstNode *node) {
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

    // gen DEFVARs for locals throughout the current func
    for (size_t i = 0; i < current_func->locals_count; i++) {
        if (!current_func->locals[i]->is_param) {
            // local frame: var_name$autoinc_id
            printf("DEFVAR LF@%s$%u\n", current_func->locals[i]->name, current_func->locals[i]->id);
        }
    }

    // count for cycles inside current func
    unsigned for_cycles = count_fors(node->as.function.block);

    // gen DEFVARs for helper vars for "for" statements
    for (unsigned i = 0; i < for_cycles; i++) {
        printf("DEFVAR LF@for$to$%u\n", for_counter + i);
        printf("DEFVAR LF@for$step$%u\n", for_counter + i);
    }

    is_func_block = true;
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
}

// count all "for" statements inside block (recursively)
static unsigned count_fors(AstNode *block) {
    unsigned count = 0;
    for (size_t i = 0; i < block->as.block.statements.count; i++) {
        switch (block->as.block.statements.items[i]->node_kind) {
        case AST_WHILE:
            count += count_fors(block->as.block.statements.items[i]->as.while_statement.block);
            break;
        case AST_FOR:
            count++;
            count += count_fors(block->as.block.statements.items[i]->as.for_statement.block);
            break;
        case AST_IF:
            count += count_fors(block->as.block.statements.items[i]->as.if_statement.then_block);
            if (block->as.block.statements.items[i]->as.if_statement.else_block != NULL) {
                count += count_fors(block->as.block.statements.items[i]->as.if_statement.else_block);
            }
            break;
        default:
            // do nothing - there is no block inside
            break;
        }
    }
    return count;
}

// generate all statements of a block sequentially
// pops unused values of expression statements from data stack
static void gen_block(AstNode *node) {
    // is this the lowest scope in a func?
    bool is_func_block_local = is_func_block;
    is_func_block = false;

    // gen all statements sequentially
    for (size_t i = 0; i < node->as.block.statements.count; i++) {
        gen_node(node->as.block.statements.items[i]);

        // check if returned value from the line should be popped
        // ALL ret_type == UNIT funcs and non-last expressions that return a value on stack, that won't be used
        if (node->as.block.statements.items[i]->node_kind == AST_EXPRESSION_STATEMENT &&
            (current_func->ret_type == TYPE_UNIT || !is_func_block_local || i != node->as.block.statements.count - 1)) {
            printf("POPS GF@tmp1\n");
        }
    }
}

// generate while cycle
static void gen_while(AstNode *node) {
    unsigned while_counter_local = while_counter++;

    // condition
    printf("LABEL while$start$%u\n", while_counter_local);
    gen_node(node->as.while_statement.condition);
    printf("PUSHS bool@false\n");
    printf("JUMPIFEQS while$end$%u\n", while_counter_local);

    // main block
    gen_node(node->as.while_statement.block);

    // unconditional jump
    printf("JUMP while$start$%u\n", while_counter_local);
    // after loop label
    printf("LABEL while$end$%u\n", while_counter_local);
}

// CONTROL: generate for cycle
static void gen_for(AstNode *node) {
    unsigned for_counter_local = for_counter++;
    char *iteration_var_name = node->as.for_statement.var_name;
    unsigned iteration_var_id = node->var_id;

    // eval generator's from, to, step and pop it to tmp vars
    gen_node(node->as.for_statement.from);
    gen_node(node->as.for_statement.to);
    // step can be implicit 1
    if (node->as.for_statement.step != NULL) {
        gen_node(node->as.for_statement.step);
        printf("POPS GF@tmp3\n");

        // check step != 0
        if (node->as.for_statement.step->node_kind != AST_LITERAL) {
            printf("JUMPIFNEQ for$zero_check$%u GF@tmp3 int@0\n"
                   "EXIT int@40\n"
                   "LABEL for$zero_check$%u\n",
                   for_counter_local,
                   for_counter_local);
        }
    } else {
        printf("MOVE GF@tmp3 int@1\n");
    }
    printf("POPS GF@tmp2\n");
    printf("POPS GF@tmp1\n");

    // check generator for wrong direction/zero cycles
    // e.g.: 1 to 4 by -2; or: 1 until 1
    //
    // then calculate end:
    // - inclusive && step is not null: end = ((to - from) / step + 1) * step + from
    // - else: end = to + (inclusive ? 1 : 0)
    //   -> because step = 1, so it HAS to hit it
    if (node->as.for_statement.inclusive && node->as.for_statement.step != NULL) {
        // check if step is +/-
        printf("LT GF@tmp4 GF@tmp3 int@0\n"
               "JUMPIFEQ for$neg_step$%u GF@tmp4 bool@true\n"

               // pos_step
               // if (from > to) then 0 cycles
               "GT GF@tmp4 GF@tmp1 GF@tmp2\n"
               "JUMPIFEQ for$end$%u GF@tmp4 bool@true\n"
               "JUMP for$calc_end$%u\n"

               // neg_step
               // if (from < to) then 0 cycles
               "LABEL for$neg_step$%u\n"
               "LT GF@tmp4 GF@tmp1 GF@tmp2\n"
               "JUMPIFEQ for$end$%u GF@tmp4 bool@true\n",
               for_counter_local,
               for_counter_local,
               for_counter_local,
               for_counter_local,
               for_counter_local);

        // calc end
        printf("LABEL for$calc_end$%u\n"
               "MOVE LF@%s$%u GF@tmp1\n"

               "SUB GF@tmp4 GF@tmp2 GF@tmp1\n"      // to - from
               "IDIV GF@tmp4 GF@tmp4 GF@tmp3\n"     // /= step
               "ADD GF@tmp4 GF@tmp4 int@1\n"        // += 1
               "MUL GF@tmp4 GF@tmp4 GF@tmp3\n"      // *= step
               "ADD LF@for$to$%u GF@tmp4 GF@tmp1\n" // += from

               "MOVE LF@for$step$%u GF@tmp3\n",
               for_counter_local,
               iteration_var_name,
               iteration_var_id,
               for_counter_local,
               for_counter_local);
    } else {
        // check generator
        if (node->as.for_statement.inclusive) {
            // inclusive right bound
            // step is implicitly 1
            // if (from > to) then 0 cycles
            printf("GT GF@tmp4 GF@tmp1 GF@tmp2\n"
                   "JUMPIFEQ for$end$%u GF@tmp4 bool@true\n",
                   for_counter_local);
        } else {
            // exclusive right bound => step = 1
            // if (from >= to) then 0 cycles
            printf("GT GF@tmp4 GF@tmp1 GF@tmp2\n"
                   "JUMPIFEQ for$end$%u GF@tmp4 bool@true\n"
                   "JUMPIFEQ for$end$%u GF@tmp1 GF@tmp2\n",
                   for_counter_local,
                   for_counter_local);
        }

        // calculate end
        // end = to ...
        printf("MOVE LF@%s$%u GF@tmp1\n"
               "MOVE LF@for$to$%u GF@tmp2\n"
               "MOVE LF@for$step$%u GF@tmp3\n",
               iteration_var_name,
               iteration_var_id,
               for_counter_local,
               for_counter_local);

        // ... + (inclusive ? 1 : 0)
        if (node->as.for_statement.inclusive) {
            printf("ADD LF@for$to$%u LF@for$to$%u int@1\n", for_counter_local, for_counter_local);
        }
    }

    // start (after gen check)
    printf("LABEL for$start$%u\n", for_counter_local);

    // if (!guard1) continue
    // if (!guard2) continue
    // ...
    // else ok, run block
    for (size_t i = 0; i < node->as.for_statement.guards.count; i++) {
        gen_node(node->as.for_statement.guards.items[i]);
        printf("PUSHS bool@false\n"
               "JUMPIFEQS for$continue$%u\n",
               for_counter_local);
    }

    // inner block
    gen_node(node->as.for_statement.block);

    // continue
    printf("LABEL for$continue$%u\n", for_counter_local);

    // i += step
    printf("ADD LF@%s$%u LF@%s$%u LF@for$step$%u\n",
           iteration_var_name,
           iteration_var_id,
           iteration_var_name,
           iteration_var_id,
           for_counter_local);

    // if (i == to) end
    printf("JUMPIFEQ for$end$%u LF@%s$%u LF@for$to$%u\n",
           for_counter_local,
           iteration_var_name,
           iteration_var_id,
           for_counter_local);

    // cycle back up for next iteration
    printf("JUMP for$start$%u\n", for_counter_local);

    // end
    printf("LABEL for$end$%u\n", for_counter_local);
}

// generate if statement with optional else block
static void gen_if(AstNode *node) {
    unsigned if_counter_local = if_counter++;

    // condition
    gen_node(node->as.if_statement.condition);
    printf("PUSHS bool@false\n");
    printf("JUMPIFEQS else$%u\n", if_counter_local);

    // then block
    gen_node(node->as.if_statement.then_block);

    // flow
    if (node->as.if_statement.else_block != NULL) {
        printf("JUMP if$%u\n", if_counter_local);
    }
    printf("LABEL else$%u\n", if_counter_local);

    // else block
    if (node->as.if_statement.else_block != NULL) {
        gen_node(node->as.if_statement.else_block);
        printf("LABEL if$%u\n", if_counter_local);
    }
}

// generate var definition
// all locals are defined at the func start, so it's just an assignment
static void gen_variable_definition(AstNode *node) {
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
}

// generate assignment to an existing var
static void gen_assign(AstNode *node) {
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
}

// generate return, return value is passed on stack
static void gen_return(AstNode *node) {
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
}

// generate expression statement used as a statement
// all expressions (see AstKind) pass evaluated value on stack
static void gen_expression_statement(AstNode *node) {
    // recurse deeper
    gen_node(node->as.expression_statement.expression);
}

// push literal on data stack
static void gen_literal(AstNode *node) {
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
}

// push var value on data stack
static void gen_identifier(AstNode *node) {
    // just pushes value on the data stack
    printf("PUSHS LF@%s$%u\n", node->as.identifier.name, node->var_id);
}

// generate unary op (-, +, !)
static void gen_unary_operator(AstNode *node) {
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
}

// generate binary op (+, -, <, >, ==, &&, ||, ...)
// handles int to float expansion and short circuit for && and ||
static void gen_binary_operator(AstNode *node) {
    AstNode *l = node->as.binary.left;
    AstNode *r = node->as.binary.right;

    // assert types
    switch (node->as.binary.operator_type) {
    case OP_SUB:
    case OP_DIV:
    case OP_MUL:
        assert(is_num(l) && is_num(r));
        break;
    case OP_ADD:
    case OP_LT:
    case OP_LE:
    case OP_GT:
    case OP_GE:
        assert((is_num(l) && is_num(r)) || (l->type == TYPE_STRING && r->type == TYPE_STRING));
        break;
    case OP_EQ:
    case OP_NE:
        assert((is_num(l) && is_num(r)) || l->type == r->type);
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
    unsigned skip_save = skip_counter++;
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
}

// returns true if node is type int or float
// useful for quick type assertion
static bool is_num(AstNode *node) {
    return node->type == TYPE_INT || node->type == TYPE_DOUBLE;
}

// generate func call
// handles arg eval and assignment to temp/local vars, frame push and pop
static void gen_call(AstNode *node) {
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
        // builtin labels start with $ so they don't collide with user funcs
        printf("CALL %s%s\n", callee->is_builtin ? "$" : "", node->as.call.name);
    } else {
        printf("CALL $EXTKW$%s$%u\n", node->as.call.name, callee->extends.type);
    }
    printf("POPFRAME\n");
}

// generate instructions of all builtin funcs
// builtin params use names without id (e.g.: LF@value)
static void gen_builtin_funcs(void) {
    // print(value: Any): Unit
    printf("LABEL $print\n"
           "PUSHS nil@nil\n"

           // check for nil
           "TYPE GF@tmp1 LF@value\n"
           "JUMPIFNEQ else$print GF@tmp1 string@nil\n"
           "WRITE string@()\n"
           "RETURN\n"

           // normal types
           "LABEL else$print\n"
           "WRITE LF@value\n"
           "RETURN\n");

    // readLine(): String
    printf("LABEL $readLine\n"
           "READ GF@tmp1 string\n"

           "JUMPIFEQ else$readLine GF@tmp1 nil@nil\n"
           "PUSHS GF@tmp1\n"
           "RETURN\n"

           "LABEL else$readLine\n"
           "PUSHS string@\n"
           "RETURN\n");

    // readInt(): Int
    printf("LABEL $readInt\n"
           "READ GF@tmp1 int\n"

           "JUMPIFEQ else$readInt GF@tmp1 nil@nil\n"
           "PUSHS GF@tmp1\n"
           "RETURN\n"

           "LABEL else$readInt\n"
           "PUSHS int@0\n"
           "RETURN\n");

    // readDouble(): Double
    printf("LABEL $readDouble\n"
           "READ GF@tmp1 float\n"

           "JUMPIFEQ else$readDouble GF@tmp1 nil@nil\n"
           "PUSHS GF@tmp1\n"
           "RETURN\n"

           "LABEL else$readDouble\n"
           "PUSHS float@%a\n"
           "RETURN\n",
           0.0f);

    // readBoolean(): Bool
    printf("LABEL $readBoolean\n"
           "READ GF@tmp1 bool\n"

           "JUMPIFEQ else$readBoolean GF@tmp1 nil@nil\n"
           "PUSHS GF@tmp1\n"
           "RETURN\n"

           "LABEL else$readBoolean\n"
           "PUSHS bool@false\n"
           "RETURN\n");

    // toStr(value: Any): String
    // toString: String
    printf("LABEL $toStr\n"
           "LABEL $toString\n" // VALMEM
           "TYPE GF@tmp1 LF@value\n"
           "PUSHS LF@value\n"

           // string
           "JUMPIFNEQ else$toStr$string GF@tmp1 string@string\n"
           "RETURN\n"
           "LABEL else$toStr$string\n"

           // int
           "JUMPIFNEQ else$toStr$int GF@tmp1 string@int\n"
           "INT2STRS\n"
           "RETURN\n"
           "LABEL else$toStr$int\n"

           // float
           "JUMPIFNEQ else$toStr$float GF@tmp1 string@float\n"
           "FLOAT2STRS\n"
           "RETURN\n"
           "LABEL else$toStr$float\n"

           // bool
           "JUMPIFNEQ else$toStr$bool GF@tmp1 string@bool\n"
           "PUSHS bool@true\n"
           "JUMPIFEQS else$toStr$bool$true\n"
           "PUSHS string@false\n"
           "RETURN\n"
           "LABEL else$toStr$bool$true\n"
           "PUSHS string@true\n"
           "RETURN\n"
           "LABEL else$toStr$bool\n"

           // nil
           "POPS GF@tmp2\n"
           "PUSHS string@()\n"
           "RETURN\n");

    // toInt(value: Double): Int
    // Int.toInt: Int
    printf("LABEL $toInt\n"
           "PUSHS LF@value\n"
           "FLOAT2INTS\n"
           "RETURN\n");

    // strLength(value: String): Int
    // String.length: Int
    printf("LABEL $strLength\n"
           "LABEL $length\n" // VALMEM
           "STRLEN GF@tmp1 LF@value\n"
           "PUSHS GF@tmp1\n"
           "RETURN\n");

    // subStr(value: String, start: Int, end: Int): String
    printf("LABEL $subStr\n"

           // start < 0
           "LT GF@tmp1 LF@start int@0\n"
           "JUMPIFEQ else$subStr GF@tmp1 bool@true\n"

           // end < 0
           "LT GF@tmp1 LF@end int@0\n"
           "JUMPIFEQ else$subStr GF@tmp1 bool@true\n"

           // start > end
           "GT GF@tmp1 LF@start LF@end\n"
           "JUMPIFEQ else$subStr GF@tmp1 bool@true\n"

           // start >= strLength(value)
           "STRLEN GF@tmp2 LF@value\n"
           "GT GF@tmp1 LF@start GF@tmp2\n"
           "JUMPIFEQ else$subStr GF@tmp1 bool@true\n"
           "JUMPIFEQ else$subStr LF@start GF@tmp2\n"

           // end > strLength(value)
           "GT GF@tmp1 LF@end GF@tmp2\n"
           "JUMPIFEQ else$subStr GF@tmp1 bool@true\n"

           "MOVE GF@tmp1 string@\n"  // op1
           "MOVE GF@tmp2 string@\n"  // op2
           "MOVE GF@tmp3 LF@start\n" // index

           "LABEL while$start$subStr\n"
           "JUMPIFEQ while$end$subStr GF@tmp3 LF@end\n"

           "GETCHAR GF@tmp2 LF@value GF@tmp3\n"
           "CONCAT GF@tmp1 GF@tmp1 GF@tmp2\n"
           "ADD GF@tmp3 GF@tmp3 int@1\n"

           "JUMP while$start$subStr\n"
           "LABEL while$end$subStr\n"

           "PUSHS GF@tmp1\n"
           "RETURN\n"

           "LABEL else$subStr\n"
           "EXIT int@40\n"); // exit 40 - runtime error

    // ord(value: String, index: Int): Int
    printf("LABEL $ord\n"

           // index < 0
           "LT GF@tmp1 LF@index int@0\n"
           "JUMPIFEQ else$ord GF@tmp1 bool@true\n"

           // index >= strLength(value)
           "STRLEN GF@tmp2 LF@value\n"
           "GT GF@tmp1 LF@index GF@tmp2\n"
           "JUMPIFEQ else$ord GF@tmp1 bool@true\n"
           "JUMPIFEQ else$ord LF@index GF@tmp2\n"

           "STRI2INT GF@tmp1 LF@value LF@index\n"
           "PUSHS GF@tmp1\n"
           "RETURN\n"

           "LABEL else$ord\n"
           "PUSHS int@-1\n"
           "RETURN\n");

    // chr(code: Int): String
    printf("LABEL $chr\n"
           "PUSHS LF@code\n"
           "INT2CHARS\n"
           "RETURN\n");

    // Double.floor: Double
    printf("LABEL $floor\n"
           "FLOAT2INT GF@tmp1 LF@value\n"
           "INT2FLOAT GF@tmp1 GF@tmp1\n"

           // if truncating up
           "LT GF@tmp2 LF@value GF@tmp1\n"
           "JUMPIFNEQ else$floor GF@tmp2 bool@true\n"

           // then value =- 1
           "SUB GF@tmp1 GF@tmp1 float@%a\n"

           // else
           "LABEL else$floor\n"
           "PUSHS GF@tmp1\n"
           "RETURN\n",
           1.0f);
}