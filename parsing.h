#ifndef PARSING_H
#define PARSING_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexAnalyszer.h"

// node definition

typedef enum {

    /* ===================== Start ===================== */
    PT_PROGRAM,

    /* ===================== External Declarations ===================== */
    PT_EXTERNAL_DECL_LIST,
    PT_EXTERNAL_DECL,

    /* ===================== Functions & Globals ===================== */
    PT_FUNCTION_OR_GLOBAL,
    PT_FUNCTION_OR_GLOBAL_REST,
    PT_FUNCTION_REST,
    PT_GLOBAL_DECL_REST,

    /* ===================== Types ===================== */
    PT_TYPE,

    /* ===================== Parameters ===================== */
    PT_PARAM_LIST,
    PT_PARAM_LIST_TAIL,
    PT_PARAM,

    /* ===================== Compound Statement ===================== */
    PT_COMPOUND_STMT,

    /* ===================== Declarations ===================== */
    PT_DECLARATION_LIST,
    PT_DECLARATION,
    PT_INIT_DECLARATOR_LIST,
    PT_INIT_DECLARATOR_LIST_TAIL,
    PT_INIT_DECLARATOR,

    /* ===================== Statements ===================== */
    PT_STATEMENT_LIST,
    PT_STATEMENT,

    /* ===================== Statement Forms ===================== */
    PT_EXPRESSION_STMT,
    PT_IF_STMT,
    PT_IF_ELSE_OPT,
    PT_WHILE_STMT,
    PT_RETURN_STMT,
    PT_RETURN_EXPR_OPT,

    /* ===================== Expressions ===================== */
    PT_EXPRESSION,
    PT_LOGICAL_OR_EXPR,
    PT_LOGICAL_OR_EXPR_TAIL,
    PT_LOGICAL_AND_EXPR,
    PT_LOGICAL_AND_EXPR_TAIL,
    PT_EQUALITY_EXPR,
    PT_EQUALITY_EXPR_TAIL,
    PT_RELATIONAL_EXPR,
    PT_RELATIONAL_EXPR_TAIL,
    PT_ADDITIVE_EXPR,
    PT_ADDITIVE_EXPR_TAIL,
    PT_MULTIPLICATIVE_EXPR,
    PT_MULTIPLICATIVE_EXPR_TAIL,
    PT_UNARY_EXPR,
    PT_PRIMARY_EXPR,

    /* ===================== Arguments ===================== */
    PT_ARG_LIST,
    PT_ARG_LIST_TAIL,

    /* ===================== Terminals ===================== */
    PT_IDENTIFIER,
    PT_INTEGER_LITERAL,
    PT_CHAR_LITERAL,

    PT_PLUS,
    PT_MINUS,
    PT_STAR,
    PT_AMPERSAND,
    PT_SLASH,
    PT_PERCENT,

    PT_ASSIGN,
    PT_EQ,
    PT_NEQ,
    PT_LT,
    PT_GT,
    PT_LE,
    PT_GE,

    PT_LOGICAL_AND,
    PT_LOGICAL_OR,
    PT_LOGICAL_NOT,

    PT_LPAREN,
    PT_RPAREN,
    PT_LBRACKET,
    PT_RBRACKET,
    PT_LBRACE,
    PT_RBRACE,
    PT_COMMA,
    PT_SEMICOLON,

    PT_IF,
    PT_ELSE,
    PT_WHILE,
    PT_RETURN,

    PT_INIT_LIST,
    PT_INIT_LIST_TAIL,

    PT_EPSILON,     // ε
    PT_ERROR

} nodeType;


typedef struct node {
    nodeType type;
    token *t;
    struct node *child;
    struct node *next;
    void *symbol;
} node;

static FILE *src;
static int row = 1, col = 1;
static token currToken;

static token *cloneToken(const token *t) {
    if (!t) return NULL;
    token *copy = malloc(sizeof(token));
    if (!copy) exit(1);
    *copy = *t;
    return copy;
}

static node* newNode(nodeType type, token *t) {
    node* n = malloc(sizeof(node));
    if (!n) exit(1);
    n->type = type;
    n->t = cloneToken(t);
    n->child = NULL;
    n->next = NULL;
    n->symbol = NULL;
    return n;
}

static node* returnErrorNode(){
    return newNode(PT_ERROR, &currToken);
}

static void addChild(node *parent, node *child) {
    if (!parent || !child) return;
    if (!parent->child) {
        parent->child = child;
    } else {
        node *c = parent->child;
        while (c->next) c = c->next;
        c->next = child;
    }
}

/* ---- parser prototypes ---- */
static node* parseProgram();
static node* parseExternalDeclList();
static node* parseExternalDecl();
static node* parseFunctionOrGlobal();
static node* parseFunctionOrGlobalRest();
static node* parseFunctionRest();
static node* parseGlobalDeclRest();
static node* parseTypes();
static node* parseParamList();
static node* parseParamListTail();
static node* parseParam();
static node* parseCompoundStmt();
static node* parseDeclarationList();
static node* parseDeclaration();
static node* parseInitDeclaratorList();
static node* parseInitDeclaratorListTail();
static node* parseInitDeclarator();
static node* parseInitList();
static node* parseInitListTail();
static node* parseStatementList();
static node* parseStatement();
static node* parseExpressionStmt();
static node* parseIfStmt();
static node* parseIfElseOpt();
static node* parseWhileStmt();
static node* parseReturnStmt();
static node* parseReturnExprOpt();
static node* parseExpression();
static node* parseLogicalOrExpr();
static node* parseLogicalOrExprTail();
static node* parseLogicalAndExpr();
static node* parseLogicalAndExprTail();
static node* parseEqualityExpr();
static node* parseEqualityExprTail();
static node* parseRelationalExpr();
static node* parseRelationalExprTail();
static node* parseAdditiveExpr();
static node* parseAdditiveExprTail();
static node* parseMultiplicativeExpr();
static node* parseMultiplicativeExprTail();
static node* parseUnaryExpr();
static node* parsePrimaryExpr();
static node* parseArgList();
static node* parseArgListTail();

static void advance() {
    currToken = getNextToken(src, &row, &col);
}

static void parserInit(FILE *input) {
    src = input;
    row = 1;
    col = 1;
    advance();
}

static token peekNextToken() {
    long pos = ftell(src);
    int savedRow = row;
    int savedCol = col;
    token saved = currToken;

    token next = getNextToken(src, &row, &col);

    fseek(src, pos, SEEK_SET);
    row = savedRow;
    col = savedCol;
    currToken = saved;

    return next;
}

static int isTokenName(const char *name) {
    return strcmp(currToken.tokenName, name) == 0;
}

static int isKeywordToken(const char *kw) {
    return isTokenName("keyword") && strcmp(currToken.tokenValue, kw) == 0;
}

