#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stdbool.h>

typedef struct stab stab;

typedef unsigned long stab_size_t;

typedef enum {
    TYPE_UNKNOWN, // unknown type, should be derived later
    TYPE_INT,
    TYPE_DOUBLE,
    TYPE_STRING,
    TYPE_BOOL, // BOOLTHEN extension
    TYPE_UNIT,
    TYPE_ANY // only for builtin functions
} stab_type_t;

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

typedef struct {
    const char *name;
    stab_type_t type;
    bool is_mutable; // error 7 (tried writting into unwriteable var)
    unsigned id;
} stab_var;

typedef enum { INFER_NONE, INFER_RUNNING, INFER_DONE } stab_infer_t;

typedef struct {
    const char *name;
    stab_type_t type;
} stab_param;

typedef struct {
    stab_param *params;
    stab_size_t count;
} stab_param_list;

typedef struct {
    const char *name;
    stab_param_list *param_lists;
    stab_size_t list_count;
    stab_type_t ret_type; // can be unknown (fns dont have to include return)
    bool ret_explicit;
    bool is_builtin;        // built in fns for code gen later
    stab_builtin_t builtin; // type of built in function (only for is_builtin = true)
    bool is_main;
    stab_var **locals;
    stab_size_t locals_count;
    stab_infer_t infer;
    bool is_method;
    stab_param receiver; // EXTKW extension (only for is_method = true)
} stab_func;

// creates new symbol table, otherwise returns null
stab *stab_new(void);

// frees symbol table
void stab_free(stab *st);

// adds function or method to symtable, returns stab_func or null
// receiver == null -> normal function
// specific reason for failure can be found in *res
stab_func *stab_add_func(stab *st,
                         const char *name,
                         const stab_param_list *param_lists,
                         stab_size_t list_count,
                         stab_type_t ret_type,
                         bool ret_explicit,
                         const stab_param receiver,
                         stab_res_t *res);

// finds function or method by name, otherwise returns null
// receiver == null -> normal function
stab_func *stab_find_func(const stab *st, const char *name, const stab_type_t *receiver);

// returns the main fn
stab_func *stab_main(const stab *st);

// declares function as main and checks for a redefined main (error)
stab_res_t stab_set_main(stab *st, stab_func *fn);

// enters new scope (while, if else etc.) and return result type
stab_res_t stab_enter_scope(stab *st);

// leaves scope
void stab_leave_scope(stab *st);

// enters function or method scope and returns result type
stab_res_t stab_enter_func(stab *st, stab_func *fn);

// leaves function scope
void stab_leave_func(stab *st);

// returns current function scope
stab_func *stab_current_func(const stab *st);

// declares var in current scope
// specific reason for failure can be found in *res
stab_var *stab_declare_var(stab *st, const char *name, stab_type_t type, bool is_mutable, stab_res_t *res);

// creates new temporary var, used for for while if else etc., returns the var or null
stab_var *stab_create_temp_var(stab *st, stab_type_t type);

// finds var by name, otherwise null
stab_var *stab_find_var(const stab *st, const char *name);

#endif // SYMTABLE_H