#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <readline/readline.h>

typedef enum
{
    Error_OK = 0,
    Error_Syntax,
    Error_Unbound,
    Error_Args,
    Error_Type,
} Error;

struct Atom
{
    enum
    {
        AtomType_Nil,
        AtomType_Pair,
        AtomType_Symbol,
        AtomType_Integer,
    } type;

    union
    {
        struct Pair *pair;
        const char *symbol;
        long integer;
    } value;
};

struct Pair
{
    struct Atom atom[2];
};

typedef struct Atom Atom;

#define car(p) ((p).value.pair->atom[0])
#define cdr(p) ((p).value.pair->atom[1])
#define nilp(atom) ((atom).type == AtomType_Nil)

static const Atom nil = {AtomType_Nil};
static Atom sym_table = {AtomType_Nil};

Atom cons(Atom car_val, Atom cdr_val)
{
    Atom p;

    p.type = AtomType_Pair;
    p.value.pair = malloc(sizeof(struct Pair));

    car(p) = car_val;
    cdr(p) = cdr_val;

    return p;
}

Atom make_int(long x)
{
    Atom a;
    a.type = AtomType_Integer;
    a.value.integer = x;
    return a;
}

Atom make_sym(const char *s)
{
    Atom a, p;

    p = sym_table;
    while (!nilp(p))
    {
        a = car(p);
        if (strcmp(a.value.symbol, s) == 0)
        {
            return a;
        }
        p = cdr(p);
    }

    a.type = AtomType_Symbol;
    a.value.symbol = strdup(s);
    sym_table = cons(a, sym_table);

    return a;
}

void print_expr(Atom atom)
{
    switch (atom.type)
    {
    case AtomType_Nil:
        printf("nil");
        break;
    case AtomType_Pair:
        putchar('(');
        print_expr(car(atom));
        atom = cdr(atom);
        while (!nilp(atom))
        {
            if (atom.type == AtomType_Pair)
            {
                putchar(' ');
                print_expr(car(atom));
                atom = cdr(atom);
            }
            else
            {
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
    }
}

int lex(const char *str, const char **start, const char **end)
{
    const char *ws = " \t\n";
    const char *delim = "() \t\n";
    const char *prefix = "()";

    str += strspn(str, ws); // advance until we hit a char not in ws

    if (str[0] == '\0')
    {
        *start = *end = NULL;
        return Error_Syntax;
    }

    *start = str;
    if (strchr(prefix, str[0]) != NULL)
    {
        *end = str + 1;
    }
    else
    {
        *end = str + strcspn(str, delim);
    }

    return Error_OK;
}

int read_expr(const char *input, const char **end, Atom *result);

int parse_simple(const char *start, const char *end, Atom *result)
{
    char *buf, *p;

    // Integer?
    long val = strtol(start, &p, 10);
    if (p == end)
    {
        result->type = AtomType_Integer;
        result->value.integer = val;
        return Error_OK;
    }

    // Nil or symbol
    buf = malloc(end - start + 1);
    p = buf;
    while (start != end)
    {
        *p++ = *start, ++start;
    }
    *p = '\0';

    if (strcmp(buf, "nil") == 0)
    {
        *result = nil;
    }
    else
    {
        *result = make_sym(buf);
    }

    free(buf);
    return Error_OK;
}

int read_list(const char *start, const char **end, Atom *result)
{
    Atom p;

    *end = start;
    p = *result = nil;

    for (;;)
    {
        const char *token;
        Atom item;
        Error err;

        err = lex(*end, &token, end);
        if (err)
        {
            return err;
        }

        if (token[0] == ')')
        {
            return Error_OK;
        }

        if (token[0] == '.' && *end - token == 1)
        {
            // improper list
            if (nilp(p))
            {
                return Error_Syntax;
            }

            err = read_expr(*end, end, &item);
            if (err)
            {
                return err;
            }

            cdr(p) = item;

            // read closing )
            err = lex(*end, &token, end);
            if (!err && token[0] != ')')
            {
                err = Error_Syntax;
            }
            return err;
        }

        err = read_expr(token, end, &item);
        if (err)
        {
            return err;
        }
        if (nilp(p))
        {
            // first item
            *result = cons(item, nil);
            p = *result;
        }
        else
        {
            cdr(p) = cons(item, nil);
            p = cdr(p);
        }
    }
}

int read_expr(const char *input, const char **end, Atom *result)
{
    const char *token;
    Error err;

    err = lex(input, &token, end);
    if (err)
    {
        return err;
    }

    if (token[0] == '(')
    {
        return read_list(*end, end, result);
    }
    else if (token[0] == ')')
    {
        return Error_Syntax;
    }
    else
    {
        return parse_simple(token, *end, result);
    }
}

Atom env_create(Atom parent)
{
    return cons(parent, nil);
}

int env_get(Atom env, Atom symbol, Atom *result)
{
    Atom parent = car(env);
    Atom bs = cdr(env);

    while (!nilp(bs))
    {
        Atom b = car(bs);
        if (car(b).value.symbol == symbol.value.symbol)
        {
            *result = cdr(b);
            return Error_OK;
        }
        bs = cdr(bs);
    }

    if (nilp(parent))
    {
        return Error_Unbound;
    }

    return env_get(parent, symbol, result);
}

int env_set(Atom env, Atom symbol, Atom value)
{
    Atom bs = cdr(env);
    Atom b = nil;

    while (!nilp(bs))
    {
        b = car(bs);
        if (car(b).value.symbol == symbol.value.symbol)
        {
            cdr(b) = value;
            return Error_OK;
        }
        bs = cdr(bs);
    }

    b = cons(symbol, value);
    cdr(env) = cons(b, cdr(env));

    return Error_OK;
}

int listp(Atom expr)
{
    while (!nilp(expr))
    {
        if (expr.type != AtomType_Pair)
        {
            return 0;
        }
        expr = cdr(expr);
    }
    return 1;
}

int eval_expr(Atom expr, Atom env, Atom *result)
{
    Atom op, args;
    Error err;

    if (expr.type == AtomType_Symbol)
    {
        return env_get(env, expr, result);
    }
    else if (expr.type != AtomType_Pair)
    {
        *result = expr;
        return Error_OK;
    }

    if (!listp(expr))
    {
        return Error_Syntax;
    }

    op = car(expr);
    args = cdr(expr);

    if (op.type == AtomType_Symbol)
    {
        if (strcmp(op.value.symbol, "quote") == 0)
        {
            if (nilp(args) || !nilp(cdr(args)))
            {
                return Error_Args;
            }

            *result = car(args);
            return Error_OK;
        }
        else if (strcmp(op.value.symbol, "define") == 0)
        {
            Atom sym, val;

            if (nilp(args) || nilp(cdr(args)) || !nilp(cdr(cdr(args))))
            {
                return Error_Args;
            }

            sym = car(args);
            if (sym.type != AtomType_Symbol)
            {
                return Error_Type;
            }

            err = eval_expr(car(cdr(args)), env, &val);
            if (err)
            {
                return err;
            }

            *result = sym;
            return env_set(env, sym, val);
        }
    }

    return Error_Syntax;
}

int main(int argc, char *argv[])
{
    Atom env;
    char *input;

    env = env_create(nil);

    while ((input = readline("> ")) != NULL)
    {
        const char *p = input;
        Error err;
        Atom expr, result;

        err = read_expr(p, &p, &expr);

        if (!err)
        {
            err = eval_expr(expr, env, &result);
        }

        switch (err)
        {
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

    Atom a = cons(make_sym("foo"), cons(make_sym("y"), cons(make_sym("y"), nil)));
    print_expr(a);
    return 0;
}