static int isTypeToken() {
    if (!currToken.tokenName[0]) return 0;
    if (!(isTokenName("keyword") || isTokenName("id"))) return 0;
    return isType(currToken.tokenValue);
}

static int isSymbolToken(char c) {
    return isTokenName("symbol") && currToken.tokenValue[0] == c;
}

static int isIdentifierToken() {
    return isTokenName("id");
}

static int isIntegerLiteralToken() {
    return currToken.tokenName[0] && isdigit((unsigned char)currToken.tokenName[0]);
}

static int isStringLiteralToken() {
    return isTokenName("stringLit");
}

static int isCharLiteralToken() {
    if (currToken.tokenName[0] == '\'') return 1;
    return isSymbolToken('\'');
}

static int isAriOpChar(char c) {
    if (!isTokenName("ariOp")) return 0;
    if (currToken.tokenValue[0] == c) return 1;
    return 0;
}

static int isStartOfExpression() {
    if (isIdentifierToken()) return 1;
    if (isIntegerLiteralToken()) return 1;
    if (isStringLiteralToken()) return 1;
    if (isCharLiteralToken()) return 1;
    if (isSymbolToken('(')) return 1;
    if (isTokenName("ariOp")) return 1; // unary '-'
    if (isTokenName("logOp")) return 1; // unary '!'
    if (isTokenName("bitOp") && currToken.tokenValue[0] == '&') return 1; // unary '&'
    return 0;
}

static int isStartOfStatement() {
    if (isSymbolToken('{')) return 1;
    if (isKeywordToken("if")) return 1;
    if (isKeywordToken("while")) return 1;
    if (isKeywordToken("return")) return 1;
    if (isTypeToken()) return 1;
    if (isSymbolToken(';')) return 1;
    if (isStartOfExpression()) return 1;
    return 0;
}

static node* expectSymbolNode(char c, nodeType type) {
    if (!isSymbolToken(c)) return returnErrorNode();
    node* n = newNode(type, &currToken);
    advance();
    return n;
}

static node* expectKeywordNode(const char *kw, nodeType type) {
    if (!isKeywordToken(kw)) return returnErrorNode();
    node* n = newNode(type, &currToken);
    advance();
    return n;
}

static node* expectIdentifierNode() {
    if (!isIdentifierToken()) return returnErrorNode();
    node* n = newNode(PT_IDENTIFIER, &currToken);
    advance();
    return n;
}

static node* expectIntegerLiteralNode() {
    if (!isIntegerLiteralToken()) return returnErrorNode();
    node* n = newNode(PT_INTEGER_LITERAL, &currToken);
    advance();
    return n;
}

static node* expectCharLiteralNode() {
    if (!isCharLiteralToken()) return returnErrorNode();
    node* n = newNode(PT_CHAR_LITERAL, &currToken);
    advance();
    return n;
}

static node* expectAssignNode() {
    if (!isTokenName("assignOp")) return returnErrorNode();
    if (currToken.tokenValue[0] && strcmp(currToken.tokenValue, "=") != 0)
        return returnErrorNode();
    node* n = newNode(PT_ASSIGN, &currToken);
    advance();
    return n;
}

static node* expectRelOpNode(const char *lexeme, nodeType type) {
    if (!isTokenName("relOp")) return returnErrorNode();
    if (currToken.tokenValue[0] && strcmp(currToken.tokenValue, lexeme) != 0)
        return returnErrorNode();
    node* n = newNode(type, &currToken);
    advance();
    return n;
}

static node* expectAriOpNode(char op, nodeType type) {
    if (!isTokenName("ariOp")) return returnErrorNode();
    if (currToken.tokenValue[0] && currToken.tokenValue[0] != op)
        return returnErrorNode();
    node* n = newNode(type, &currToken);
    advance();
    return n;
}

static node* expectLogOpNode(const char *lexeme, nodeType type) {
    if (!isTokenName("logOp")) return returnErrorNode();
    if (currToken.tokenValue[0] && strcmp(currToken.tokenValue, lexeme) != 0)
        return returnErrorNode();
    node* n = newNode(type, &currToken);
    advance();
    return n;
}

static node* expectBitOpNode(char op, nodeType type) {
    if (!isTokenName("bitOp")) return returnErrorNode();
    if (currToken.tokenValue[0] && currToken.tokenValue[0] != op)
        return returnErrorNode();
    node* n = newNode(type, &currToken);
    advance();
    return n;
}

//Program -> ExternalDeclList EOF
static node* parseProgram() {
    node* program = newNode(PT_PROGRAM, NULL);
    node* declList = parseExternalDeclList();
    if (!declList || declList->type == PT_ERROR)
        return declList;

    addChild(program, declList);
    if (currToken.tokenName[0] != '\0')
        return returnErrorNode();

    return program;
}

//ExternalDeclList -> ExternalDecl ExternalDeclList | ε
static node* parseExternalDeclList() {
    node* list = newNode(PT_EXTERNAL_DECL_LIST, NULL);
    if (currToken.tokenName[0] == '\0') {
        addChild(list, newNode(PT_EPSILON, NULL));
        return list;
    }

    node* curr = parseExternalDecl();
    if (!curr || curr->type == PT_ERROR)
        return curr;

    addChild(list, curr);
    addChild(list, parseExternalDeclList());
    return list;
}

//ExternalDecl -> FunctionOrGlobal
static node* parseExternalDecl() {
    node* n = newNode(PT_EXTERNAL_DECL, NULL);
    node* fg = parseFunctionOrGlobal();
    if (!fg || fg->type == PT_ERROR)
        return fg;
    addChild(n, fg);
    return n;
}

//FunctionOrGlobal -> Type IDENTIFIER FunctionOrGlobalRest
static node* parseFunctionOrGlobal() {
    node* n = newNode(PT_FUNCTION_OR_GLOBAL, NULL);
    node* type = parseTypes();
    if (!type || type->type == PT_ERROR)
        return type;
    node* id = expectIdentifierNode();
    if (!id || id->type == PT_ERROR)
        return id;

    node* rest = parseFunctionOrGlobalRest();
    if (!rest || rest->type == PT_ERROR)
        return rest;

    addChild(n, type);
    addChild(n, id);
    addChild(n, rest);
    return n;
}

//FunctionOrGlobalRest -> '(' ParamList ')' FunctionRest | GlobalDeclRest
static node* parseFunctionOrGlobalRest() {
    node* n = newNode(PT_FUNCTION_OR_GLOBAL_REST, NULL);

    if (isSymbolToken('(')) {
        node* lp = expectSymbolNode('(', PT_LPAREN);
        node* params = parseParamList();
        node* rp = expectSymbolNode(')', PT_RPAREN);
        node* rest = parseFunctionRest();

        if (!lp || lp->type == PT_ERROR) return lp;
        if (!params || params->type == PT_ERROR) return params;
        if (!rp || rp->type == PT_ERROR) return rp;
        if (!rest || rest->type == PT_ERROR) return rest;

        addChild(n, lp);
        addChild(n, params);
        addChild(n, rp);
        addChild(n, rest);
        return n;
    }

    node* g = parseGlobalDeclRest();
    if (!g || g->type == PT_ERROR)
        return g;
    addChild(n, g);
    return n;
}

