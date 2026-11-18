/**
 * @file symtable.h
 * @brief Header file for Symtable ADT
 * @author Filip Radulovič (xradulf00)
 */

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include "types.h"
#include <stdbool.h>

typedef struct stab stab;

typedef unsigned long stab_size_t;

typedef struct stab_func stab_func;

typedef void (*stab_foreach_fn)(stab_func *fn, void *ctx);

typedef enum {
    ST_OK,
    ST_REDEFINED, // defined twice - error 4
    ST_NOMEM      // memory allocation failed - error 99
} stab_res_t;

typedef enum {
    BI_PRINT,
    BI_READLINE,
    BI_READINT,
    BI_READDOUBLE,
    BI_READBOOLEAN, // BOOLTHEN
    BI_TOSTR,
    BI_TOINT,
    BI_STRLENGTH,
    BI_SUBSTR,
    BI_ORD,
    BI_CHR,
    BI_M_TOSTRING,
    BI_M_TOINT,
    BI_M_FLOOR,
    BI_M_LENGTH
} stab_builtin_t;

// variable record, owned by function.locals
typedef struct {
    const char *name;
    DataType type;
    bool is_mutable; // error 7
    unsigned id;
} stab_var;

typedef enum { INFER_NONE, INFER_RUNNING, INFER_DONE } stab_infer_t;

// parameter record, owned by function record
typedef struct {
    const char *name;
    DataType type;
} stab_param;

typedef struct {
    stab_param *params;
    stab_size_t count;
} stab_param_list;

// function / method record
// name, is_method and receiver.type are used as key for hashtable
// changing them will make the record non accessible
struct stab_func {
    const char *name;
    stab_param_list *param_lists;
    stab_size_t list_count;
    DataType ret_type; // can be unknown (fns dont have to include return)
    bool ret_explicit;
    bool is_builtin;        // built in fns for code gen later
    stab_builtin_t builtin; // type of built in function (only for is_builtin = true)
    bool is_main;
    stab_var **locals; // used for code gen declarations
    stab_size_t locals_count;
    stab_infer_t infer;
    bool is_method;
    stab_param receiver; // EXTKW extension (only for is_method = true)
};

// creates new symbol table, otherwise returns null
// builtin functions are already registered
stab *stab_new(void);

// frees symbol table and everything in it
void stab_free(stab *st);

// adds function or method to symtable, returns stab_func or null
// receiver == null -> normal function
// specific reason for failure can be found in *res
// redefining a function, even a builtin one, will result into ST_REDEFINED
stab_func *stab_add_func(stab *st,
                         const char *name,
                         const stab_param_list *param_lists,
                         stab_size_t list_count,
                         DataType ret_type,
                         bool ret_explicit,
                         const stab_param *receiver,
                         stab_res_t *res);

// finds function or method by name, otherwise returns null
// receiver == null -> normal function
// if no function with given receiver exists, falls back to receiver.type TYPE_ANY
stab_func *stab_find_func(const stab *st, const char *name, const DataType *receiver);

// returns the main fn or null, if main was not set
stab_func *stab_main(const stab *st);

// declares function as main and checks for a redefinition
stab_res_t stab_set_main(stab *st, stab_func *fn);

// enters new scope (while, if else etc.) and return result type
stab_res_t stab_enter_scope(stab *st);

// leaves scope
// declared variables can no longer be found by name and are kept only in function.locals
void stab_leave_scope(stab *st);

// enters function or method scope and returns result type
// does NOT delclare parameters, use stab_declare_var after
// scopes are forgotten after leaving them but are retained in locals for codegen
// re-entering the same fn will lead to redefined vars in code gen later
stab_res_t stab_enter_func(stab *st, stab_func *fn);

// leaves function scope including any scopes open inside it
// declared variables can no longer be found by name and are kept only in function.locals
void stab_leave_func(stab *st);

// returns current function scope or null outside any function
stab_func *stab_current_func(const stab *st);

// declares var in current scope
// specific reason for failure can be found in *res
stab_var *stab_declare_var(stab *st, const char *name, DataType type, bool is_mutable, stab_res_t *res);

// creates new temporary var, used for for while if else etc., returns the var or null
// this variable is only found in function.locals
stab_var *stab_create_temp_var(stab *st, DataType type);

// finds var by name, otherwise null
// searches from current scope outwards while staying inside of the current function
stab_var *stab_find_var(const stab *st, const char *name);

// runs stab_foreach_fn for every function / method
// st and fn cannot be null
void stab_foreach_func(const stab *st, stab_foreach_fn fn, void *ctx);

#endif // SYMTABLE_H