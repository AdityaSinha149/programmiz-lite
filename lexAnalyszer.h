#ifndef LEXANALYSZER_H
#define LEXANALYSZER_H

#include "preprocessing.h"

//structs
typedef struct token{
    char tokenName [50];
    char tokenValue [50];
    char tokenType [50];
    char tokenReturnType [50];
    int row,col,size;
}token;

//global variables
static const char *keywords[] = {
    "auto", "break", "case", "char", "const",
    "continue", "default", "do", "double",
    "else", "enum", "extern", "float", "for",
    "goto", "if", "int", "long", "register",
    "return", "short", "signed", "sizeof",
    "static", "struct", "switch", "typedef",
    "union", "unsigned", "void", "volatile", "while"
};

static const char *types[] = {
    "void", "char", "short", "int", "long",
    "float", "double", "signed", "unsigned",
    "_Bool", "_Complex"
};

static char type[50] = "";

//Token identifying
static token isKeyword(int ch, FILE *src, int *row, int *col);
static token isIdentifier(int ch, FILE *src, int *row, int *col);

static token isOperator(int ch, FILE *src, int *row, int *col);

static token isRelationalOperator(int ch, FILE *src, int *row, int *col);
static token isArithmeticOperator(int ch, FILE *src, int *row, int *col);
static token isLogicalOperator(int ch, FILE *src, int *row, int *col);
static token isBitwiseOperator(int ch, FILE *src, int *row, int *col);
static token isConditionalOperator(int ch, int *row, int *col);
static token isAssignmentOperator(int ch, FILE *src, int *row, int *col);

static token isStringLiteral(int ch, FILE *src, int *row, int *col);

static token isNumber(int ch, FILE *src, int *row, int *col);

//Token Server
static token getNextToken(FILE *src, int *row, int *col);

//Helpers
static void PrintToken(token t, FILE *dst);
static void copyFile(FILE *src, FILE *dst);
static void postprocess(FILE *src, FILE *dst);
static int findSizeOf( char *word );
static int isType(char *s);

//Token Server
static token getNextToken(FILE *src, int *row, int *col){
    token curr;
    memset(&curr, 0, sizeof(curr));

    int ch;
    ch = fgetc(src);

    while (ch != EOF) {

        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
            if (ch == '\n') {
                (*row)++;
                *col = 1;
            } else {
                (*col)++;
            }
            ch = fgetc(src);
            continue;
        }

        if (isalpha(ch)) {
            curr = isKeyword(ch, src, row, col);
            if (curr.tokenName[0]) return curr;

            curr = isIdentifier(ch, src, row, col);
            if (curr.tokenName[0]) return curr;

        }
        
        else if (isdigit(ch)) {
            curr = isNumber(ch, src, row, col);
            return curr;
        }
        
        else if (ch == '"') {
            curr = isStringLiteral(ch, src, row, col);
            return curr;
        }

        else if (strchr("+-&|*/%<>!^?=", ch)) {
            curr = isOperator(ch, src, row, col);
            return curr;
        }

        else {//Symbol
            curr.col = *col;
            curr.row = *row;
            curr.tokenValue[0] = ch;
            strcpy(curr.tokenName, "symbol");
            (*col)++;
            if (ch == ';' || ch == '{')
                type[0] = '\0';
            return curr;
        }
        ch = fgetc(src);
    }
    return curr;
}

//Token identifying

static token isKeyword(int ch, FILE *src, int *row, int *col) {
    token curr;
    memset(&curr, 0, sizeof(curr));
    int c = 1;
    curr.col = *col;
    curr.row = *row;
    char word[50];
    int i = 0;
    word[i++] = (char)ch;

    while ((ch = fgetc(src)) != EOF) {
        c++;
        if (!isalpha(ch)) {
            fseek(src, -1, SEEK_CUR);
            c--;
            break;
        }
        if (i < (int)sizeof(word) - 1)
            word[i++] = ch;
    }

    word[i] = '\0';
    if ( isType ( word )) 
        strcpy( type, word );

    for (int k = 0; k < (int)(sizeof(keywords)/sizeof(keywords[0])); k++) {
        if (strcmp(word, keywords[k]) == 0) {
            strcpy(curr.tokenValue, word);
            strcpy(curr.tokenName, "keyword");
            *col += c;
            return curr;
        }
    }
    fseek(src, -c+1, SEEK_CUR);
    return curr;
}