//FunctionRest -> ';' | CompoundStmt
static node* parseFunctionRest() {
    node* n = newNode(PT_FUNCTION_REST, NULL);

    if (isSymbolToken(';')) {
        node* semi = expectSymbolNode(';', PT_SEMICOLON);
        if (!semi || semi->type == PT_ERROR) return semi;
        addChild(n, semi);
        return n;
    }

    node* body = parseCompoundStmt();
    if (!body || body->type == PT_ERROR)
        return body;

    addChild(n, body);
    return n;
}

//GlobalDeclRest -> InitDeclaratorListTail ';'
static node* parseGlobalDeclRest() {
    node* n = newNode(PT_GLOBAL_DECL_REST, NULL);

    if (isSymbolToken('[')) {
        node* lb = expectSymbolNode('[', PT_LBRACKET);
        node* size = expectIntegerLiteralNode();
        node* rb = expectSymbolNode(']', PT_RBRACKET);
        if (!lb || lb->type == PT_ERROR) return lb;
        if (!size || size->type == PT_ERROR) return size;
        if (!rb || rb->type == PT_ERROR) return rb;
        addChild(n, lb);
        addChild(n, size);
        addChild(n, rb);
    }

    node* tail = parseInitDeclaratorListTail();
    if (!tail || tail->type == PT_ERROR)
        return tail;
    node* semi = expectSymbolNode(';', PT_SEMICOLON);
    if (!semi || semi->type == PT_ERROR)
        return semi;

    addChild(n, tail);
    addChild(n, semi);
    return n;
}

//Type -> any token recognized by isType() in the lexer
static node* parseTypes() {
    if (!isTypeToken())
        return returnErrorNode();
    node *type = newNode(PT_TYPE, &currToken);
    advance();
    return type;
}

//ParamList -> Param ParamListTail | ε
static node* parseParamList() {
    node* n = newNode(PT_PARAM_LIST, NULL);

    if (!isTypeToken()) {
        addChild(n, newNode(PT_EPSILON, NULL));
        return n;
    }

    node* param = parseParam();
    if (!param || param->type == PT_ERROR)
        return param;

    node* tail = parseParamListTail();
    if (!tail || tail->type == PT_ERROR)
        return tail;

    addChild(n, param);
    addChild(n, tail);
    return n;
}

//ParamListTail -> ',' Param ParamListTail | ε
static node* parseParamListTail() {
    node* n = newNode(PT_PARAM_LIST_TAIL, NULL);

    if (!isSymbolToken(',')) {
        addChild(n, newNode(PT_EPSILON, NULL));
        return n;
    }

    node* comma = expectSymbolNode(',', PT_COMMA);
    node* param = parseParam();
    node* tail = parseParamListTail();

    if (!comma || comma->type == PT_ERROR) return comma;
    if (!param || param->type == PT_ERROR) return param;
    if (!tail || tail->type == PT_ERROR) return tail;

    addChild(n, comma);
    addChild(n, param);
    addChild(n, tail);
    return n;
}

//Param -> Type IDENTIFIER
static node* parseParam() {
    node* n = newNode(PT_PARAM, NULL);
    node* type = parseTypes();
    if (!type || type->type == PT_ERROR)
        return type;

    if (isAriOpChar('*')) {
        node* star = expectAriOpNode('*', PT_STAR);
        if (!star || star->type == PT_ERROR)
            return star;
        addChild(n, type);
        addChild(n, star);
        node* idStar = expectIdentifierNode();
        if (!idStar || idStar->type == PT_ERROR)
            return idStar;
        addChild(n, idStar);
        return n;
    }

    node* id = expectIdentifierNode();
    if (!id || id->type == PT_ERROR)
        return id;

    addChild(n, type);
    addChild(n, id);

    if (isSymbolToken('[')) {
        node* lb = expectSymbolNode('[', PT_LBRACKET);
        node* size = expectIntegerLiteralNode();
        node* rb = expectSymbolNode(']', PT_RBRACKET);
        if (!lb || lb->type == PT_ERROR) return lb;
        if (!size || size->type == PT_ERROR) return size;
        if (!rb || rb->type == PT_ERROR) return rb;
        addChild(n, lb);
        addChild(n, size);
        addChild(n, rb);
    }

    return n;
}

//CompoundStmt -> '{' DeclarationList StatementList '}'
static node* parseCompoundStmt() {
    node* n = newNode(PT_COMPOUND_STMT, NULL);

    node* lbrace = expectSymbolNode('{', PT_LBRACE);
    if (!lbrace || lbrace->type == PT_ERROR)
        return lbrace;

    node* declarationList = parseDeclarationList();
    node* statementList = parseStatementList();

    node* rbrace = expectSymbolNode('}', PT_RBRACE);
    if (!rbrace || rbrace->type == PT_ERROR)
        return rbrace;

    addChild(n, lbrace);
    addChild(n, declarationList);
    addChild(n, statementList);
    addChild(n, rbrace);

    return n;
}

//DeclarationList -> Declaration DeclarationList | ε
static node* parseDeclarationList() {
    node* n = newNode(PT_DECLARATION_LIST, NULL);

    if (!isTypeToken()) {
        addChild(n, newNode(PT_EPSILON, NULL));
        return n;
    }

    node* declaration = parseDeclaration();
    if (!declaration || declaration->type == PT_ERROR)
        return declaration;

    node* rest = parseDeclarationList();
    if (!rest || rest->type == PT_ERROR)
        return rest;

    addChild(n, declaration);
    addChild(n, rest);
    return n;
}

//Declaration -> Type InitDeclaratorList ';'
static node* parseDeclaration() {
    node* n = newNode(PT_DECLARATION, NULL);

    node* type = parseTypes();
    if (!type || type->type == PT_ERROR)
        return type;

    node* declList = parseInitDeclaratorList();
    if (!declList || declList->type == PT_ERROR)
        return declList;

    node* semi = expectSymbolNode(';', PT_SEMICOLON);
    if (!semi || semi->type == PT_ERROR)
        return semi;

    addChild(n, type);
    addChild(n, declList);
    addChild(n, semi);
    return n;
}

//InitDeclaratorList -> InitDeclarator InitDeclaratorListTail
static node* parseInitDeclaratorList() {
    node* n = newNode(PT_INIT_DECLARATOR_LIST, NULL);
    node* first = parseInitDeclarator();
    if (!first || first->type == PT_ERROR)
        return first;

    node* tail = parseInitDeclaratorListTail();
    if (!tail || tail->type == PT_ERROR)
        return tail;

    addChild(n, first);
    addChild(n, tail);
    return n;
}

