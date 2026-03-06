/*
 * chim_parser.h
 * Parser + code generator for Chim
 * Walks token stream, emits C code
 *
 * Fixed:
 *   - Rn: Fe pre-scans body for Rn, emits char* return type
 *   - Li: implemented as C array with index access
 *   - V + Ge: emits a proper error message, not broken C
 */

#ifndef CHIM_PARSER_H
#define CHIM_PARSER_H
#pragma once

#include "chim_lexer.h"
#include "chim_codegen.h"

/* ─────────────────────────────────────────
   Function registry
   ───────────────────────────────────────── */
#define MAX_FUNCS 128

typedef struct
{
    char name[64];
    char params[8][64];
    int param_count;
    int has_return; /* 1 if body contains Rn -> return type is char* */
} FuncEntry;

typedef struct
{
    FuncEntry entries[MAX_FUNCS];
    int count;
} FuncTable;

static void ft_init(FuncTable *ft) { ft->count = 0; }

static void ft_add(FuncTable *ft, const char *name, char params[][64], int pc, int has_ret)
{
    if (ft->count >= MAX_FUNCS)
        return;
    FuncEntry *e = &ft->entries[ft->count++];
    strncpy(e->name, name, 63);
    e->param_count = pc;
    e->has_return = has_ret;
    for (int i = 0; i < pc; i++)
        strncpy(e->params[i], params[i], 63);
}

static FuncEntry *ft_find(FuncTable *ft, const char *name)
{
    for (int i = 0; i < ft->count; i++)
        if (strcmp(ft->entries[i].name, name) == 0)
            return &ft->entries[i];
    return NULL;
}

/* ─────────────────────────────────────────
   Parser state
   ───────────────────────────────────────── */
typedef struct
{
    TokenStream *ts;
    CodeBuf funcs;
    CodeBuf main;
    CodeBuf *target;
    VarTable vars;
    FuncTable ftable;
    int indent;
} Parser;

static void parser_init(Parser *p, TokenStream *ts)
{
    p->ts = ts;
    cb_init(&p->funcs);
    cb_init(&p->main);
    p->target = &p->main;
    vt_init(&p->vars);
    ft_init(&p->ftable);
    p->indent = 1;
}

static void parser_free(Parser *p)
{
    cb_free(&p->funcs);
    cb_free(&p->main);
}