static token isIdentifier(int ch, FILE *src, int *row, int *col) {
    token curr;
    memset(&curr, 0, sizeof(curr));

    curr.col = *col;
    curr.row = *row;
    char word[50];
    word[0] = ch;
    (*col)++;
    int i = 1;
    while (ch != EOF) {
        ch = fgetc(src);
        (*col)++;
        if(ch != '_' && !isalnum(ch)){
            fseek(src, -1, SEEK_CUR);
            (*col)--;
            strcpy(curr.tokenName, "id");
            
            word[i] = 0;
            strcpy ( curr.tokenValue, word );
            if ( isType ( word ) )
                strcpy( type, word);
            else {
                if ( strcmp ( word, "printf" ) == 0 ||
                        strcmp ( word, "scanf" ) == 0 )
                    return curr;
            ch = fgetc( src );
            if( ch == '(' ) {
                strcpy ( curr.tokenReturnType, type );
                fseek ( src, -1, SEEK_CUR );
            }
            else {
                strcpy ( curr.tokenType, type );
                if (ch != EOF)
                    fseek(src, -1, SEEK_CUR);
            }
            curr.size = findSizeOf( type );

            }
            return curr;
        }
        word[i++] = ch;
    }
   
    return curr;
}

static token isOperator(int ch, FILE *src, int *row, int *col) {
    token curr;
    memset(&curr, 0, sizeof(curr));

    curr = isRelationalOperator(ch, src, row, col);
    if (curr.tokenName[0]) return curr;

    curr = isArithmeticOperator(ch, src, row, col);
    if (curr.tokenName[0]) return curr;

    curr = isLogicalOperator(ch, src, row, col);
    if (curr.tokenName[0]) return curr;

    curr = isBitwiseOperator(ch, src, row, col);
    if (curr.tokenName[0]) return curr;

    curr = isConditionalOperator(ch, row, col);
    if (curr.tokenName[0]) return curr;

    curr = isAssignmentOperator(ch, src, row, col);
    if (curr.tokenName[0]) return curr;

    return curr;
}

static token isRelationalOperator(int ch, FILE *src, int *row, int *col) {
    token curr;
    memset(&curr, 0, sizeof(curr));
    if(ch != '=' && ch != '<' && ch != '>' && ch != '!')
        return curr;
    curr.col = *col;
    curr.row = *row;
    int prev = ch;
    ch = fgetc(src);

    if (ch == '='){
        strcpy(curr.tokenName, "relOp");
        curr.tokenValue[0] = (char)prev;
        curr.tokenValue[1] = '=';
        curr.tokenValue[2] = '\0';
        (*col) += 2;
        return curr;
    }
    else if (prev == '<' || prev == '>'){
        strcpy(curr.tokenName, "relOp");
        curr.tokenValue[0] = (char)prev;
        curr.tokenValue[1] = '\0';
        (*col)++;
        return curr;
    }
    else
        fseek(src, -1, SEEK_CUR);

    return curr;
}

static token isArithmeticOperator(int ch, FILE *src, int *row, int *col) {
    token curr;
    memset(&curr, 0, sizeof(curr));
    if(ch != '+' && ch != '-' && ch != '*' && ch != '/' && ch != '%')
        return curr;
    curr.col = *col;
    curr.row = *row;
    int prev = ch;
    ch = fgetc(src);

    if ((prev == '+' && ch == '+') || 
        (prev == '-' && ch == '-')){
        strcpy(curr.tokenName, "ariOp");
        curr.tokenValue[0] = (char)prev;
        curr.tokenValue[1] = (char)ch;
        curr.tokenValue[2] = '\0';
        (*col) += 2;
        return curr;
    }
    else{
        strcpy(curr.tokenName, "ariOp");
        curr.tokenValue[0] = (char)prev;
        curr.tokenValue[1] = '\0';
        (*col)++;
        fseek(src, -1, SEEK_CUR);
    }

    return curr;
}