//InitDeclaratorListTail -> ',' InitDeclarator InitDeclaratorListTail | ε
static node* parseInitDeclaratorListTail() {
    node* n = newNode(PT_INIT_DECLARATOR_LIST_TAIL, NULL);

    if (!isSymbolToken(',')) {
        addChild(n, newNode(PT_EPSILON, NULL));
        return n;
    }

    node* comma = expectSymbolNode(',', PT_COMMA);
    node* decl = parseInitDeclarator();
    node* tail = parseInitDeclaratorListTail();

    if (!comma || comma->type == PT_ERROR) return comma;
    if (!decl || decl->type == PT_ERROR) return decl;
    if (!tail || tail->type == PT_ERROR) return tail;

    addChild(n, comma);
    addChild(n, decl);
    addChild(n, tail);
    return n;
}

//InitDeclarator -> IDENTIFIER | IDENTIFIER '=' Expression
static node* parseInitDeclarator() {
    node* n = newNode(PT_INIT_DECLARATOR, NULL);

    node* id = expectIdentifierNode();
    if (!id || id->type == PT_ERROR)
        return id;

    addChild(n, id);

    if (isSymbolToken('[')) {
        node* lb = expectSymbolNode('[', PT_LBRACKET);
        if (!lb || lb->type == PT_ERROR) return lb;
        addChild(n, lb);

        if (!isSymbolToken(']')) {
            node* size = expectIntegerLiteralNode();
            if (!size || size->type == PT_ERROR) return size;
            addChild(n, size);
        }

        node* rb = expectSymbolNode(']', PT_RBRACKET);
        if (!rb || rb->type == PT_ERROR) return rb;
        addChild(n, rb);
    }

    if (isTokenName("assignOp")) {
        node* assign = expectAssignNode();
        if (!assign || assign->type == PT_ERROR)
            return assign;
        addChild(n, assign);

        if (isSymbolToken('{')) {
            node* lb = expectSymbolNode('{', PT_LBRACE);
            if (!lb || lb->type == PT_ERROR) return lb;
            node* list = parseInitList();
            if (!list || list->type == PT_ERROR) return list;
            node* rb = expectSymbolNode('}', PT_RBRACE);
            if (!rb || rb->type == PT_ERROR) return rb;
            addChild(n, lb);
            addChild(n, list);
            addChild(n, rb);
        } else {
            node* expr = parseExpression();
            if (!expr || expr->type == PT_ERROR)
                return expr;
            addChild(n, expr);
        }
    }

    return n;
}

// InitList → Expression InitListTail
static node* parseInitList() {
    node* n = newNode(PT_INIT_LIST, NULL);

    if (!isStartOfExpression())
        return returnErrorNode();

    node* expr = parseExpression();
    if (!expr || expr->type == PT_ERROR)
        return expr;

    node* tail = parseInitListTail();
    if (!tail || tail->type == PT_ERROR)
        return tail;

    addChild(n, expr);
    addChild(n, tail);
    return n;
}

// InitListTail → ',' Expression InitListTail | ε
static node* parseInitListTail() {
    node* n = newNode(PT_INIT_LIST_TAIL, NULL);

    if (!isSymbolToken(',')) {
        addChild(n, newNode(PT_EPSILON, NULL));
        return n;
    }

    node* comma = expectSymbolNode(',', PT_COMMA);
    node* expr = parseExpression();
    node* tail = parseInitListTail();

    if (!comma || comma->type == PT_ERROR) return comma;
    if (!expr || expr->type == PT_ERROR) return expr;
    if (!tail || tail->type == PT_ERROR) return tail;

    addChild(n, comma);
    addChild(n, expr);
    addChild(n, tail);
    return n;
}

//StatementList -> Statement StatementList | ε
static node* parseStatementList() {
    node* n = newNode(PT_STATEMENT_LIST, NULL);

    if (!isStartOfStatement()) {
        addChild(n, newNode(PT_EPSILON, NULL));
        return n;
    }

    node* statement = parseStatement();
    if (!statement || statement->type == PT_ERROR)
        return statement;

    node* rest = parseStatementList();
    if (!rest || rest->type == PT_ERROR)
        return rest;

    addChild(n, statement);
    addChild(n, rest);
    return n;
}

//Statement -> ExpressionStmt | CompoundStmt | IfStmt | WhileStmt | ReturnStmt
static node* parseStatement() {
    node* n = newNode(PT_STATEMENT, NULL);

    if (isSymbolToken('{')) {
        node* c = parseCompoundStmt();
        if (!c || c->type == PT_ERROR) return c;
        addChild(n, c);
        return n;
    }
    if (isKeywordToken("if")) {
        node* s = parseIfStmt();
        if (!s || s->type == PT_ERROR) return s;
        addChild(n, s);
        return n;
    }
    if (isKeywordToken("while")) {
        node* s = parseWhileStmt();
        if (!s || s->type == PT_ERROR) return s;
        addChild(n, s);
        return n;
    }
    if (isKeywordToken("return")) {
        node* s = parseReturnStmt();
        if (!s || s->type == PT_ERROR) return s;
        addChild(n, s);
        return n;
    }

    if (isTypeToken()) {
        node* decl = parseDeclaration();
        if (!decl || decl->type == PT_ERROR) return decl;
        addChild(n, decl);
        return n;
    }

    node* exprStmt = parseExpressionStmt();
    if (!exprStmt || exprStmt->type == PT_ERROR) return exprStmt;
    addChild(n, exprStmt);
    return n;
}

//ExpressionStmt -> Expression ';' | ';'
static node* parseExpressionStmt() {
    node* n = newNode(PT_EXPRESSION_STMT, NULL);

    if (isSymbolToken(';')) {
        node* semi = expectSymbolNode(';', PT_SEMICOLON);
        if (!semi || semi->type == PT_ERROR) return semi;
        addChild(n, semi);
        return n;
    }

    node* expr = parseExpression();
    if (!expr || expr->type == PT_ERROR)
        return expr;

    node* semi = expectSymbolNode(';', PT_SEMICOLON);
    if (!semi || semi->type == PT_ERROR)
        return semi;

    addChild(n, expr);
    addChild(n, semi);
    return n;
}

