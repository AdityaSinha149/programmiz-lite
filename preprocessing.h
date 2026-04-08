#ifndef PREPROCESSING_H
#define PREPROCESSING_H

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

typedef struct {
    char name[100];
    char value[100];
} Macro;

static Macro macros[100];
static int macroCount = 0;

static void addMacro(const char *name, const char *value);
static const char* getMacroValue(const char *name);
static void preprocess(FILE *src, FILE * dst);

static void preprocess(FILE *src, FILE *dst) {
    int ch;
    char token[256];
    int tlen;

    while ((ch = fgetc(src)) != EOF) {

        if (ch == '/') {
            int next = fgetc(src);

            if (next == '/') {
                putc(' ', dst);
                putc(' ', dst);
                while ((ch = fgetc(src)) != EOF && ch != '\n')
                    putc(' ', dst);
                if (ch == '\n')
                    putc('\n', dst);
                continue;
            }

            if (next == '*') {
                putc(' ', dst);
                putc(' ', dst);
                int prev = 0;
                while ((ch = fgetc(src)) != EOF) {
                    if (ch == '\n')
                        putc('\n', dst);
                    else
                        putc(' ', dst);

                    if (prev == '*' && ch == '/')
                        break;
                    prev = ch;
                }
                continue;
            }

            putc('/', dst);
            ungetc(next, src);
            continue;
        }

        if (ch == '#') {
            char directive[20];
            int dlen = 0;

            directive[dlen++] = ch;

            while ((ch = fgetc(src)) != EOF && !isspace(ch)) {
                directive[dlen++] = ch;
            }
            directive[dlen] = '\0';

            if (strcmp(directive, "#include") == 0) {
                while(ch != '\n')
                    ch = fgetc(src);
                fputc(ch, dst);
                continue;
            } 
            if (strcmp(directive, "#define") == 0) {
                char name[100];
                char value[100];
                int i = 0;

                while (ch != EOF && isspace(ch))
                    ch = fgetc(src);

                while (ch != EOF && (isalnum(ch) || ch == '_')) {
                    name[i++] = ch;
                    ch = fgetc(src);
                }
                name[i] = '\0';

                while (ch != EOF && isspace(ch))
                    ch = fgetc(src);

                i = 0;
                while (ch != EOF && ch != '\n') {
                    value[i++] = ch;
                    ch = fgetc(src);
                }
                value[i] = '\0';

                addMacro(name, value);
                fputc(ch, dst);
                continue;
            }

            fputs(directive, dst);
            if (ch != EOF)
                putc(ch, dst);
            continue;
        }

        if (isalpha(ch) || ch == '_') {
            tlen = 0;
            token[tlen++] = ch;

            while ((ch = fgetc(src)) != EOF && (isalnum(ch) || ch == '_')) {
                token[tlen++] = ch;
            }
            token[tlen] = '\0';

            const char *val = getMacroValue(token);
            if (val)
                fputs(val, dst);
            else
                fputs(token, dst);

            if (ch != EOF)
                ungetc(ch, src);

            continue;
        }

        putc(ch, dst);
    }
}

static void addMacro(const char *name, const char *value) {
    if (macroCount >= 100) return;
    strcpy(macros[macroCount].name, name);
    strcpy(macros[macroCount].value, value);
    macroCount++;
}

static const char* getMacroValue(const char *name) {
    for (int i = 0; i < macroCount; i++) {
        if (strcmp(macros[i].name, name) == 0)
            return macros[i].value;
    }
    return NULL;
}

#endif
