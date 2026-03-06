/*
 * chim_lexer.h
 * Lexer/Tokenizer for the Chim programming language
 * Chim -> C -> gcc -> .exe
 *
 * Token types based on the periodic table, obviously
 */

#ifndef CHIM_LEXER_H
#define CHIM_LEXER_H
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ─────────────────────────────────────────
   Token types  (the periodic table of tokens)
   ───────────────────────────────────────── */
typedef enum
{
    /* Keywords / commands */
    TOK_PR, /* Pr  - print              */
    TOK_IN, /* In  - input              */
    TOK_FE, /* Fe  - function def       */
    TOK_RU, /* Ru  - run function       */
    TOK_RN, /* Rn  - return             */
    TOK_GE, /* Ge  - get (return value) */
    TOK_CO, /* Co  - condition (if)     */
    TOK_ER, /* Er  - error/exception    */
    TOK_ES, /* Es  - end success        */
    TOK_FL, /* Fl  - file operations    */

    /* Type keywords */
    TOK_S,      /* S  - string              */
    TOK_I,      /* I  - integer             */
    TOK_F,      /* F  - float               */
    TOK_C_TYPE, /* C  - char                */
    TOK_LI,     /* Li - list                */
    TOK_V,      /* V  - var (dynamic)       */
    TOK_LI_VAL, /* I(...) S(...) inside Li  */

    /* Literals */
    TOK_STRING, /* "hello"                  */
    TOK_CHAR,   /* 'A'                      */
    TOK_INT,    /* 42                       */
    TOK_FLOAT,  /* 3.14  (rare in the wild) */
    TOK_IDENT,  /* myVariable, myFunction   */

    /* Punctuation */
    TOK_LPAREN, /* (  */
    TOK_RPAREN, /* )  */
    TOK_DOT,    /* .  - statement terminator */
    TOK_PIPE,   /* |  - file op separator   */
    TOK_COMMA,  /* ,  */
    TOK_EQUALS, /* =  */

    /* Special */
    TOK_EOF,
    TOK_UNKNOWN
} TokenType;

typedef struct
{
    TokenType type;
    char value[512]; /* token text */
    int line;
} Token;

/* ─────────────────────────────────────────
   Lexer state
   ───────────────────────────────────────── */
typedef struct
{
    const char *src;
    int pos;
    int len;
    int line;
} Lexer;

static void lexer_init(Lexer *lex, const char *source)
{
    lex->src = source;
    lex->pos = 0;
    lex->len = (int)strlen(source);
    lex->line = 1;
}

static char lex_peek(Lexer *lex)
{
    if (lex->pos >= lex->len)
        return '\0';
    return lex->src[lex->pos];
}

static char lex_peek2(Lexer *lex)
{
    if (lex->pos + 1 >= lex->len)
        return '\0';
    return lex->src[lex->pos + 1];
}

static char lex_advance(Lexer *lex)
{
    char c = lex->src[lex->pos++];
    if (c == '\n')
        lex->line++;
    return c;
}

/* Skip whitespace and // comments */
static void lex_skip(Lexer *lex)
{
    while (lex->pos < lex->len)
    {
        char c = lex_peek(lex);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
            lex_advance(lex);
        }
        else if (c == '/' && lex_peek2(lex) == '/')
        {
            while (lex->pos < lex->len && lex_peek(lex) != '\n')
                lex_advance(lex);
        }
        else
        {
            break;
        }
    }
}

/* Map identifier string to keyword token type */
static TokenType keyword_type(const char *word)
{
    if (strcmp(word, "Pr") == 0)
        return TOK_PR;
    if (strcmp(word, "In") == 0)
        return TOK_IN;
    if (strcmp(word, "Fe") == 0)
        return TOK_FE;
    if (strcmp(word, "Ru") == 0)
        return TOK_RU;
    if (strcmp(word, "Rn") == 0)
        return TOK_RN;
    if (strcmp(word, "Ge") == 0)
        return TOK_GE;
    if (strcmp(word, "Co") == 0)
        return TOK_CO;
    if (strcmp(word, "Er") == 0)
        return TOK_ER;
    if (strcmp(word, "Es") == 0)
        return TOK_ES;
    if (strcmp(word, "Fl") == 0)
        return TOK_FL;
    if (strcmp(word, "S") == 0)
        return TOK_S;
    if (strcmp(word, "I") == 0)
        return TOK_I;
    if (strcmp(word, "F") == 0)
        return TOK_F;
    if (strcmp(word, "C") == 0)
        return TOK_C_TYPE;
    if (strcmp(word, "Li") == 0)
        return TOK_LI;
    if (strcmp(word, "V") == 0)
        return TOK_V;
    return TOK_IDENT;
}

