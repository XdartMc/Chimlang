/*
 * chim.c
 * Main entry point for the Chim transpiler
 *
 * Usage: ./chimc input.chim [output_name]
 *
 * Pipeline:
 *   input.chim -> chimc -> generated.c -> gcc -> output(.exe on Windows, ELF on Linux)
 *
 * This is not a serious language. Seriously.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chim_lexer.h"
#include "chim_codegen.h"
#include "chim_parser.h"

/* ─────────────────────────────────────────
   Read entire file into a malloc'd buffer
   ───────────────────────────────────────── */
static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        fprintf(stderr, "[Chim] Cannot open file: %s\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *buf = (char *)malloc(size + 1);
    if (!buf)
    {
        fclose(f);
        return NULL;
    }
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    return buf;
}

/* ─────────────────────────────────────────
   Write buffer to file
   ───────────────────────────────────────── */
static int write_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "w");
    if (!f)
    {
        fprintf(stderr, "[Chim] Cannot write file: %s\n", path);
        return 0;
    }
    fputs(content, f);
    fclose(f);
    return 1;
}

/* ─────────────────────────────────────────
   Strip extension from filename
   "hello.chim" -> "hello"
   ───────────────────────────────────────── */
static void strip_ext(const char *src, char *dst, int dstlen)
{
    strncpy(dst, src, dstlen - 1);
    dst[dstlen - 1] = '\0';
    char *dot = strrchr(dst, '.');
    if (dot)
        *dot = '\0';
}

/* ─────────────────────────────────────────
   Main
   ───────────────────────────────────────── */
int main(int argc, char *argv[])
{
    /* Disable buffering - critical on Windows/PowerShell */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    printf("  ___  _     _           \n");
    printf(" / __|| |_  (_) _ __     \n");
    printf("| (__ | ' \\ | || '  \\  \n");
    printf(" \\___||_||_||_||_|_|_|  \n");
    printf("  Transpiler v0.2        \n");
    printf("  Chim -> C -> binary    \n");
    printf("================================\n\n");

    if (argc < 2)
    {
        printf("Usage: %s <input.chim> [output_name]\n\n", argv[0]);
        printf("Example:\n");
        printf("  %s hello.chim hello\n", argv[0]);
        printf("  -> Generates hello.c, compiles to ./hello\n");
        return 1;
    }

    const char *input_path = argv[1];

    /* Determine output name */
    char base_name[256];
    if (argc >= 3)
    {
        strncpy(base_name, argv[2], 255);
    }
    else
    {
        strip_ext(input_path, base_name, 256);
    }

    char c_path[512];
    snprintf(c_path, sizeof(c_path), "%s.c", base_name);

    /* ── Step 1: Read source ── */
    printf("[1/4] Reading %s ...\n", input_path);
    char *source = read_file(input_path);
    if (!source)
        return 1;

    /* ── Step 2: Tokenize ── */
    printf("[2/4] Tokenizing ...\n");
    TokenStream ts;
    ts_init(&ts);
    tokenize(&ts, source);
    free(source);

    printf("      Tokens: %d\n", ts.count);

    /* ── Step 3: Parse & generate C ── */
    printf("[3/4] Transpiling to C ...\n");
    Parser parser;
    parser_init(&parser, &ts);
    parse_program(&parser);

    CodeBuf output;
    assemble_output(&parser, &output);

    if (!write_file(c_path, output.buf))
    {
        parser_free(&parser);
        cb_free(&output);
        return 1;
    }
    printf("      Written: %s (%d bytes of glorious C)\n", c_path, output.len);

    /* ── Step 4: Compile with gcc ── */
    printf("[4/4] Compiling with gcc ...\n");

    /* Detect if gcc is available at all */
    if (system("gcc --version > /dev/null 2>&1") != 0 &&
        system("gcc --version > nul 2>&1") != 0)
    {
        fprintf(stderr, "\n  [Chim] ERROR: gcc not found in PATH!\n");
        fprintf(stderr, "  Install MinGW-w64 (Windows) or gcc (Linux/Mac)\n");
        fprintf(stderr, "  The .c file was written to: %s\n", c_path);
        fprintf(stderr, "  You can compile it manually: gcc %s -o %s\n", c_path, base_name);
        parser_free(&parser);
        cb_free(&output);
        return 1;
    }

    char gcc_cmd[1024];
    char out_path[512];

    /* Always append .exe on Windows, detect via _WIN32 or COMSPEC env var */
#if defined(_WIN32) || defined(_WIN64) || defined(__MINGW32__) || defined(__MINGW64__)
    snprintf(out_path, sizeof(out_path), "%s.exe", base_name);
#else
    /* Also check at runtime for Windows via env variable */
    if (getenv("COMSPEC") != NULL)
        snprintf(out_path, sizeof(out_path), "%s.exe", base_name);
    else
        snprintf(out_path, sizeof(out_path), "%s", base_name);
#endif

    snprintf(gcc_cmd, sizeof(gcc_cmd), "gcc \"%s\" -o \"%s\" -w", c_path, out_path);
    printf("      Command: %s\n", gcc_cmd);

    int ret = system(gcc_cmd);

    if (ret == 0)
    {
        printf("\n  Compiled successfully!\n");
        printf("  Output:  %s\n", out_path);
        printf("  Run it:  .\\%s\n", out_path);
    }
    else
    {
        printf("\n  Compilation failed (gcc returned %d)\n", ret);
        printf("  The generated C file is at: %s\n", c_path);
        printf("  Try running manually: gcc %s -o %s\n", c_path, out_path);
        printf("  (It's probably our fault, not yours.)\n");
    }

    printf("\n================================\n");

    parser_free(&parser);
    cb_free(&output);
    ts_free(&ts);
    return ret == 0 ? 0 : 1;
}