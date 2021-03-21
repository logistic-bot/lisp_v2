#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <readline/readline.h>

typedef enum {
    Error_OK = 0,
    Error_Syntax,
    Error_Unbound,
    Error_Args,
    Error_Type,
} Error;

struct Atom;
typedef int (*Builtin)(struct Atom args, struct Atom* result);

struct Atom {
    enum {
        AtomType_Nil,
        AtomType_Pair,
        AtomType_Symbol,
        AtomType_Integer,
        AtomType_Builtin,
        AtomType_Closure,
    } type;

    union {
        struct Pair* pair;
        const char* symbol;
        long integer;
        Builtin builtin;
    } value;
};

struct Pair {
    struct Atom atom[2];
};

typedef struct Atom Atom;

#define car(p) ((p).value.pair->atom[0])
#define cdr(p) ((p).value.pair->atom[1])
#define nilp(atom) ((atom).type == AtomType_Nil)

static const Atom nil = { AtomType_Nil };
static Atom sym_table = { AtomType_Nil };

Atom cons(Atom car_val, Atom cdr_val) {
    Atom p;

    p.type = AtomType_Pair;
    p.value.pair = malloc(sizeof(struct Pair));

    car(p) = car_val;
    cdr(p) = cdr_val;

    return p;
}

Atom make_int(long x) {
    Atom a;
    a.type = AtomType_Integer;
    a.value.integer = x;
    return a;
}

Atom make_sym(const char* s) {
    Atom a, p;

    p = sym_table;
    while (!nilp(p)) {
        a = car(p);
        if (strcmp(a.value.symbol, s) == 0) {
            return a;
        }
        p = cdr(p);
    }

    a.type = AtomType_Symbol;
    a.value.symbol = strdup(s);
    sym_table = cons(a, sym_table);

    return a;
}

int make_closure(Atom env, Atom args, Atom body, Atom* result) {
    Atom p;

    if (!listp(args) || !listp(body)) {
        return Error_Syntax;
    }

    // Check argument names are all symbols
    p = args;
    while (!nilp(p)) {
        if (car(p).type != AtomType_Symbol) {
            return Error_Type;
        }
        p = cdr(p);
    }

    *result = cons(env, cons(args, body));
    result->type = AtomType_Closure;

    return Error_OK;
}

void print_expr(Atom atom) {
    switch (atom.type) {
    case AtomType_Nil:
        printf("nil");
        break;
    case AtomType_Pair:
        putchar('(');
        print_expr(car(atom));
        atom = cdr(atom);
        while (!nilp(atom)) {
            if (atom.type == AtomType_Pair) {
                putchar(' ');
                print_expr(car(atom));
                atom = cdr(atom);
            } else {
                printf(" . ");
                print_expr(atom);
                break;
            }
        }
        putchar(')');
        break;
    case AtomType_Symbol:
        printf("%s", atom.value.symbol);
        break;
    case AtomType_Integer:
        printf("%ld", atom.value.integer);
        break;
    case AtomType_Builtin:
        printf("#<builtin:%p>", atom.value.builtin);
        break;
    }
}

int lex(const char* str, const char** start, const char** end) {
    const char* ws = " \t\n";
    const char* delim = "() \t\n";
    const char* prefix = "()";

    str += strspn(str, ws); // advance until we hit a char not in ws

    if (str[0] == '\0') {
        *start = *end = NULL;
        return Error_Syntax;
    }

    *start = str;
    if (strchr(prefix, str[0]) != NULL) {
        *end = str + 1;
    } else {
        *end = str + strcspn(str, delim);
    }

    return Error_OK;
}

int read_expr(const char* input, const char** end, Atom* result);

