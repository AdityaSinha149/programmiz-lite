#include "lexAnalyszer.h"

static const char *lexemeFor(const token *t, char *buf, size_t bufSize) {
    if (t->tokenValue[0]) {
        return t->tokenValue;
    }
    if (t->tokenName[0]) {
        return t->tokenName;
    }
    snprintf(buf, bufSize, "-");
    return buf;
}

int main() {
    printf("Enter program to lexical analyse: ");
    char file[256];
    if (scanf("%255s", file) != 1) return 1;

    FILE *src = fopen(file, "r");
    if (!src) {
        perror("error");
        return 1;
    }

    FILE *tmp = fopen("tmp.txt", "w+");
    if (!tmp) {
        perror("error");
        fclose(src);
        return 1;
    }

    FILE *dst = fopen("ans.txt", "w+");
    if (!dst) {
        perror("error");
        fclose(src);
        fclose(tmp);
        return 1;
    }

    preprocess(src, tmp);
    fseek(tmp, 0, SEEK_SET);

    int row = 1;
    int col = 1;
    int lastRow = 0;

    while (1) {
        token t = getNextToken(tmp, &row, &col);
        if (!t.tokenName[0] && !t.tokenValue[0]) break;

        if (lastRow != 0 && t.row != lastRow) {
            fprintf(dst, "\n");
        }

        char fallback[4];
        const char *lex = lexemeFor(&t, fallback, sizeof(fallback));
        fprintf(dst, "<%s,%d,%d>", lex, t.row, t.col);
        lastRow = t.row;
    }

    fclose(src);
    fclose(tmp);
    fclose(dst);
    return 0;
}