/* Read one token from the lexer */
static Token lex_next(Lexer *lex)
{
    Token tok;
    tok.value[0] = '\0';
    tok.line = lex->line;

    lex_skip(lex);

    if (lex->pos >= lex->len)
    {
        tok.type = TOK_EOF;
        return tok;
    }

    char c = lex_peek(lex);

    /* String literal "..." */
    if (c == '"')
    {
        lex_advance(lex);
        int i = 0;
        while (lex->pos < lex->len && lex_peek(lex) != '"')
        {
            char ch = lex_advance(lex);
            if (ch == '\\')
            {
                tok.value[i++] = ch;
                if (lex->pos < lex->len)
                    tok.value[i++] = lex_advance(lex);
            }
            else
            {
                tok.value[i++] = ch;
            }
        }
        if (lex->pos < lex->len)
            lex_advance(lex); /* closing " */
        tok.value[i] = '\0';
        tok.type = TOK_STRING;
        return tok;
    }

    /* Char literal 'x' */
    if (c == '\'')
    {
        lex_advance(lex);
        int i = 0;
        while (lex->pos < lex->len && lex_peek(lex) != '\'')
        {
            tok.value[i++] = lex_advance(lex);
        }
        if (lex->pos < lex->len)
            lex_advance(lex); /* closing ' */
        tok.value[i] = '\0';
        tok.type = TOK_CHAR;
        return tok;
    }

    /* Number literal: integer or float
     * Decimal separator is '_' (not '.'), because '.' is the statement terminator.
     * So  3_14  means 3.14 in C,  42  means 42.
     * Examples:
     *   3_14159  -> 3.14159 (double)
     *   42       -> 42      (int)
     *   -1_5     -> -1.5    (double)
     */
    if (isdigit(c) || (c == '-' && isdigit(lex_peek2(lex))))
    {
        int i = 0;
        int is_float = 0;
        tok.value[i++] = lex_advance(lex);
        while (lex->pos < lex->len)
        {
            char next = lex_peek(lex);
            if (isdigit(next))
            {
                tok.value[i++] = lex_advance(lex);
            }
            else if (next == '_' && lex->pos + 1 < lex->len && isdigit(lex->src[lex->pos + 1]))
            {
                lex_advance(lex);     /* consume '_' */
                tok.value[i++] = '.'; /* emit '.'  into C value */
                is_float = 1;
            }
            else
            {
                break;
            }
        }
        tok.value[i] = '\0';
        tok.type = is_float ? TOK_FLOAT : TOK_INT;
        return tok;
    }

    /* Identifier or keyword */
    if (isalpha(c) || c == '_')
    {
        int i = 0;
        while (lex->pos < lex->len && (isalnum(lex_peek(lex)) || lex_peek(lex) == '_'))
        {
            tok.value[i++] = lex_advance(lex);
        }
        tok.value[i] = '\0';
        tok.type = keyword_type(tok.value);
        return tok;
    }

    /* Single-character tokens */
    lex_advance(lex);
    tok.value[0] = c;
    tok.value[1] = '\0';
    switch (c)
    {
    case '(':
        tok.type = TOK_LPAREN;
        break;
    case ')':
        tok.type = TOK_RPAREN;
        break;
    case '.':
        tok.type = TOK_DOT;
        break;
    case '|':
        tok.type = TOK_PIPE;
        break;
    case ',':
        tok.type = TOK_COMMA;
        break;
    case '=':
        tok.type = TOK_EQUALS;
        break;
    default:
        tok.type = TOK_UNKNOWN;
        break;
    }
    return tok;
}

/* ─────────────────────────────────────────
   Token stream  (we buffer all tokens upfront)
   ───────────────────────────────────────── */
typedef struct
{
    Token *tokens; /* heap allocated */
    int count;
    int pos;
    int cap;
} TokenStream;

static void ts_init(TokenStream *ts)
{
    ts->cap = 1024;
    ts->count = 0;
    ts->pos = 0;
    ts->tokens = (Token *)malloc(ts->cap * sizeof(Token));
}

static void ts_free(TokenStream *ts)
{
    free(ts->tokens);
    ts->tokens = NULL;
}

static void ts_push(TokenStream *ts, Token t)
{
    if (ts->count >= ts->cap)
    {
        ts->cap *= 2;
        ts->tokens = (Token *)realloc(ts->tokens, ts->cap * sizeof(Token));
    }
    ts->tokens[ts->count++] = t;
}

static void tokenize(TokenStream *ts, const char *source)
{
    Lexer lex;
    lexer_init(&lex, source);
    ts->count = 0;
    ts->pos = 0;
    while (1)
    {
        Token t = lex_next(&lex);
        ts_push(ts, t);
        if (t.type == TOK_EOF)
            break;
    }
}

static Token ts_peek(TokenStream *ts)
{
    if (ts->pos >= ts->count)
    {
        Token eof;
        eof.type = TOK_EOF;
        eof.value[0] = '\0';
        return eof;
    }
    return ts->tokens[ts->pos];
}

static Token ts_advance(TokenStream *ts)
{
    Token t = ts_peek(ts);
    if (ts->pos < ts->count)
        ts->pos++;
    return t;
}

static int ts_check(TokenStream *ts, TokenType type)
{
    return ts_peek(ts).type == type;
}

static Token ts_expect(TokenStream *ts, TokenType type, const char *ctx)
{
    Token t = ts_advance(ts);
    if (t.type != type)
    {
        fprintf(stderr, "[Chim] Parse error near '%s' (line %d): expected token %d, got %d (%s) in %s\n",
                t.value, t.line, type, t.type, t.value, ctx);
    }
    return t;
}

#endif /* CHIM_LEXER_H */