// IfStmt -> 'if' '(' Expression ')' Statement IfElseOpt
static node* parseIfStmt() {
    node* n = newNode(PT_IF_STMT, NULL);

    node* kw = expectKeywordNode("if", PT_IF);
    if (!kw || kw->type == PT_ERROR) return kw;

    node* lp = expectSymbolNode('(', PT_LPAREN);
    if (!lp || lp->type == PT_ERROR) return lp;

    node* expr = parseExpression();
    if (!expr || expr->type == PT_ERROR)
        return expr;

    node* rp = expectSymbolNode(')', PT_RPAREN);
    if (!rp || rp->type == PT_ERROR) return rp;

    node* stmt = parseStatement();
    if (!stmt || stmt->type == PT_ERROR)
        return stmt;

    node* opt = parseIfElseOpt();
    if (!opt || opt->type == PT_ERROR)
        return opt;

    addChild(n, kw);
    addChild(n, lp);
    addChild(n, expr);
    addChild(n, rp);
    addChild(n, stmt);
    addChild(n, opt);
    return n;
}

// IfElseOpt -> 'else' Statement | ε
static node* parseIfElseOpt() {
    node* n = newNode(PT_IF_ELSE_OPT, NULL);

    if (!isKeywordToken("else")) {
        addChild(n, newNode(PT_EPSILON, NULL));
        return n;
    }

    node* kw = expectKeywordNode("else", PT_ELSE);
    node* stmt = parseStatement();
    if (!kw || kw->type == PT_ERROR) return kw;
    if (!stmt || stmt->type == PT_ERROR) return stmt;

    addChild(n, kw);
    addChild(n, stmt);
    return n;
}

// WhileStmt -> 'while' '(' Expression ')' Statement
static node* parseWhileStmt() {
    node* n = newNode(PT_WHILE_STMT, NULL);

    node* kw = expectKeywordNode("while", PT_WHILE);
    if (!kw || kw->type == PT_ERROR) return kw;

    node* lp = expectSymbolNode('(', PT_LPAREN);
    if (!lp || lp->type == PT_ERROR) return lp;

    node* expr = parseExpression();
    if (!expr || expr->type == PT_ERROR)
        return expr;

    node* rp = expectSymbolNode(')', PT_RPAREN);
    if (!rp || rp->type == PT_ERROR) return rp;

    node* stmt = parseStatement();
    if (!stmt || stmt->type == PT_ERROR) return stmt;

    addChild(n, kw);
    addChild(n, lp);
    addChild(n, expr);
    addChild(n, rp);
    addChild(n, stmt);
    return n;
}

// ReturnStmt -> 'return' ReturnExprOpt ';'
static node* parseReturnStmt() {
    node* n = newNode(PT_RETURN_STMT, NULL);

    node* kw = expectKeywordNode("return", PT_RETURN);
    if (!kw || kw->type == PT_ERROR) return kw;

    node* opt = parseReturnExprOpt();
    if (!opt || opt->type == PT_ERROR) return opt;

    node* semi = expectSymbolNode(';', PT_SEMICOLON);
    if (!semi || semi->type == PT_ERROR) return semi;

    addChild(n, kw);
    addChild(n, opt);
    addChild(n, semi);
    return n;
}

// ReturnExprOpt -> Expression | ε
static node* parseReturnExprOpt() {
    node* n = newNode(PT_RETURN_EXPR_OPT, NULL);

    if (!isStartOfExpression()) {
        addChild(n, newNode(PT_EPSILON, NULL));
        return n;
    }

    node* expr = parseExpression();
    if (!expr || expr->type == PT_ERROR)
        return expr;

    addChild(n, expr);
    return n;
}

// Expression -> IDENTIFIER '=' Expression | LogicalOrExpr
static node* parseExpression() {
    node* n = newNode(PT_EXPRESSION, NULL);

    if (isIdentifierToken()) {
        token next = peekNextToken();
        if (strcmp(next.tokenName, "assignOp") == 0 &&
            (next.tokenValue[0] == '\0' || strcmp(next.tokenValue, "=") == 0)) {
            node* id = expectIdentifierNode();
            node* assign = expectAssignNode();
            node* expr = parseExpression();
            if (!id || id->type == PT_ERROR) return id;
            if (!assign || assign->type == PT_ERROR) return assign;
            if (!expr || expr->type == PT_ERROR) return expr;
            addChild(n, id);
            addChild(n, assign);
            addChild(n, expr);
            return n;
        }
    }

    node* lor = parseLogicalOrExpr();
    if (!lor || lor->type == PT_ERROR)
        return lor;

    addChild(n, lor);
    return n;
}

// LogicalOrExpr -> LogicalAndExpr LogicalOrExpr'
static node* parseLogicalOrExpr() {
    node* n = newNode(PT_LOGICAL_OR_EXPR, NULL);

    node* andExpr = parseLogicalAndExpr();
    if (!andExpr || andExpr->type == PT_ERROR)
        return andExpr;

    node* tail = parseLogicalOrExprTail();
    if (!tail || tail->type == PT_ERROR)
        return tail;

    addChild(n, andExpr);
    addChild(n, tail);
    return n;
}

// LogicalOrExpr' -> '||' LogicalAndExpr LogicalOrExpr' | ε
static node* parseLogicalOrExprTail() {
    node* n = newNode(PT_LOGICAL_OR_EXPR_TAIL, NULL);

    if (!isTokenName("logOp")) {
        addChild(n, newNode(PT_EPSILON, NULL));
        return n;
    }

    node* op = expectLogOpNode("||", PT_LOGICAL_OR);
    if (!op || op->type == PT_ERROR)
        return op;

    node* andExpr = parseLogicalAndExpr();
    if (!andExpr || andExpr->type == PT_ERROR)
        return andExpr;

    node* tail = parseLogicalOrExprTail();
    if (!tail || tail->type == PT_ERROR)
        return tail;

    addChild(n, op);
    addChild(n, andExpr);
    addChild(n, tail);
    return n;
}

// LogicalAndExpr -> EqualityExpr LogicalAndExpr'
static node* parseLogicalAndExpr() {
    node* n = newNode(PT_LOGICAL_AND_EXPR, NULL);

    node* eq = parseEqualityExpr();
    if (!eq || eq->type == PT_ERROR)
        return eq;

    node* tail = parseLogicalAndExprTail();
    if (!tail || tail->type == PT_ERROR)
        return tail;

    addChild(n, eq);
    addChild(n, tail);
    return n;
}

// LogicalAndExpr' -> '&&' EqualityExpr LogicalAndExpr' | ε
static node* parseLogicalAndExprTail() {
    node* n = newNode(PT_LOGICAL_AND_EXPR_TAIL, NULL);

    if (!isTokenName("logOp")) {
        addChild(n, newNode(PT_EPSILON, NULL));
        return n;
    }

    node* op = expectLogOpNode("&&", PT_LOGICAL_AND);
    if (!op || op->type == PT_ERROR)
        return op;

    node* eq = parseEqualityExpr();
    if (!eq || eq->type == PT_ERROR)
        return eq;

    node* tail = parseLogicalAndExprTail();
    if (!tail || tail->type == PT_ERROR)
        return tail;

    addChild(n, op);
    addChild(n, eq);
    addChild(n, tail);
    return n;
}

