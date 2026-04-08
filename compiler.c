#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "preprocessing.h"
#include "parsing.h"
#include "semanticAnalyzer.h"

#define RED "\x1b[31m"
#define RESET "\x1b[0m"

static int endsWith(const char *s, const char *suffix) {
    size_t ls = strlen(s);
    size_t lf = strlen(suffix);
    if (lf > ls) return 0;
    return strcmp(s + ls - lf, suffix) == 0;
}

static int hasSingleQuote(const char *s) {
    return s && strchr(s, '\'') != NULL;
}

static void printErr(const char *msg) {
    fprintf(stderr, RED "%s" RESET "\n", msg);
}

static void printFileLinesRed(FILE *f) {
    if (!f) return;
    rewind(f);

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\0') continue;
        fprintf(stderr, RED "%s" RESET, line);
    }
}

static int printCompilerErrorsOnly(const char *inputPath, const char *outputPath) {
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "gcc -w -x c '%s' -o '%s' 2>&1",
             inputPath, outputPath);

    FILE *p = popen(cmd, "r");
    if (!p) {
        printErr("Failed to start system compiler.");
        return 1;
    }

    char line[1024];
    int sawError = 0;
    while (fgets(line, sizeof(line), p)) {
        if (strstr(line, "error:") || strstr(line, "undefined reference") || strstr(line, "collect2: error")) {
            sawError = 1;
            fprintf(stderr, RED "%s" RESET, line);
        }
    }

    int rc = pclose(p);
    if (rc != 0 && !sawError) {
        printErr("Compilation failed.");
        return 1;
    }

    return rc == 0 ? 0 : 1;
}

int main(int argc, char **argv) {
    const char *inputPath = NULL;
    const char *outputPath = "a.out";

    if (argc < 2) {
        printErr("Usage: ./compiler <input>.psa [-o output]");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                printErr("Missing output file after -o");
                return 1;
            }
            outputPath = argv[++i];
        } else if (!inputPath) {
            inputPath = argv[i];
        } else {
            printErr("Usage: ./compiler <input>.psa [-o output]");
            return 1;
        }
    }

    if (!inputPath || !endsWith(inputPath, ".psa")) {
        printErr("Input must be a .psa file");
        return 1;
    }

    if (hasSingleQuote(inputPath) || hasSingleQuote(outputPath)) {
        printErr("Paths containing single quote (') are not supported.");
        return 1;
    }

    FILE *srcFile = fopen(inputPath, "r");
    if (!srcFile) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Cannot open input file: %s", inputPath);
        printErr(msg);
        return 1;
    }

    FILE *tmp = tmpfile();
    if (!tmp) {
        fclose(srcFile);
        printErr("Failed to create temporary stream.");
        return 1;
    }

    preprocess(srcFile, tmp);
    fseek(tmp, 0, SEEK_SET);

    parserInit(tmp);
    node *root = parseProgram();
    if (!root || root->type == PT_ERROR) {
        printErr("Parsing failed.");
        if (root && root->t) {
            char msg[512];
            const char *name = root->t->tokenName[0] ? root->t->tokenName : "<eof>";
            const char *value = root->t->tokenValue[0] ? root->t->tokenValue : "";
            if (value[0]) {
                snprintf(msg, sizeof(msg), "Near token '%s' (%s) at %d:%d",
                         value, name, root->t->row, root->t->col);
            } else {
                snprintf(msg, sizeof(msg), "Near token type %s at %d:%d",
                         name, root->t->row, root->t->col);
            }
            printErr(msg);
        }
        fclose(tmp);
        fclose(srcFile);
        return 1;
    }

    FILE *semErr = tmpfile();
    if (!semErr) {
        fclose(tmp);
        fclose(srcFile);
        printErr("Failed to create semantic diagnostics stream.");
        return 1;
    }

    int semErrors = doSemanticChecks(root, semErr);
    if (semErrors > 0) {
        printFileLinesRed(semErr);
        char msg[128];
        snprintf(msg, sizeof(msg), "Semantic analysis failed with %d error(s).", semErrors);
        printErr(msg);
        fclose(semErr);
        fclose(tmp);
        fclose(srcFile);
        return 1;
    }

    fclose(semErr);
    fclose(tmp);
    fclose(srcFile);

    // Native compile step after your frontend pass.
    // Produces only the requested output binary.
    return printCompilerErrorsOnly(inputPath, outputPath);
}