int parse_simple(const char* start, const char* end, Atom* result) {
    char *buf, *p;

    // Integer?
    long val = strtol(start, &p, 10);
    if (p == end) {
        result->type = AtomType_Integer;
        result->value.integer = val;
        return Error_OK;
    }

    // Nil or symbol
    buf = malloc(end - start + 1);
    p = buf;
    while (start != end) {
        *p++ = *start, ++start;
    }
    *p = '\0';

    if (strcmp(buf, "nil") == 0) {
        *result = nil;
    } else {
        *result = make_sym(buf);
    }

    free(buf);
    return Error_OK;
}

int read_list(const char* start, const char** end, Atom* result) {
    Atom p;

    *end = start;
    p = *result = nil;

    for (;;) {
        const char* token;
        Atom item;
        Error err;

        err = lex(*end, &token, end);
        if (err) {
            return err;
        }

        if (token[0] == ')') {
            return Error_OK;
        }

        if (token[0] == '.' && *end - token == 1) {
            // improper list
            if (nilp(p)) {
                return Error_Syntax;
            }

            err = read_expr(*end, end, &item);
            if (err) {
                return err;
            }

            cdr(p) = item;

            // read closing )
            err = lex(*end, &token, end);
            if (!err && token[0] != ')') {
                err = Error_Syntax;
            }
            return err;
        }

        err = read_expr(token, end, &item);
        if (err) {
            return err;
        }
        if (nilp(p)) {
            // first item
            *result = cons(item, nil);
            p = *result;
        } else {
            cdr(p) = cons(item, nil);
            p = cdr(p);
        }
    }
}

int read_expr(const char* input, const char** end, Atom* result) {
    const char* token;
    Error err;

    err = lex(input, &token, end);
    if (err) {
        return err;
    }

    if (token[0] == '(') {
        return read_list(*end, end, result);
    } else if (token[0] == ')') {
        return Error_Syntax;
    } else {
        return parse_simple(token, *end, result);
    }
}

Atom env_create(Atom parent) {
    return cons(parent, nil);
}

int env_get(Atom env, Atom symbol, Atom* result) {
    Atom parent = car(env);
    Atom bs = cdr(env);

    while (!nilp(bs)) {
        Atom b = car(bs);
        if (car(b).value.symbol == symbol.value.symbol) {
            *result = cdr(b);
            return Error_OK;
        }
        bs = cdr(bs);
    }

    if (nilp(parent)) {
        return Error_Unbound;
    }

    return env_get(parent, symbol, result);
}

int env_set(Atom env, Atom symbol, Atom value) {
    Atom bs = cdr(env);
    Atom b = nil;

    while (!nilp(bs)) {
        b = car(bs);
        if (car(b).value.symbol == symbol.value.symbol) {
            cdr(b) = value;
            return Error_OK;
        }
        bs = cdr(bs);
    }

    b = cons(symbol, value);
    cdr(env) = cons(b, cdr(env));

    return Error_OK;
}

int listp(Atom expr) {
    while (!nilp(expr)) {
        if (expr.type != AtomType_Pair) {
            return 0;
        }
        expr = cdr(expr);
    }
    return 1;
}

Atom copy_list(Atom list) {
    Atom a, p;

    if (nilp(list)) {
        return nil;
    }

    a = cons(car(list), nil);
    p = a;
    list = cdr(list);

    while (!nilp(list)) {
        cdr(p) = cons(car(list), nil);
        p = cdr(p);
        list = cdr(list);
    }

    return a;
}

int apply(Atom fn, Atom args, Atom* result) {
    Atom env, arg_names, body;

    if (fn.type == AtomType_Builtin) {
        return (*fn.value.builtin)(args, result);
    } else if (fn.type != AtomType_Closure) {
        return Error_Type;
    }

    env = env_create(car(fn));
    arg_names = car(cdr(fn));
    body = cdr(cdr(fn));

    // bind arguments
    while (!nilp(arg_names)) {
        if (nilp(args)) {
            return Error_Args;
        }
        env_set(env, car(arg_names), car(args));
        arg_names = cdr(arg_names);
        args = cdr(args);
    }
    if (!nilp(args)) {
        return Error_Args;
    }

    // eval body
    while (!nilp(body)) {
        Error err = eval_expr(car(body), env, result);
        if (err) {
            return err;
        }
        body = cdr(body);
    }

    return Error_OK;
}