// EqualityExpr -> RelationalExpr EqualityExpr'
static node* parseEqualityExpr() {
    node* n = newNode(PT_EQUALITY_EXPR, NULL);

    node* rel = parseRelationalExpr();
    if (!rel || rel->type == PT_ERROR)
        return rel;

    node* tail = parseEqualityExprTail();
    if (!tail || tail->type == PT_ERROR)
        return tail;

    addChild(n, rel);
    addChild(n, tail);
    return n;
}

// EqualityExpr' -> '==' RelationalExpr EqualityExpr' | '!=' RelationalExpr EqualityExpr' | ε
static node* parseEqualityExprTail() {
    node* n = newNode(PT_EQUALITY_EXPR_TAIL, NULL);

    if (!isTokenName("relOp")) {
        addChild(n, newNode(PT_EPSILON, NULL));
        return n;
    }

    if (currToken.tokenValue[0] && strcmp(currToken.tokenValue, "==") == 0) {
        node* op = expectRelOpNode("==", PT_EQ);
        node* rel = parseRelationalExpr();
        node* tail = parseEqualityExprTail();
        if (!op || op->type == PT_ERROR) return op;
        if (!rel || rel->type == PT_ERROR) return rel;
        if (!tail || tail->type == PT_ERROR) return tail;
        addChild(n, op);
        addChild(n, rel);
        addChild(n, tail);
        return n;
    }

    if (currToken.tokenValue[0] && strcmp(currToken.tokenValue, "!=") == 0) {
        node* op = expectRelOpNode("!=", PT_NEQ);
        node* rel = parseRelationalExpr();
        node* tail = parseEqualityExprTail();
        if (!op || op->type == PT_ERROR) return op;
        if (!rel || rel->type == PT_ERROR) return rel;
        if (!tail || tail->type == PT_ERROR) return tail;
        addChild(n, op);
        addChild(n, rel);
        addChild(n, tail);
        return n;
    }

    if (!currToken.tokenValue[0]) {
        node* op = expectRelOpNode("==", PT_EQ);
        if (op->type != PT_ERROR) {
            node* rel = parseRelationalExpr();
            node* tail = parseEqualityExprTail();
            if (!rel || rel->type == PT_ERROR) return rel;
            if (!tail || tail->type == PT_ERROR) return tail;
            addChild(n, op);
            addChild(n, rel);
            addChild(n, tail);
            return n;
        }
    }

    addChild(n, newNode(PT_EPSILON, NULL));
    return n;
}

// RelationalExpr -> AdditiveExpr RelationalExpr'
static node* parseRelationalExpr() {
    node* n = newNode(PT_RELATIONAL_EXPR, NULL);

    node* add = parseAdditiveExpr();
    if (!add || add->type == PT_ERROR)
        return add;

    node* tail = parseRelationalExprTail();
    if (!tail || tail->type == PT_ERROR)
        return tail;

    addChild(n, add);
    addChild(n, tail);
    return n;
}

// RelationalExpr' -> '<' AdditiveExpr RelationalExpr' | '>' AdditiveExpr RelationalExpr'
//                | '<=' AdditiveExpr RelationalExpr' | '>=' AdditiveExpr RelationalExpr' | ε
static node* parseRelationalExprTail() {
    node* n = newNode(PT_RELATIONAL_EXPR_TAIL, NULL);

    if (!isTokenName("relOp")) {
        addChild(n, newNode(PT_EPSILON, NULL));
        return n;
    }

    if (currToken.tokenValue[0]) {
        node* op = NULL;
        if (strcmp(currToken.tokenValue, "<") == 0) op = expectRelOpNode("<", PT_LT);
        else if (strcmp(currToken.tokenValue, ">") == 0) op = expectRelOpNode(">", PT_GT);
        else if (strcmp(currToken.tokenValue, "<=") == 0) op = expectRelOpNode("<=", PT_LE);
        else if (strcmp(currToken.tokenValue, ">=") == 0) op = expectRelOpNode(">=", PT_GE);
        else return returnErrorNode();

        if (!op || op->type == PT_ERROR) return op;
        node* add = parseAdditiveExpr();
        node* tail = parseRelationalExprTail();
        if (!add || add->type == PT_ERROR) return add;
        if (!tail || tail->type == PT_ERROR) return tail;
        addChild(n, op);
        addChild(n, add);
        addChild(n, tail);
        return n;
    }

    node* op = expectRelOpNode("<", PT_LT);
    if (!op || op->type == PT_ERROR) return op;
    node* add = parseAdditiveExpr();
    node* tail = parseRelationalExprTail();
    if (!add || add->type == PT_ERROR) return add;
    if (!tail || tail->type == PT_ERROR) return tail;
    addChild(n, op);
    addChild(n, add);
    addChild(n, tail);
    return n;
}

// AdditiveExpr -> MultiplicativeExpr AdditiveExpr'
static node* parseAdditiveExpr() {
    node* n = newNode(PT_ADDITIVE_EXPR, NULL);

    node* mul = parseMultiplicativeExpr();
    if (!mul || mul->type == PT_ERROR)
        return mul;

    node* tail = parseAdditiveExprTail();
    if (!tail || tail->type == PT_ERROR)
        return tail;

    addChild(n, mul);
    addChild(n, tail);
    return n;
}

// AdditiveExpr' -> '+' MultiplicativeExpr AdditiveExpr' | '-' MultiplicativeExpr AdditiveExpr' | ε
static node* parseAdditiveExprTail() {
    node* n = newNode(PT_ADDITIVE_EXPR_TAIL, NULL);

    if (!isTokenName("ariOp")) {
        addChild(n, newNode(PT_EPSILON, NULL));
        return n;
    }

    if (currToken.tokenValue[0] && currToken.tokenValue[0] != '+' && currToken.tokenValue[0] != '-') {
        addChild(n, newNode(PT_EPSILON, NULL));
        return n;
    }

    node* op = NULL;
    if (currToken.tokenValue[0] == '-') op = expectAriOpNode('-', PT_MINUS);
    else op = expectAriOpNode('+', PT_PLUS);

    if (!op || op->type == PT_ERROR)
        return op;

    node* mul = parseMultiplicativeExpr();
    if (!mul || mul->type == PT_ERROR)
        return mul;

    node* tail = parseAdditiveExprTail();
    if (!tail || tail->type == PT_ERROR)
        return tail;

    addChild(n, op);
    addChild(n, mul);
    addChild(n, tail);
    return n;
}

// MultiplicativeExpr -> UnaryExpr MultiplicativeExpr'
static node* parseMultiplicativeExpr() {
    node* n = newNode(PT_MULTIPLICATIVE_EXPR, NULL);

    node* unary = parseUnaryExpr();
    if (!unary || unary->type == PT_ERROR)
        return unary;

    node* tail = parseMultiplicativeExprTail();
    if (!tail || tail->type == PT_ERROR)
        return tail;

    addChild(n, unary);
    addChild(n, tail);
    return n;
}

