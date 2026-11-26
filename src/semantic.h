/**
 * @file semantic.h
 * @brief Header file for Semantic analysis
 * @author Filip Radulovič (xradulf00)
 */

#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"
#include "symtable.h"

// return codes, refer to project specification
typedef enum {
    SEM_OK = 0,
    SEM_UNDEFINED = 3,
    SEM_REDEFINED = 4,
    SEM_BAD_CALL = 5,
    SEM_TYPE = 6,
    SEM_IMMUTABLE = 7,
    SEM_OTHER = 10,
    SEM_INTERNAL = 99,
} sem_res_t;

// initializes the analyser with a symtable created by stab_new()
// stab should be held by main.c for easier passing to code gen later
// st cannot be NULL
void semantic_init(stab* st);

// runs semantic analysis of the provided program ast
// return sem_res_t return code
sem_res_t semantic_analyse(AstNode* program);

#endif // SEMANTIC_H