static token isLogicalOperator(int ch, FILE *src, int *row, int *col) {
    token curr;
    memset(&curr, 0, sizeof(curr));
    if(ch != '&' && ch != '|' && ch != '!')
        return curr;
    curr.col = *col;
    curr.row = *row;
    int prev = ch;
    ch = fgetc(src);

    if (prev != '!' && ch == prev){
        strcpy(curr.tokenName, "logOp");
        curr.tokenValue[0] = (char)prev;
        curr.tokenValue[1] = (char)ch;
        curr.tokenValue[2] = '\0';
        (*col) += 2;
        return curr;
    }
    else if (ch == '!') {
        strcpy(curr.tokenName, "logOp");
        curr.tokenValue[0] = (char)prev;
        curr.tokenValue[1] = '\0';
        (*col)++;

        fseek(src, -1, SEEK_CUR);

        return curr;
    }
    else
        fseek(src, -1, SEEK_CUR);

    return curr;
}

static token isBitwiseOperator(int ch, FILE *src, int *row, int *col) {
    token curr;
    memset(&curr, 0, sizeof(curr));
    if(ch != '<' && ch != '>' && ch != '|' && ch != '&' && ch != '^' && ch != '~')
        return curr;
    curr.col = *col;
    curr.row = *row;
    int prev = ch;
    ch = fgetc(src);

    if (prev == '^' || prev == '~') {
        strcpy(curr.tokenName, "bitOp");
        curr.tokenValue[0] = (char)prev;
        curr.tokenValue[1] = '\0';
        (*col)++;
        return curr;
    }
    else if ((ch == '<' || ch == '>') && prev == ch){
        strcpy(curr.tokenName, "bitOp");
        curr.tokenValue[0] = (char)prev;
        curr.tokenValue[1] = (char)ch;
        curr.tokenValue[2] = '\0';
        (*col) += 2;
        return curr;
    }
    if ((prev == '&' || prev == '|') && prev != ch) {
        strcpy(curr.tokenName, "bitOp");
        curr.tokenValue[0] = (char)prev;
        curr.tokenValue[1] = '\0';
        (*col)++;

        fseek(src, -1, SEEK_CUR);
        return curr;
    }
    else
        fseek(src, -1, SEEK_CUR);

    return curr;
}

static token isConditionalOperator(int ch, int *row, int *col) {
    token curr;
    memset(&curr, 0, sizeof(curr));

    if (ch == '?' || ch == ':') {
        strcpy(curr.tokenName, "condOp");
        curr.tokenValue[0] = (char)ch;
        curr.tokenValue[1] = '\0';
        curr.row = *row;
        curr.col = *col;
        (*col)++;
    }

    return curr;
}

static token isAssignmentOperator(int ch, FILE *src, int *row, int *col) {
    token curr;
    memset(&curr, 0, sizeof(curr));

    if (ch != '=' && ch != '+' && ch != '-' && ch != '*' &&
        ch != '/' && ch != '%' && ch != '<' && ch != '>' &&
        ch != '&' && ch != '|' && ch != '^')
        return curr;

    curr.col = *col;
    curr.row = *row;

    int next = fgetc(src);

    if (ch == '=' && next != '=') {
        strcpy(curr.tokenName, "assignOp");
        curr.tokenValue[0] = (char)ch;
        curr.tokenValue[1] = '\0';
        (*col)++;

        if (next != EOF)
            fseek(src, -1, SEEK_CUR);

        return curr;
    }

    if (next == '=') {
        strcpy(curr.tokenName, "assignOp");
        curr.tokenValue[0] = (char)ch;
        curr.tokenValue[1] = '=';
        curr.tokenValue[2] = '\0';
        (*col) += 2;
        return curr;
    }

    if ((ch == '<' || ch == '>') && next == ch) {
        int next2 = fgetc(src);

        if (next2 == '=') {
            strcpy(curr.tokenName, "assignOp");
            curr.tokenValue[0] = (char)ch;
            curr.tokenValue[1] = (char)next;
            curr.tokenValue[2] = '=';
            curr.tokenValue[3] = '\0';
            (*col) += 3;
            return curr;
        }
        fseek(src, -2, SEEK_CUR);

        return curr;
    }

    if (next != EOF)
        fseek(src, -1, SEEK_CUR);

    return curr;
}

