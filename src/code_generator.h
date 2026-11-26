/**
 * @file code_generator.h
 * @brief Header file for the code generator
 * @author Tomáš Neckař (xneckat00)
 */

#ifndef CODE_GENERATOR_H
#define CODE_GENERATOR_H

#include "ast.h"
#include "symtable.h"

// outputs raw IFJcode26 instructions to stdout
// based on the inputted AST and symtable
void generate_code(AstNode* ast, stab* st);

#endif