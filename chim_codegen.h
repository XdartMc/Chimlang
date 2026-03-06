/*
 * chim_codegen.h
 * Code generation buffer for Chim -> C output
 */

#ifndef CHIM_CODEGEN_H
#define CHIM_CODEGEN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ─────────────────────────────────────────
   Output buffer  (grows as needed)
   ───────────────────────────────────────── */
typedef struct
{
    char *buf;
    int len;
    int cap;
} CodeBuf;

static void cb_init(CodeBuf *cb)
{
    cb->cap = 65536;
    cb->buf = (char *)malloc(cb->cap);
    cb->buf[0] = '\0';
    cb->len = 0;
}

static void cb_free(CodeBuf *cb)
{
    free(cb->buf);
}

static void cb_grow(CodeBuf *cb, int needed)
{
    while (cb->len + needed + 1 > cb->cap)
    {
        cb->cap *= 2;
        cb->buf = (char *)realloc(cb->buf, cb->cap);
    }
}

static void cb_append(CodeBuf *cb, const char *s)
{
    int slen = (int)strlen(s);
    cb_grow(cb, slen);
    memcpy(cb->buf + cb->len, s, slen + 1);
    cb->len += slen;
}

static void cb_printf(CodeBuf *cb, const char *fmt, ...)
{
    char tmp[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    cb_append(cb, tmp);
}

/* ─────────────────────────────────────────
   Variable type tracking
   ───────────────────────────────────────── */
#define MAX_VARS 256

typedef enum
{
    VTYPE_S,
    VTYPE_I,
    VTYPE_F,
    VTYPE_C,
    VTYPE_V,
    VTYPE_UNKNOWN
} VarType;

typedef struct
{
    char name[64];
    VarType type;
    int is_list; /* 1 if declared with Li */
} VarEntry;

typedef struct
{
    VarEntry entries[MAX_VARS];
    int count;
} VarTable;

static void vt_init(VarTable *vt) { vt->count = 0; }

static void vt_set(VarTable *vt, const char *name, VarType type)
{
    for (int i = 0; i < vt->count; i++)
    {
        if (strcmp(vt->entries[i].name, name) == 0)
        {
            vt->entries[i].type = type;
            return;
        }
    }
    if (vt->count < MAX_VARS)
    {
        strncpy(vt->entries[vt->count].name, name, 63);
        vt->entries[vt->count].type = type;
        vt->entries[vt->count].is_list = 0;
        vt->count++;
    }
}

static void vt_set_list(VarTable *vt, const char *name, VarType elem_type)
{
    vt_set(vt, name, elem_type);
    for (int i = 0; i < vt->count; i++)
    {
        if (strcmp(vt->entries[i].name, name) == 0)
        {
            vt->entries[i].is_list = 1;
            return;
        }
    }
}

static VarType vt_get(VarTable *vt, const char *name)
{
    for (int i = 0; i < vt->count; i++)
    {
        if (strcmp(vt->entries[i].name, name) == 0)
            return vt->entries[i].type;
    }
    return VTYPE_UNKNOWN;
}

static int vt_is_list(VarTable *vt, const char *name)
{
    for (int i = 0; i < vt->count; i++)
    {
        if (strcmp(vt->entries[i].name, name) == 0)
            return vt->entries[i].is_list;
    }
    return 0;
}

static const char *vtype_fmt(VarType t)
{
    switch (t)
    {
    case VTYPE_S:
        return "%s";
    case VTYPE_I:
        return "%d";
    case VTYPE_F:
        return "%f";
    case VTYPE_C:
        return "%c";
    default:
        return "%s";
    }
}

static const char *vtype_ctype(VarType t)
{
    switch (t)
    {
    case VTYPE_S:
        return "char*";
    case VTYPE_I:
        return "int";
    case VTYPE_F:
        return "double";
    case VTYPE_C:
        return "char";
    default:
        return "void*";
    }
}

#endif /* CHIM_CODEGEN_H */