static token isStringLiteral(int ch, FILE *src, int *row, int *col) {
    token curr;
    memset(&curr, 0, sizeof(curr));
    curr.col = *col;
    curr.row = *row;
    strcpy(curr.tokenName, "stringLit");

    int i = 0;
    ch = fgetc(src);
    (*col)++;

    while (ch != EOF && ch != '"') {
        if (ch == '\\') {
            int esc = fgetc(src);
            if (esc == EOF) break;
            (*col)++;

            if (esc == 'n') ch = '\n';
            else if (esc == 't') ch = '\t';
            else if (esc == 'r') ch = '\r';
            else if (esc == '0') ch = '\0';
            else ch = esc;
        }

        if (i < (int)sizeof(curr.tokenValue) - 1) {
            curr.tokenValue[i++] = (char)ch;
        }

        if (ch == '\n') {
            (*row)++;
            *col = 1;
        } else {
            (*col)++;
        }

        ch = fgetc(src);
    }

    curr.tokenValue[i] = '\0';

    if (ch == '"') {
        (*col)++;
    }
    return curr;
}

static token isNumber(int ch, FILE *src, int *row, int *col) {
    token curr;
    memset(&curr, 0, sizeof(curr));
    curr.col = *col;
    curr.row = *row;
    int i = 0;

    int state = 1;
    int prev;

    while (state != 4) {
        prev = ch;

        if (ch != EOF) {
            ch = fgetc(src);
            (*col)++;
        }

        if (state == 1) {
            curr.tokenName[i++] = prev;
            if (isdigit(ch)) continue;
            else if (ch == 'e' || ch =='E') state = 3;
            else if (ch == '.') state = 2;
            else state = 4;
        }
        else if (state == 2) {
            curr.tokenName[i++] = prev;
            if (isdigit(ch)) state = 5;
            else state = 4;
        }
        else if (state == 3) {
            curr.tokenName[i++] = prev;
            if (isdigit(ch)) state = 6;
            else if (ch == '+' || ch == '-') state = 7;
            else state = 4;
        }
        else if (state == 5) {
            curr.tokenName[i++] = prev;
            if (isdigit(ch)) continue;
            if (ch == 'e' || ch =='E') state = 3;
            else state = 4;
        }
        else if (state == 6) {
            curr.tokenName[i++] = prev;
            if (isdigit(ch)) continue;
            else state = 4;
        }
        else {
            curr.tokenName[i++] = prev;
            if (isdigit(ch)) state = 6;
            else state = 4;
        }
    }

    fseek(src, -1, SEEK_CUR);

    curr.tokenName[i] = '\0';
    return curr;
}

static void PrintToken(token t, FILE *dst) {
    fprintf(dst, "<%s,%d,%d>", t.tokenName, t.row, t.col);
}

static void copyFile(FILE *src, FILE *dst) {
    int ch;
    while((ch = fgetc(src)) != EOF) {
        putc(ch, dst);
    }
}

static void postprocess(FILE *src, FILE *dst) {
    int ch;
    int newLine = 1;
    while((ch = fgetc(src)) != EOF){
        if(newLine && ch == '\n') continue;
        fputc(ch, dst);

        if (ch == '\n')
            newLine = 1;
        else
            newLine = 0;
    }
}

static int isType(char *s) {
    int n = sizeof(types) / sizeof(types[0]);

    for (int i = 0; i < n; i++) {
        if (strcmp(s, types[i]) == 0)
            return 1;
    }
    return 0;
}

static int findSizeOf(char *word) {
    
    if (strcmp(word, "int") == 0) return sizeof(int);
    if (strcmp(word, "char") == 0) return sizeof(char);
    if (strcmp(word, "void") == 0) return 0;
    if (strcmp(word, "short") == 0) return sizeof(short);
    if (strcmp(word, "long") == 0) return sizeof(long);
    if (strcmp(word, "float") == 0) return sizeof(float);
    if (strcmp(word, "double") == 0) return sizeof(double);
    if (strcmp(word, "signed") == 0) return sizeof(int);
    if (strcmp(word, "unsigned") == 0) return sizeof(unsigned int);
    if (strcmp(word, "_Bool") == 0) return sizeof(_Bool);

    return 0;
}

#endif