// MultiplicativeExpr' -> '*' UnaryExpr MultiplicativeExpr' | '/' UnaryExpr MultiplicativeExpr' | '%' UnaryExpr MultiplicativeExpr' | ε
static node* parseMultiplicativeExprTail() {
    node* n = newNode(PT_MULTIPLICATIVE_EXPR_TAIL, NULL);

    if (!isTokenName("ariOp")) {
        addChild(n, newNode(PT_EPSILON, NULL));
        return n;
    }

    if (currToken.tokenValue[0] &&
        currToken.tokenValue[0] != '*' &&
        currToken.tokenValue[0] != '/' &&
        currToken.tokenValue[0] != '%') {
        addChild(n, newNode(PT_EPSILON, NULL));
        return n;
    }

    node* op = NULL;
    if (currToken.tokenValue[0] == '/') op = expectAriOpNode('/', PT_SLASH);
    else if (currToken.tokenValue[0] == '%') op = expectAriOpNode('%', PT_PERCENT);
    else op = expectAriOpNode('*', PT_STAR);

    if (!op || op->type == PT_ERROR)
        return op;

    node* unary = parseUnaryExpr();
    if (!unary || unary->type == PT_ERROR)
        return unary;

    node* tail = parseMultiplicativeExprTail();
    if (!tail || tail->type == PT_ERROR)
        return tail;

    addChild(n, op);
    addChild(n, unary);
    addChild(n, tail);
    return n;
}

// UnaryExpr -> '-' UnaryExpr | '!' UnaryExpr | '&' UnaryExpr | PrimaryExpr
static node* parseUnaryExpr() {
    node* n = newNode(PT_UNARY_EXPR, NULL);

    if (isTokenName("ariOp")) {
        node* op = expectAriOpNode('-', PT_MINUS);
        if (!op || op->type == PT_ERROR) return op;
        node* unary = parseUnaryExpr();
        if (!unary || unary->type == PT_ERROR) return unary;
        addChild(n, op);
        addChild(n, unary);
        return n;
    }

    if (isTokenName("logOp")) {
        node* op = expectLogOpNode("!", PT_LOGICAL_NOT);
        if (!op || op->type == PT_ERROR) return op;
        node* unary = parseUnaryExpr();
        if (!unary || unary->type == PT_ERROR) return unary;
        addChild(n, op);
        addChild(n, unary);
        return n;
    }

    if (isTokenName("bitOp") && currToken.tokenValue[0] == '&') {
        node* op = expectBitOpNode('&', PT_AMPERSAND);
        if (!op || op->type == PT_ERROR) return op;
        node* unary = parseUnaryExpr();
        if (!unary || unary->type == PT_ERROR) return unary;
        addChild(n, op);
        addChild(n, unary);
        return n;
    }

    node* primary = parsePrimaryExpr();
    if (!primary || primary->type == PT_ERROR)
        return primary;

    addChild(n, primary);
    return n;
}

// PrimaryExpr -> IDENTIFIER | IDENTIFIER '(' ArgList ')' | INTEGER_LITERAL | CHAR_LITERAL | '(' Expression ')'
static node* parsePrimaryExpr() {
    node* n = newNode(PT_PRIMARY_EXPR, NULL);

    if (isIdentifierToken()) {
        token next = peekNextToken();
        node* id = expectIdentifierNode();
        if (!id || id->type == PT_ERROR) return id;

        if (strcmp(next.tokenName, "symbol") == 0 && next.tokenValue[0] == '(') {
            node* lp = expectSymbolNode('(', PT_LPAREN);
            node* args = parseArgList();
            node* rp = expectSymbolNode(')', PT_RPAREN);
            if (!lp || lp->type == PT_ERROR) return lp;
            if (!args || args->type == PT_ERROR) return args;
            if (!rp || rp->type == PT_ERROR) return rp;
            addChild(n, id);
            addChild(n, lp);
            addChild(n, args);
            addChild(n, rp);
            return n;
        }

        addChild(n, id);
        return n;
    }

    if (isIntegerLiteralToken()) {
        node* lit = expectIntegerLiteralNode();
        if (!lit || lit->type == PT_ERROR) return lit;
        addChild(n, lit);
        return n;
    }

    if (isStringLiteralToken()) {
        node* lit = newNode(PT_CHAR_LITERAL, &currToken);
        advance();
        addChild(n, lit);
        return n;
    }

    if (isCharLiteralToken()) {
        if (isSymbolToken('\'')) {
            token mid;
            node* lit = NULL;
            advance();
            if (!isIdentifierToken() && !isIntegerLiteralToken())
                return returnErrorNode();
            mid = currToken;
            advance();
            if (!isSymbolToken('\''))
                return returnErrorNode();
            advance();
            lit = newNode(PT_CHAR_LITERAL, &mid);
            addChild(n, lit);
            return n;
        } else {
            node* lit = expectCharLiteralNode();
            if (!lit || lit->type == PT_ERROR) return lit;
            addChild(n, lit);
            return n;
        }
    }

    if (isSymbolToken('(')) {
        node* lp = expectSymbolNode('(', PT_LPAREN);
        node* expr = parseExpression();
        node* rp = expectSymbolNode(')', PT_RPAREN);
        if (!lp || lp->type == PT_ERROR) return lp;
        if (!expr || expr->type == PT_ERROR) return expr;
        if (!rp || rp->type == PT_ERROR) return rp;
        addChild(n, lp);
        addChild(n, expr);
        addChild(n, rp);
        return n;
    }

    return returnErrorNode();
}

// ArgList -> Expression ArgListTail | ε
static node* parseArgList() {
    node* n = newNode(PT_ARG_LIST, NULL);

    if (!isStartOfExpression()) {
        addChild(n, newNode(PT_EPSILON, NULL));
        return n;
    }

    node* expr = parseExpression();
    if (!expr || expr->type == PT_ERROR)
        return expr;

    node* tail = parseArgListTail();
    if (!tail || tail->type == PT_ERROR)
        return tail;

    addChild(n, expr);
    addChild(n, tail);
    return n;
}

// ArgListTail -> ',' Expression ArgListTail | ε
static node* parseArgListTail() {
    node* n = newNode(PT_ARG_LIST_TAIL, NULL);

    if (!isSymbolToken(',')) {
        addChild(n, newNode(PT_EPSILON, NULL));
        return n;
    }

    node* comma = expectSymbolNode(',', PT_COMMA);
    node* expr = parseExpression();
    node* tail = parseArgListTail();

    if (!comma || comma->type == PT_ERROR) return comma;
    if (!expr || expr->type == PT_ERROR) return expr;
    if (!tail || tail->type == PT_ERROR) return tail;

    addChild(n, comma);
    addChild(n, expr);
    addChild(n, tail);
    return n;
}