int eval_expr(Atom expr, Atom env, Atom* result) {
    Atom op, args, p;
    Error err;

    if (expr.type == AtomType_Symbol) {
        return env_get(env, expr, result);
    } else if (expr.type != AtomType_Pair) {
        *result = expr;
        return Error_OK;
    }

    if (!listp(expr)) {
        return Error_Syntax;
    }

    op = car(expr);
    args = cdr(expr);

    if (op.type == AtomType_Symbol) {
        if (strcmp(op.value.symbol, "quote") == 0) {
            if (nilp(args) || !nilp(cdr(args))) {
                return Error_Args;
            }

            *result = car(args);
            return Error_OK;
        } else if (strcmp(op.value.symbol, "if") == 0) {
            Atom cond, val;

            if (nilp(args) || nilp(cdr(args)) || nilp(cdr(cdr(args))) || !nilp(cdr(cdr(cdr(args))))) {
                return Error_Args;
            }

            err = eval_expr(car(args), env, &cond);
            if (err) {
                return err;
            }

            val = nilp(cond) ? car(cdr(cdr(args))) : car(cdr(args));
            return eval_expr(val, env, result);
        } else if (strcmp(op.value.symbol, "lambda") == 0) {
            if (nilp(args) || nilp(cdr(args))) {
                return Error_Args;
            }

            return make_closure(env, car(args), cdr(args), result);
        } else if (strcmp(op.value.symbol, "define") == 0) {
            Atom sym, val;

            if (nilp(args) || nilp(cdr(args)) || !nilp(cdr(cdr(args)))) {
                return Error_Args;
            }

            sym = car(args);
            if (sym.type != AtomType_Symbol) {
                return Error_Type;
            }

            err = eval_expr(car(cdr(args)), env, &val);
            if (err) {
                return err;
            }

            *result = sym;
            return env_set(env, sym, val);
        }
    }

    /* evaluate operator */
    err = eval_expr(op, env, &op);
    if (err) {
        return err;
    }

    // eval arguments
    args = copy_list(args);
    p = args;
    while (!nilp(p)) {
        err = eval_expr(car(p), env, &car(p));
        if (err) {
            return err;
        }

        p = cdr(p);
    }

    return apply(op, args, result);
}

Atom make_builtin(Builtin fn) {
    Atom a;
    a.type = AtomType_Builtin;
    a.value.builtin = fn;
    return a;
}

int builtin_car(Atom args, Atom* result) {
    if (nilp(args) || !nilp(cdr(args))) {
        return Error_Args;
    }

    if (nilp(car(args))) {
        *result = nil;
    } else if (car(args).type != AtomType_Pair) {
        return Error_Type;
    } else {
        *result = car(car(args));
    }

    return Error_OK;
}

int builtin_cdr(Atom args, Atom* result) {
    if (nilp(args) || !nilp(cdr(args))) {
        return Error_Args;
    }

    if (nilp(car(args))) {
        *result = nil;
    } else if (car(args).type != AtomType_Pair) {
        return Error_Type;
    } else {
        *result = cdr(car(args));
    }

    return Error_OK;
}

int builtin_cons(Atom args, Atom* result) {
    if (nilp(args) || nilp(cdr(args)) || !nilp(cdr(cdr(args)))) {
        return Error_Args;
    }

    *result = cons(car(args), car(cdr(args)));
    return Error_OK;
}

int builtin_add(Atom args, Atom* result) {
    Atom a, b;

    if (nilp(args) || nilp(cdr(args)) || !nilp(cdr(cdr(args)))) {
        return Error_Args;
    }

    a = car(args);
    b = car(cdr(args));

    if (a.type != AtomType_Integer || b.type != AtomType_Integer) {
        return Error_Type;
    }

    *result = make_int(a.value.integer + b.value.integer);

    return Error_OK;
}

