#include <stdio.h>
#include "parsing.h"
#include "semanticAnalyzer.h"
#include "preprocessing.h"

int main() {
    FILE *srcFile = fopen("file.c", "r");
    if (!srcFile) {
        perror("file.c");
        return 1;
    }

    FILE *tmp = fopen("tmp.txt", "w+");
    if (!tmp) {
        perror("tmp.txt");
        fclose(srcFile);
        return 1;
    }

    preprocess(srcFile, tmp);
    fseek(tmp, 0, SEEK_SET);

    FILE *out = fopen("ans.txt", "w");
    if (!out) {
        perror("ans.txt");
        fclose(tmp);
        fclose(srcFile);
        return 1;
    }

    parserInit(tmp);
    node *root = parseProgram();
    printParseTree(root, out, 0);

    int semErrors = doSemanticChecks(root, stderr);
    if (semErrors > 0) {
        fprintf(stderr, "Semantic analysis failed with %d error(s).\n", semErrors);
    }

    fclose(out);
    fclose(tmp);
    fclose(srcFile);
    return semErrors > 0 ? 1 : 0;
}
