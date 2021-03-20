#include <malloc.h>

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
    Atom a;
    a.type = AtomType_Symbol;
    a.value.symbol = strdup(s);
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

int main(int argc, char *argv[])
{
    Atom a = cons(make_sym("foo"), cons(make_sym("y"), cons(make_int(1), nil)));
    print_expr(a);
    return 0;
}