static void emit(Parser *p, const char *fmt, ...)
{
    char tmp[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    for (int i = 0; i < p->indent * 4; i++)
        cb_append(p->target, " ");
    cb_append(p->target, tmp);
    cb_append(p->target, "\n");
}

static void emit_raw(Parser *p, const char *fmt, ...)
{
    char tmp[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    cb_append(p->target, tmp);
    cb_append(p->target, "\n");
}

/* Forward declaration */
static void parse_statement(Parser *p);

/* ─────────────────────────────────────────
   Parse a (block)
   ───────────────────────────────────────── */
static void parse_block(Parser *p)
{
    ts_expect(p->ts, TOK_LPAREN, "block");
    p->indent++;
    while (!ts_check(p->ts, TOK_RPAREN) && !ts_check(p->ts, TOK_EOF))
    {
        parse_statement(p);
        if (ts_check(p->ts, TOK_DOT))
            ts_advance(p->ts);
    }
    p->indent--;
    ts_expect(p->ts, TOK_RPAREN, "block end");
}

/* ─────────────────────────────────────────
   Pre-scan: does the upcoming (block...) contain Rn?
   Used to decide Fe return type before emitting it.
   ───────────────────────────────────────── */
static int prescan_has_rn(TokenStream *ts)
{
    /* Scan ALL upcoming (blocks) belonging to this Fe statement.
       Stops at a top-level DOT or EOF, NOT at closing paren,
       so it correctly sees Rn in the second/third block too.   */
    int depth = 0;
    for (int i = ts->pos; i < ts->count; i++)
    {
        TokenType t = ts->tokens[i].type;
        if (t == TOK_EOF)
            break;
        if (t == TOK_DOT && depth == 0)
            break;
        if (t == TOK_LPAREN)
        {
            depth++;
            continue;
        }
        if (t == TOK_RPAREN)
        {
            depth--;
            continue;
        }
        if (t == TOK_RN)
            return 1;
    }
    return 0;
}

/* ─────────────────────────────────────────
   String interpolation helper
   "Hello, {name}!" -> printf("Hello, %s!\n", name)
   Supports {var} for any declared variable type.
   ───────────────────────────────────────── */
static void emit_interpolated_string(Parser *p, const char *s)
{
    /* Build format string and argument list */
    char fmt[2048] = "";
    char args[2048] = "";
    int i = 0;
    int slen = (int)strlen(s);
    int has_args = 0;

    while (i < slen)
    {
        if (s[i] == '{')
        {
            /* Find closing } */
            int j = i + 1;
            while (j < slen && s[j] != '}')
                j++;
            if (j < slen)
            {
                /* Extract variable name */
                char varname[64] = "";
                int vlen = j - i - 1;
                if (vlen > 0 && vlen < 63)
                {
                    strncpy(varname, s + i + 1, vlen);
                    varname[vlen] = '\0';
                }
                VarType vt = vt_get(&p->vars, varname);
                const char *spec = vtype_fmt(vt); /* %s %d %f %c */
                strncat(fmt, spec, sizeof(fmt) - strlen(fmt) - 1);
                if (has_args)
                    strncat(args, ", ", sizeof(args) - strlen(args) - 1);
                strncat(args, varname, sizeof(args) - strlen(args) - 1);
                has_args = 1;
                i = j + 1;
                continue;
            }
        }
        /* Escape special printf chars */
        if (s[i] == '%')
            strncat(fmt, "%%", sizeof(fmt) - strlen(fmt) - 1);
        else
        {
            char tmp[2] = {s[i], '\0'};
            strncat(fmt, tmp, sizeof(fmt) - strlen(fmt) - 1);
        }
        i++;
    }

    if (has_args)
        emit(p, "printf(\"%s\\n\", %s);", fmt, args);
    else
        emit(p, "printf(\"%%s\\n\", \"%s\");", fmt);
}

/* ─────────────────────────────────────────
   Pr
   Handles:
     Pr "string"
     Pr "Hello, {name}!"   <- string interpolation
     Pr varname
     Pr myList(0)          <- list index access
   ───────────────────────────────────────── */
static void parse_pr(Parser *p)
{
    Token t = ts_advance(p->ts);
    if (t.type == TOK_STRING)
    {
        /* Check if string contains {var} interpolation */
        if (strchr(t.value, '{') != NULL)
            emit_interpolated_string(p, t.value);
        else
            emit(p, "printf(\"%%s\\n\", \"%s\");", t.value);
    }
    else if (t.type == TOK_IDENT)
    {
        /* Peek: is next token '(' and var is a list? -> index access */
        if (ts_check(p->ts, TOK_LPAREN) && vt_is_list(&p->vars, t.value))
        {
            ts_advance(p->ts); /* consume '(' */
            Token idx = ts_advance(p->ts);
            ts_expect(p->ts, TOK_RPAREN, "list index");
            VarType elem_type = vt_get(&p->vars, t.value);
            const char *fmt = vtype_fmt(elem_type);
            emit(p, "printf(\"%s\\n\", %s[%s]);", fmt, t.value, idx.value);
        }
        else
        {
            VarType vt = vt_get(&p->vars, t.value);
            const char *fmt = vtype_fmt(vt);
            emit(p, "printf(\"%s\\n\", %s);", fmt, t.value);
        }
    }
    else
    {
        emit(p, "printf(\"%%s\\n\", \"%s\");", t.value);
    }
}

/* ─────────────────────────────────────────
   Variable declarations: S I F C V
   ───────────────────────────────────────── */
static void parse_var_decl(Parser *p, TokenType type_tok)
{
    ts_expect(p->ts, TOK_LPAREN, "var decl");
    Token name_tok = ts_expect(p->ts, TOK_IDENT, "var name");
    ts_expect(p->ts, TOK_RPAREN, "var decl");
    const char *name = name_tok.value;

    /* In (input) */
    if (ts_check(p->ts, TOK_IN))
    {
        ts_advance(p->ts);
        Token prompt = ts_expect(p->ts, TOK_STRING, "In prompt");
        if (type_tok == TOK_S)
        {
            vt_set(&p->vars, name, VTYPE_S);
            emit(p, "char %s[256];", name);
            emit(p, "printf(\"%%s\", \"%s\");", prompt.value);
            emit(p, "fgets(%s, 256, stdin);", name);
            emit(p, "%s[strcspn(%s, \"\\n\")] = 0;", name, name);
        }
        else if (type_tok == TOK_I)
        {
            vt_set(&p->vars, name, VTYPE_I);
            emit(p, "int %s;", name);
            emit(p, "printf(\"%%s\", \"%s\");", prompt.value);
            emit(p, "scanf(\"%%d\", &%s);", name);
        }
        else if (type_tok == TOK_F)
        {
            vt_set(&p->vars, name, VTYPE_F);
            emit(p, "double %s;", name);
            emit(p, "printf(\"%%s\", \"%s\");", prompt.value);
            emit(p, "scanf(\"%%lf\", &%s);", name);
        }
        else
        {
            emit(p, "/* In: unsupported type */");
        }
        return;
    }

    /* V(x) Ge funcname  ->  error: not supported */
    if (type_tok == TOK_V)
    {
        Token next = ts_advance(p->ts);
        if (next.type == TOK_GE)
        {
            Token fname = ts_advance(p->ts);
            /* Emit runtime error, not broken C */
            emit(p, "/* V + Ge: Function return value can not keep in dynamic variable */");
            emit(p, "fprintf(stderr, \"[ChimError] Function return value can not keep in dynamic variable: %s = Ge %s\\n\");",
                 name, fname.value);
            emit(p, "exit(1);");
        }
        else
        {
            emit(p, "/* V: expected Ge after V(...) */");
        }
        return;
    }

    Token val = ts_advance(p->ts);

    switch (type_tok)
    {
    case TOK_S:
        vt_set(&p->vars, name, VTYPE_S);
        if (val.type == TOK_STRING)
            emit(p, "char %s[] = \"%s\";", name, val.value);
        else
            emit(p, "char %s[] = \"%s\";", name, val.value);
        break;
    case TOK_I:
        vt_set(&p->vars, name, VTYPE_I);
        emit(p, "int %s = %s;", name, val.value);
        break;
    case TOK_F:
        vt_set(&p->vars, name, VTYPE_F);
        emit(p, "double %s = %s;", name, val.value);
        break;
    case TOK_C_TYPE:
        vt_set(&p->vars, name, VTYPE_C);
        emit(p, "char %s = '%s';", name, val.value);
        break;
    default:
        emit(p, "/* unknown type decl */");
    }
}

/* ─────────────────────────────────────────
   Li - list declaration
   Li(name) I(1, 2, 3, 4)
   Li(name) S("a", "b", "c")
   Li(name) C('a', 'b', 'c') etc.
   ───────────────────────────────────────── */
static void parse_li(Parser *p)
{
    ts_expect(p->ts, TOK_LPAREN, "Li name");
    Token name_tok = ts_expect(p->ts, TOK_IDENT, "Li varname");
    ts_expect(p->ts, TOK_RPAREN, "Li name end");
    const char *name = name_tok.value;

    /* Expect the element type keyword: I, S, F, C */
    Token type_tok = ts_advance(p->ts);
    ts_expect(p->ts, TOK_LPAREN, "Li values");

    /* Collect values until ) */
    char values[64][512];
    int count = 0;
    VarType elem_type = VTYPE_UNKNOWN;

    if (type_tok.type == TOK_I)
        elem_type = VTYPE_I;
    else if (type_tok.type == TOK_F)
        elem_type = VTYPE_F;
    else if (type_tok.type == TOK_S)
        elem_type = VTYPE_S;
    else if (type_tok.type == TOK_C_TYPE)
        elem_type = VTYPE_C;

    while (!ts_check(p->ts, TOK_RPAREN) && !ts_check(p->ts, TOK_EOF))
    {
        if (ts_check(p->ts, TOK_COMMA))
        {
            ts_advance(p->ts);
            continue;
        }
        Token v = ts_advance(p->ts);
        if (v.type == TOK_STRING)
            snprintf(values[count++], 511, "\"%s\"", v.value);
        else
            snprintf(values[count++], 511, "%s", v.value);
    }
    ts_expect(p->ts, TOK_RPAREN, "Li values end");

    /* Determine C type and emit array */
    const char *c_type;
    switch (elem_type)
    {
    case VTYPE_I:
        c_type = "int";
        break;
    case VTYPE_F:
        c_type = "double";
        break;
    case VTYPE_C:
        c_type = "char";
        break;
    default:
        c_type = "char*";
        break;
    }

    /* Emit: int name[] = {1, 2, 3}; */
    char decl[4096];
    snprintf(decl, sizeof(decl), "%s %s[] = {", c_type, name);
    for (int i = 0; i < count; i++)
    {
        strncat(decl, values[i], sizeof(decl) - strlen(decl) - 1);
        if (i < count - 1)
            strncat(decl, ", ", sizeof(decl) - strlen(decl) - 1);
    }
    strncat(decl, "};", sizeof(decl) - strlen(decl) - 1);
    emit(p, "%s", decl);

    /* Register as list in var table, tracking element type */
    vt_set_list(&p->vars, name, elem_type);
}

/* ─────────────────────────────────────────
   Fe - function definition
   Fe(name, param1, param2) (body...) (body...)
   ───────────────────────────────────────── */
static void parse_fe(Parser *p)
{
    ts_expect(p->ts, TOK_LPAREN, "Fe args");

    char func_name[64];
    char params[8][64];
    int param_count = 0;

    Token fname = ts_expect(p->ts, TOK_IDENT, "Fe funcname");
    strncpy(func_name, fname.value, 63);

    while (ts_check(p->ts, TOK_COMMA))
    {
        ts_advance(p->ts);
        Token pname = ts_expect(p->ts, TOK_IDENT, "Fe param");
        strncpy(params[param_count++], pname.value, 63);
    }
    ts_expect(p->ts, TOK_RPAREN, "Fe args end");

    /* Pre-scan body blocks to detect Rn -> determines return type */
    int has_rn = prescan_has_rn(p->ts);

    /* Register (before parsing body so recursive calls work) */
    ft_add(&p->ftable, func_name, params, param_count, has_rn);

    /* Build header */
    const char *ret_type = has_rn ? "char*" : "void";
    char header[512];
    if (param_count == 0)
    {
        snprintf(header, sizeof(header), "%s %s(void)", ret_type, func_name);
    }
    else
    {
        snprintf(header, sizeof(header), "%s %s(", ret_type, func_name);
        for (int i = 0; i < param_count; i++)
        {
            char part[128];
            snprintf(part, sizeof(part), "char* %s%s", params[i],
                     i < param_count - 1 ? ", " : "");
            strncat(header, part, sizeof(header) - strlen(header) - 1);
        }
        strncat(header, ")", sizeof(header) - strlen(header) - 1);
    }

    /* Switch emit target to funcs buffer */
    CodeBuf *old_target = p->target;
    int old_indent = p->indent;
    p->target = &p->funcs;
    p->indent = 0;

    emit_raw(p, "%s {", header);
    p->indent = 1;

    while (ts_check(p->ts, TOK_LPAREN))
    {
        parse_block(p);
    }

    if (has_rn)
        emit(p, "return NULL;");

    p->indent = 0;
    emit_raw(p, "}");
    emit_raw(p, "");

    p->target = old_target;
    p->indent = old_indent;
}

/* ─────────────────────────────────────────
   Ru - call function
   Syntax A (old):   Ru funcName (arg1) (arg2)
   Syntax B (new):   Ru funcName "arg1" arg2
   Both styles can be mixed per argument.
   ───────────────────────────────────────── */
static void parse_ru(Parser *p)
{
    Token fname = ts_expect(p->ts, TOK_IDENT, "Ru funcname");
    FuncEntry *fe = ft_find(&p->ftable, fname.value);

    char args[8][512];
    int arg_count = 0;

    if (fe && fe->param_count > 0)
    {
        while (arg_count < fe->param_count)
        {
            TokenType next = ts_peek(p->ts).type;

            /* Syntax A: (arg) */
            if (next == TOK_LPAREN)
            {
                ts_advance(p->ts);
                Token arg = ts_advance(p->ts);
                if (arg.type == TOK_STRING)
                    snprintf(args[arg_count++], 510, "\"%s\"", arg.value);
                else
                    snprintf(args[arg_count++], 510, "%s", arg.value);
                ts_expect(p->ts, TOK_RPAREN, "Ru arg");

                /* Syntax B: "string" or varname directly after Ru funcname */
            }
            else if (next == TOK_STRING)
            {
                Token arg = ts_advance(p->ts);
                snprintf(args[arg_count++], 510, "\"%s\"", arg.value);
            }
            else if (next == TOK_IDENT)
            {
                Token arg = ts_advance(p->ts);
                snprintf(args[arg_count++], 510, "%s", arg.value);
            }
            else if (next == TOK_INT || next == TOK_FLOAT)
            {
                Token arg = ts_advance(p->ts);
                snprintf(args[arg_count++], 510, "%s", arg.value);
            }
            else
            {
                break; /* no more args */
            }
        }
    }

    /* Ru is always just a call - never auto-wraps in printf.
       If user wants output, they use Pr separately. */
    char call[1024];
    snprintf(call, sizeof(call), "%s(", fname.value);
    for (int i = 0; i < arg_count; i++)
    {
        strncat(call, args[i], sizeof(call) - strlen(call) - 1);
        if (i < arg_count - 1)
            strncat(call, ", ", sizeof(call) - strlen(call) - 1);
    }
    strncat(call, ");", sizeof(call) - strlen(call) - 1);
    emit(p, "%s", call);
}

/* ─────────────────────────────────────────
   Co - condition (if / else)
   Co(var=val, var2=val2) (then) (else)
   ───────────────────────────────────────── */
static void parse_co(Parser *p)
{
    ts_expect(p->ts, TOK_LPAREN, "Co cond");

    char cond_buf[1024] = "";
    int first = 1;

    while (!ts_check(p->ts, TOK_RPAREN) && !ts_check(p->ts, TOK_EOF))
    {
        if (!first && ts_check(p->ts, TOK_COMMA))
        {
            ts_advance(p->ts);
        }
        first = 0;

        Token lhs = ts_advance(p->ts);
        if (!ts_check(p->ts, TOK_EQUALS) && !ts_check(p->ts, TOK_LPAREN))
        {
            strncat(cond_buf, lhs.value, sizeof(cond_buf) - strlen(cond_buf) - 1);
            continue;
        }

        /* list(idx)=val  ->  list[idx] == val */
        char lhs_expr[128];
        VarType vt;
        if (ts_check(p->ts, TOK_LPAREN) && vt_is_list(&p->vars, lhs.value))
        {
            ts_advance(p->ts); /* ( */
            Token idx = ts_advance(p->ts);
            ts_expect(p->ts, TOK_RPAREN, "Co list index");
            snprintf(lhs_expr, sizeof(lhs_expr), "%s[%s]", lhs.value, idx.value);
            vt = vt_get(&p->vars, lhs.value);
        }
        else
        {
            snprintf(lhs_expr, sizeof(lhs_expr), "%s", lhs.value);
            vt = vt_get(&p->vars, lhs.value);
        }

        ts_advance(p->ts); /* = */
        Token rhs = ts_advance(p->ts);

        char part[256];
        if (vt == VTYPE_S)
            snprintf(part, sizeof(part), "strcmp(%s, \"%s\") == 0", lhs_expr, rhs.value);
        else
            snprintf(part, sizeof(part), "%s == %s", lhs_expr, rhs.value);

        if (strlen(cond_buf) > 0)
            strncat(cond_buf, " && ", sizeof(cond_buf) - strlen(cond_buf) - 1);
        strncat(cond_buf, part, sizeof(cond_buf) - strlen(cond_buf) - 1);
    }
    ts_expect(p->ts, TOK_RPAREN, "Co cond end");

    emit(p, "if (%s) {", cond_buf);
    if (ts_check(p->ts, TOK_LPAREN))
        parse_block(p);
    emit(p, "}");

    if (ts_check(p->ts, TOK_LPAREN))
    {
        emit(p, "else {");
        parse_block(p);
        emit(p, "}");
    }
}

/* ─────────────────────────────────────────
   Er - error
   Er(Type) "message"
   ───────────────────────────────────────── */
static void parse_er(Parser *p)
{
    ts_expect(p->ts, TOK_LPAREN, "Er type");
    Token exc = ts_expect(p->ts, TOK_IDENT, "Er exception");
    ts_expect(p->ts, TOK_RPAREN, "Er type end");
    Token msg = ts_expect(p->ts, TOK_STRING, "Er message");
    emit(p, "fprintf(stderr, \"[%s] %s\\n\");", exc.value, msg.value);
    emit(p, "exit(1);");
}

/* ─────────────────────────────────────────
   Fl - file operations
   Fl("file") op
   Fl("file") op "arg"
   ───────────────────────────────────────── */
static void parse_fl(Parser *p)
{
    ts_expect(p->ts, TOK_LPAREN, "Fl filename");
    Token fname = ts_expect(p->ts, TOK_STRING, "Fl filename string");
    ts_expect(p->ts, TOK_RPAREN, "Fl filename end");

    Token op = ts_advance(p->ts);

    if (strcmp(op.value, "create") == 0)
    {
        emit(p, "{ FILE* _cf = fopen(\"%s\", \"w\"); if(_cf) fclose(_cf); }", fname.value);
    }
    else if (strcmp(op.value, "delete") == 0)
    {
        emit(p, "remove(\"%s\");", fname.value);
    }
    else if (strcmp(op.value, "write") == 0)
    {
        Token arg = ts_expect(p->ts, TOK_STRING, "Fl write arg");
        emit(p, "{ FILE* _cf = fopen(\"%s\", \"w\"); if(_cf){ fprintf(_cf, \"%%s\", \"%s\"); fclose(_cf); } }", fname.value, arg.value);
    }
    else if (strcmp(op.value, "writeLine") == 0)
    {
        Token arg = ts_expect(p->ts, TOK_STRING, "Fl writeLine arg");
        emit(p, "{ FILE* _cf = fopen(\"%s\", \"a\"); if(_cf){ fprintf(_cf, \"%%s\\n\", \"%s\"); fclose(_cf); } }", fname.value, arg.value);
    }
    else if (strcmp(op.value, "getText") == 0)
    {
        emit(p, "{ FILE* _cf = fopen(\"%s\", \"r\"); if(_cf){ char _cb[4096]; size_t _cn=fread(_cb,1,4095,_cf); _cb[_cn]='\\0'; printf(\"%%s\", _cb); fclose(_cf); } }", fname.value);
    }
    else if (strcmp(op.value, "getLines") == 0)
    {
        emit(p, "{ FILE* _cf = fopen(\"%s\", \"r\"); if(_cf){ char _cl[1024]; while(fgets(_cl,sizeof(_cl),_cf)) printf(\"%%s\",_cl); fclose(_cf); } }", fname.value);
    }
    else if (strcmp(op.value, "clearText") == 0)
    {
        emit(p, "{ FILE* _cf = fopen(\"%s\", \"w\"); if(_cf) fclose(_cf); }", fname.value);
    }
    else if (strcmp(op.value, "move") == 0)
    {
        Token arg = ts_expect(p->ts, TOK_STRING, "Fl move dest");
        emit(p, "rename(\"%s\", \"%s\");", fname.value, arg.value);
    }
    else if (strcmp(op.value, "copy") == 0)
    {
        Token arg = ts_expect(p->ts, TOK_STRING, "Fl copy dest");
        emit(p, "{ FILE *_s=fopen(\"%s\",\"rb\"),*_d=fopen(\"%s\",\"wb\"); if(_s&&_d){int _c; while((_c=fgetc(_s))!=EOF)fputc(_c,_d); fclose(_s);fclose(_d);} }", fname.value, arg.value);
    }
    else
    {
        emit(p, "/* Fl: unknown op '%s' */", op.value);
    }
}

/* ─────────────────────────────────────────
   Rn - return value from function
   Rn varname  /  Rn "string literal"
   Fe is declared char* when Rn is found (prescan_has_rn).
   Non-string types are serialised into a static buffer.
   ───────────────────────────────────────── */
static void parse_rn(Parser *p)
{
    Token val = ts_advance(p->ts);
    if (val.type == TOK_STRING)
    {
        emit(p, "return \"%s\";", val.value);
    }
    else
    {
        VarType vt = vt_get(&p->vars, val.value);
        if (vt == VTYPE_I)
            emit(p, "{ static char _rb[64]; snprintf(_rb,sizeof(_rb),\"%%d\",%s); return _rb; }", val.value);
        else if (vt == VTYPE_F)
            emit(p, "{ static char _rb[64]; snprintf(_rb,sizeof(_rb),\"%%f\",%s); return _rb; }", val.value);
        else if (vt == VTYPE_C)
            emit(p, "{ static char _rb[4]; _rb[0]=%s; _rb[1]='\\0'; return _rb; }", val.value);
        else
            emit(p, "return %s;", val.value);
    }
}

/* ─────────────────────────────────────────
   Statement dispatcher
   ───────────────────────────────────────── */
static void parse_statement(Parser *p)
{
    Token t = ts_peek(p->ts);

    switch (t.type)
    {
    case TOK_PR:
        ts_advance(p->ts);
        parse_pr(p);
        break;
    case TOK_S:
    case TOK_I:
    case TOK_F:
    case TOK_C_TYPE:
    case TOK_V:
        ts_advance(p->ts);
        parse_var_decl(p, t.type);
        break;
    case TOK_LI:
        ts_advance(p->ts);
        parse_li(p);
        break;
    case TOK_FE:
        ts_advance(p->ts);
        parse_fe(p);
        break;
    case TOK_RU:
        ts_advance(p->ts);
        parse_ru(p);
        break;
    case TOK_RN:
        ts_advance(p->ts);
        parse_rn(p);
        break;
    case TOK_CO:
        ts_advance(p->ts);
        parse_co(p);
        break;
    case TOK_ER:
        ts_advance(p->ts);
        parse_er(p);
        break;
    case TOK_FL:
        ts_advance(p->ts);
        parse_fl(p);
        break;
    case TOK_ES:
        ts_advance(p->ts);
        emit(p, "return 0;");
        break;
    case TOK_DOT:
        ts_advance(p->ts);
        break;
    case TOK_EOF:
        break;
    default:
        fprintf(stderr, "[Chim] Unknown statement '%s' (line %d)\n",
                t.value, t.line);
        ts_advance(p->ts);
        break;
    }
}

/* ─────────────────────────────────────────
   Parse whole program
   ───────────────────────────────────────── */
static void parse_program(Parser *p)
{
    while (!ts_check(p->ts, TOK_EOF))
        parse_statement(p);
}

/* ─────────────────────────────────────────
   Assemble final C output
   ───────────────────────────────────────── */
static void assemble_output(Parser *p, CodeBuf *out)
{
    cb_init(out);

    cb_append(out,
              "#include <stdio.h>\n"
              "#include <stdlib.h>\n"
              "#include <string.h>\n"
              "#include <stdint.h>\n"
              "\n"
              "/* ============================================ */\n"
              "/* Generated by Chim Transpiler v0.3           */\n"
              "/* This C code was not written by a human      */\n"
              "/* ============================================ */\n"
              "\n");

    /* Forward declarations with correct return types */
    for (int i = 0; i < p->ftable.count; i++)
    {
        FuncEntry *fe = &p->ftable.entries[i];
        const char *ret = fe->has_return ? "char*" : "void";
        cb_append(out, ret);
        cb_append(out, " ");
        cb_append(out, fe->name);
        cb_append(out, "(");
        if (fe->param_count == 0)
        {
            cb_append(out, "void");
        }
        else
        {
            for (int j = 0; j < fe->param_count; j++)
            {
                cb_append(out, "char* ");
                cb_append(out, fe->params[j]);
                if (j < fe->param_count - 1)
                    cb_append(out, ", ");
            }
        }
        cb_append(out, ");\n");
    }
    if (p->ftable.count > 0)
        cb_append(out, "\n");

    cb_append(out, p->funcs.buf);

    cb_append(out, "int main(void) {\n");
    cb_append(out, p->main.buf);

    /* Auto return if needed */
    int len = p->main.len;
    int has_ret = len > 7 &&
                  strstr(len > 40 ? p->main.buf + len - 40 : p->main.buf, "return") != NULL;
    if (!has_ret)
        cb_append(out, "    return 0;\n");

    cb_append(out, "}\n");
}

#endif /* CHIM_PARSER_H */