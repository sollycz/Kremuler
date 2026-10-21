#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stdbool.h>

typedef struct stab stab;

typedef unsigned long stab_size_t;

// waiting for ast.h, this is temporary
// variable types (int, char etc.)
typedef enum { TYPE_INT } stab_data_t;

typedef enum {
    ST_OK,
    ST_REDEFINED, // error 4
    ST_NOMEM      // error 99
} stab_res_t;

typedef struct {
    const char *name;
    stab_data_t type;
    bool is_mutable; // error 7
    unsigned id;
} stab_var;

typedef struct {
    stab_data_t *types;
    stab_size_t count;
} stab_params;

typedef struct {
    const char *name;
    stab_params params;
    stab_data_t ret_type; // can be unknown (fns dont have to include return)
    bool is_builtin;      // built in fns for code gen later
    bool is_main;
    stab_var **locals;
    stab_size_t locals_count;
} stab_func;

stab *stab_new(void);
void stab_free(stab *st);

stab_res_t stab_add_func(stab *st, const char *name, const stab_params *params, stab_data_t ret_type, bool is_main);
stab_func *stab_find_func(const stab *st, const char *name);
// returns the main fn
stab_func *stab_main(const stab *st);

// variable scope for if etc. inside the function
bool stab_enter_scope(stab *st);
void stab_leave_scope(stab *st);
// variables scope of the function (used for locals)
bool stab_enter_func(stab *st, stab_func *fn);
void stab_leave_func(stab *st);

stab_res_t stab_declare_var(stab *st, const char *name, stab_data_t type, bool is_mutable);
stab_var *stab_find_var(const stab *st, const char *name);

#endif // SYMTABLE_H