int builtin_subtract(Atom args, Atom* result) {
    Atom a, b;

    if (nilp(args) || nilp(cdr(args)) || !nilp(cdr(cdr(args)))) {
        return Error_Args;
    }

    a = car(args);
    b = car(cdr(args));

    if (a.type != AtomType_Integer || b.type != AtomType_Integer) {
        return Error_Type;
    }

    *result = make_int(a.value.integer - b.value.integer);

    return Error_OK;
}

int builtin_multiply(Atom args, Atom* result) {
    Atom a, b;

    if (nilp(args) || nilp(cdr(args)) || !nilp(cdr(cdr(args)))) {
        return Error_Args;
    }

    a = car(args);
    b = car(cdr(args));

    if (a.type != AtomType_Integer || b.type != AtomType_Integer) {
        return Error_Type;
    }

    *result = make_int(a.value.integer * b.value.integer);

    return Error_OK;
}

int builtin_divide(Atom args, Atom* result) {
    Atom a, b;

    if (nilp(args) || nilp(cdr(args)) || !nilp(cdr(cdr(args)))) {
        return Error_Args;
    }

    a = car(args);
    b = car(cdr(args));

    if (a.type != AtomType_Integer || b.type != AtomType_Integer) {
        return Error_Type;
    }

    *result = make_int(a.value.integer / b.value.integer);

    return Error_OK;
}

int builtin_numeq(Atom args, Atom* result) {
    Atom a, b;

    if (nilp(args) || nilp(cdr(args)) || !nilp(cdr(cdr(args)))) {
        return Error_Args;
    }

    a = car(args);
    b = car(cdr(args));

    if (a.type != AtomType_Integer || b.type != AtomType_Integer) {
        return Error_Type;
    }

    *result = (a.value.integer == b.value.integer) ? make_sym("t") : nil;

    return Error_OK;
}

int builtin_less(Atom args, Atom* result) {
    Atom a, b;

    if (nilp(args) || nilp(cdr(args)) || !nilp(cdr(cdr(args)))) {
        return Error_Args;
    }

    a = car(args);
    b = car(cdr(args));

    if (a.type != AtomType_Integer || b.type != AtomType_Integer) {
        return Error_Type;
    }

    *result = (a.value.integer < b.value.integer) ? make_sym("t") : nil;

    return Error_OK;
}

int main(int argc, char* argv[]) {
    Atom env;
    char* input;

    env = env_create(nil);

    // initial environment
    env_set(env, make_sym("car"), make_builtin(builtin_car));
    env_set(env, make_sym("cdr"), make_builtin(builtin_cdr));
    env_set(env, make_sym("cons"), make_builtin(builtin_cons));

    env_set(env, make_sym("+"), make_builtin(builtin_add));
    env_set(env, make_sym("-"), make_builtin(builtin_subtract));
    env_set(env, make_sym("*"), make_builtin(builtin_multiply));
    env_set(env, make_sym("/"), make_builtin(builtin_divide));

    env_set(env, make_sym("t"), make_sym("t"));

    env_set(env, make_sym("="), make_builtin(builtin_numeq));
    env_set(env, make_sym("<"), make_builtin(builtin_less));

    while ((input = readline("> ")) != NULL) {
        const char* p = input;
        Error err;
        Atom expr, result;

        err = read_expr(p, &p, &expr);

        if (!err) {
            err = eval_expr(expr, env, &result);
        }

        switch (err) {
        case Error_OK:
            print_expr(result);
            putchar('\n');
            break;
        case Error_Syntax:
            puts("Syntax error");
            break;
        case Error_Unbound:
            puts("Symbol not bound");
            break;
        case Error_Args:
            puts("Wrong number of arguments");
            break;
        case Error_Type:
            puts("Wrong type");
            break;
        }

        free(input);
    }

    return 0;
}
