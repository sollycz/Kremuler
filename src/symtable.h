/**
 * @file symtable.h
 * @brief Header file for Symtable ADT
 * @author Filip Radulovič (xradulf00)
 * @todo Update nescesseary function definitions to work with new ast_node parameter in StabFunc
 */

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include "types.h"
#include <stdbool.h>

typedef struct Stab Stab;

typedef unsigned long StabSize;

typedef struct StabFunc StabFunc;

typedef enum {
    ST_OK,
    ST_REDEFINED, // defined twice - error 4
    ST_NOMEM // memory allocation failed - error 99
} StabRes;

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
} StabBuiltin;

// variable record, owned by function.locals
typedef struct {
    const char* name;
    DataType type;
    bool is_mutable; // error 7
    unsigned id;
    bool is_param;
} StabVar;

typedef enum { INFER_NONE,
    INFER_RUNNING,
    INFER_DONE } StabInfer;

// parameter record, owned by function record
typedef struct {
    const char* name;
    DataType type;
    unsigned id; // var id set by semantic. usefull for codegen.
} StabParam;

typedef struct {
    StabParam* params;
    StabSize count;
} StabParamList;

// function / method record
// name, is_extension and extends.type are used as key for hashtable
// changing them will make the record non accessible
struct StabFunc {
    const char* name;
    StabParamList* param_lists;
    StabSize list_count;
    DataType ret_type; // can be unknown (fns dont have to include return)
    bool ret_explicit;
    bool is_builtin; // built in fns for code gen later
    StabBuiltin builtin; // type of built in function (only for is_builtin = true)
    bool is_main;
    StabVar** locals; // used for code gen declarations
    StabSize locals_count;
    StabInfer infer;
    bool is_extension;
    StabParam extends; // EXTKW extension (only for is_extension = true)
    void* ast_node; // AST node of the definition
};

// creates new symbol table, otherwise returns null
// builtin functions are already registered
Stab* stab_new(void);

// frees symbol table and everything in it
void stab_free(Stab* st);

// adds function or method to symtable, returns StabFunc or null
// extends == null -> normal function
// specific reason for failure can be found in *res
// redefining a function, even a builtin one, will result into ST_REDEFINED
StabFunc* stab_add_func(Stab* st,
    const char* name,
    const StabParamList* param_lists,
    StabSize list_count,
    DataType ret_type,
    bool ret_explicit,
    const StabParam* extends,
    StabRes* res);

// finds function or method by name, otherwise returns null
// extends == null -> normal function
// if no function with given extends exists, falls back to extends.type TYPE_ANY
StabFunc* stab_find_func(const Stab* st, const char* name, const DataType* extends);

// returns the main fn or null, if main was not set
StabFunc* stab_main(const Stab* st);

// declares function as main and checks for a redefinition
StabRes stab_set_main(Stab* st, StabFunc* fn);

// enters new scope (while, if else etc.) and return result type
StabRes stab_enter_scope(Stab* st);

// leaves scope
// declared variables can no longer be found by name and are kept only in function.locals
void stab_leave_scope(Stab* st);

// enters function or method scope and returns result type
// does NOT delclare parameters, use stab_declare_var after
// scopes are forgotten after leaving them but are retained in locals for codegen
// re-entering the same fn will lead to redefined vars in code gen later
StabRes stab_enter_func(Stab* st, StabFunc* fn);

// leaves function scope including any scopes open inside it
// declared variables can no longer be found by name and are kept only in function.locals
void stab_leave_func(Stab* st);

// returns current function scope or null outside any function
StabFunc* stab_current_func(const Stab* st);

// declares var in current scope
// specific reason for failure can be found in *res
StabVar* stab_declare_var(Stab* st, const char* name, DataType type, bool is_mutable, StabRes* res, bool is_param);

// finds var by name, otherwise null
// searches from current scope outwards while staying inside of the current function
StabVar* stab_find_var(const Stab* st, const char* name);

#endif // SYMTABLE_H