static const char *nodeTypeName(nodeType type) {
    switch (type) {
        case PT_PROGRAM: return "PT_PROGRAM";
        case PT_EXTERNAL_DECL_LIST: return "PT_EXTERNAL_DECL_LIST";
        case PT_EXTERNAL_DECL: return "PT_EXTERNAL_DECL";
        case PT_FUNCTION_OR_GLOBAL: return "PT_FUNCTION_OR_GLOBAL";
        case PT_FUNCTION_OR_GLOBAL_REST: return "PT_FUNCTION_OR_GLOBAL_REST";
        case PT_FUNCTION_REST: return "PT_FUNCTION_REST";
        case PT_GLOBAL_DECL_REST: return "PT_GLOBAL_DECL_REST";
        case PT_TYPE: return "PT_TYPE";
        case PT_PARAM_LIST: return "PT_PARAM_LIST";
        case PT_PARAM_LIST_TAIL: return "PT_PARAM_LIST_TAIL";
        case PT_PARAM: return "PT_PARAM";
        case PT_COMPOUND_STMT: return "PT_COMPOUND_STMT";
        case PT_DECLARATION_LIST: return "PT_DECLARATION_LIST";
        case PT_DECLARATION: return "PT_DECLARATION";
        case PT_INIT_DECLARATOR_LIST: return "PT_INIT_DECLARATOR_LIST";
        case PT_INIT_DECLARATOR_LIST_TAIL: return "PT_INIT_DECLARATOR_LIST_TAIL";
        case PT_INIT_DECLARATOR: return "PT_INIT_DECLARATOR";
        case PT_STATEMENT_LIST: return "PT_STATEMENT_LIST";
        case PT_STATEMENT: return "PT_STATEMENT";
        case PT_EXPRESSION_STMT: return "PT_EXPRESSION_STMT";
        case PT_IF_STMT: return "PT_IF_STMT";
        case PT_IF_ELSE_OPT: return "PT_IF_ELSE_OPT";
        case PT_WHILE_STMT: return "PT_WHILE_STMT";
        case PT_RETURN_STMT: return "PT_RETURN_STMT";
        case PT_RETURN_EXPR_OPT: return "PT_RETURN_EXPR_OPT";
        case PT_EXPRESSION: return "PT_EXPRESSION";
        case PT_LOGICAL_OR_EXPR: return "PT_LOGICAL_OR_EXPR";
        case PT_LOGICAL_OR_EXPR_TAIL: return "PT_LOGICAL_OR_EXPR_TAIL";
        case PT_LOGICAL_AND_EXPR: return "PT_LOGICAL_AND_EXPR";
        case PT_LOGICAL_AND_EXPR_TAIL: return "PT_LOGICAL_AND_EXPR_TAIL";
        case PT_EQUALITY_EXPR: return "PT_EQUALITY_EXPR";
        case PT_EQUALITY_EXPR_TAIL: return "PT_EQUALITY_EXPR_TAIL";
        case PT_RELATIONAL_EXPR: return "PT_RELATIONAL_EXPR";
        case PT_RELATIONAL_EXPR_TAIL: return "PT_RELATIONAL_EXPR_TAIL";
        case PT_ADDITIVE_EXPR: return "PT_ADDITIVE_EXPR";
        case PT_ADDITIVE_EXPR_TAIL: return "PT_ADDITIVE_EXPR_TAIL";
        case PT_MULTIPLICATIVE_EXPR: return "PT_MULTIPLICATIVE_EXPR";
        case PT_MULTIPLICATIVE_EXPR_TAIL: return "PT_MULTIPLICATIVE_EXPR_TAIL";
        case PT_UNARY_EXPR: return "PT_UNARY_EXPR";
        case PT_PRIMARY_EXPR: return "PT_PRIMARY_EXPR";
        case PT_ARG_LIST: return "PT_ARG_LIST";
        case PT_ARG_LIST_TAIL: return "PT_ARG_LIST_TAIL";
        case PT_IDENTIFIER: return "PT_IDENTIFIER";
        case PT_INTEGER_LITERAL: return "PT_INTEGER_LITERAL";
        case PT_CHAR_LITERAL: return "PT_CHAR_LITERAL";
        case PT_PLUS: return "PT_PLUS";
        case PT_MINUS: return "PT_MINUS";
        case PT_STAR: return "PT_STAR";
        case PT_AMPERSAND: return "PT_AMPERSAND";
        case PT_SLASH: return "PT_SLASH";
        case PT_PERCENT: return "PT_PERCENT";
        case PT_ASSIGN: return "PT_ASSIGN";
        case PT_EQ: return "PT_EQ";
        case PT_NEQ: return "PT_NEQ";
        case PT_LT: return "PT_LT";
        case PT_GT: return "PT_GT";
        case PT_LE: return "PT_LE";
        case PT_GE: return "PT_GE";
        case PT_LOGICAL_AND: return "PT_LOGICAL_AND";
        case PT_LOGICAL_OR: return "PT_LOGICAL_OR";
        case PT_LOGICAL_NOT: return "PT_LOGICAL_NOT";
        case PT_LPAREN: return "PT_LPAREN";
        case PT_RPAREN: return "PT_RPAREN";
        case PT_LBRACKET: return "PT_LBRACKET";
        case PT_RBRACKET: return "PT_RBRACKET";
        case PT_LBRACE: return "PT_LBRACE";
        case PT_RBRACE: return "PT_RBRACE";
        case PT_COMMA: return "PT_COMMA";
        case PT_SEMICOLON: return "PT_SEMICOLON";
        case PT_IF: return "PT_IF";
        case PT_ELSE: return "PT_ELSE";
        case PT_WHILE: return "PT_WHILE";
        case PT_RETURN: return "PT_RETURN";
        case PT_INIT_LIST: return "PT_INIT_LIST";
        case PT_INIT_LIST_TAIL: return "PT_INIT_LIST_TAIL";
        case PT_EPSILON: return "PT_EPSILON";
        case PT_ERROR: return "PT_ERROR";
        default: return "PT_UNKNOWN";
    }
}

static void printParseTree(node *n, FILE *out, int depth) {
    if (!n) return;

    for (int i = 0; i < depth; i++) {
        fputs("  ", out);
    }

    fprintf(out, "%s", nodeTypeName(n->type));

    if (n->t && n->t->tokenName[0]) {
        const char *name = n->t->tokenName;
        const char *value = n->t->tokenValue;
        if (value[0])
            fprintf(out, " [tokenName=%s tokenValue=%s]", name, value);
        else
            fprintf(out, " [tokenName=%s]", name);
    }

    fputc('\n', out);

    printParseTree(n->child, out, depth + 1);
    printParseTree(n->next, out, depth);
}

#endif
