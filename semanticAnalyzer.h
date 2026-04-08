#ifndef SEMANTICANALYZER_H
#define SEMANTICANALYZER_H

#include "parsing.h"
#include <stdarg.h>

typedef enum {
    SEM_TYPE_UNKNOWN = 0,
    SEM_TYPE_INT,
    SEM_TYPE_CHAR,
    SEM_TYPE_VOID
} SemType;

typedef struct SemSymbol {
    char name[50];
    SemType type;
    int isFunction;
    int isDefined;
    int paramCount;
    SemType params[32];
    struct SemSymbol *next;
} SemSymbol;

typedef struct SemScope {
    struct SemScope *parent;
    SemSymbol *symbols;
} SemScope;

typedef struct {
    SemScope *globalScope;
    SemScope *currentScope;
    FILE *errOut;
    int errorCount;
    SemType currentFunctionReturn;
    int inFunction;
} SemanticContext;

static int semNodeIs(const node *n, nodeType t) {
    return n && n->type == t;
}

static node *semChildAt(node *n, int idx) {
    node *c = n ? n->child : NULL;
    for (int i = 0; c && i < idx; i++) c = c->next;
    return c;
}

static SemType semTypeFromTokenText(const char *s) {
    if (!s || !s[0]) return SEM_TYPE_UNKNOWN;
    if (strcmp(s, "char") == 0) return SEM_TYPE_CHAR;
    if (strcmp(s, "void") == 0) return SEM_TYPE_VOID;
    return SEM_TYPE_INT;
}

static SemType semTypeFromTypeNode(node *typeNode) {
    if (!typeNode || !typeNode->t) return SEM_TYPE_UNKNOWN;
    return semTypeFromTokenText(typeNode->t->tokenValue);
}

static const char *semTypeName(SemType t) {
    switch (t) {
        case SEM_TYPE_INT: return "int";
        case SEM_TYPE_CHAR: return "char";
        case SEM_TYPE_VOID: return "void";
        default: return "unknown";
    }
}

static int semTypeCompatible(SemType dst, SemType src) {
    if (dst == SEM_TYPE_UNKNOWN || src == SEM_TYPE_UNKNOWN) return 1;
    if (dst == src) return 1;
    if ((dst == SEM_TYPE_INT || dst == SEM_TYPE_CHAR) &&
        (src == SEM_TYPE_INT || src == SEM_TYPE_CHAR)) return 1;
    return 0;
}

static void semReport(SemanticContext *ctx, node *at, const char *fmt, ...) {
    va_list ap;
    int row = 0;
    int col = 0;
    FILE *out = (ctx && ctx->errOut) ? ctx->errOut : stderr;

    if (at && at->t) {
        row = at->t->row;
        col = at->t->col;
    }

    fprintf(out, "Semantic error");
    if (row > 0) fprintf(out, " at %d:%d", row, col);
    fprintf(out, ": ");

    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);
    fputc('\n', out);

    if (ctx) ctx->errorCount++;
}

static SemScope *semNewScope(SemScope *parent) {
    SemScope *s = (SemScope *)calloc(1, sizeof(SemScope));
    if (!s) exit(1);
    s->parent = parent;
    return s;
}

static void semEnterScope(SemanticContext *ctx) {
    ctx->currentScope = semNewScope(ctx->currentScope);
}

static void semExitScope(SemanticContext *ctx) {
    if (ctx->currentScope) ctx->currentScope = ctx->currentScope->parent;
}

static SemSymbol *semLookupCurrentScope(SemanticContext *ctx, const char *name) {
    if (!ctx || !ctx->currentScope) return NULL;
    for (SemSymbol *s = ctx->currentScope->symbols; s; s = s->next) {
        if (strcmp(s->name, name) == 0) return s;
    }
    return NULL;
}

static SemSymbol *semLookupAnyScope(SemanticContext *ctx, const char *name) {
    for (SemScope *sc = ctx->currentScope; sc; sc = sc->parent) {
        for (SemSymbol *s = sc->symbols; s; s = s->next) {
            if (strcmp(s->name, name) == 0) return s;
        }
    }
    return NULL;
}

static SemSymbol *semAddSymbol(SemanticContext *ctx, node *at, const char *name, SemType type, int isFunction) {
    if (!ctx || !ctx->currentScope) return NULL;
    if (semLookupCurrentScope(ctx, name)) {
        semReport(ctx, at, "redeclaration of '%s'", name);
        return NULL;
    }

    SemSymbol *sym = (SemSymbol *)calloc(1, sizeof(SemSymbol));
    if (!sym) exit(1);

    strncpy(sym->name, name, sizeof(sym->name) - 1);
    sym->type = type;
    sym->isFunction = isFunction;

    sym->next = ctx->currentScope->symbols;
    ctx->currentScope->symbols = sym;
    return sym;
}

static SemSymbol *semAddImplicitFunction(SemanticContext *ctx, const char *name) {
    if (!ctx || !ctx->globalScope || !name || !name[0]) return NULL;

    for (SemSymbol *s = ctx->globalScope->symbols; s; s = s->next) {
        if (strcmp(s->name, name) == 0) {
            if (s->isFunction) return s;
            return NULL;
        }
    }

    SemSymbol *sym = (SemSymbol *)calloc(1, sizeof(SemSymbol));
    if (!sym) exit(1);

    strncpy(sym->name, name, sizeof(sym->name) - 1);
    sym->type = SEM_TYPE_INT;
    sym->isFunction = 1;
    sym->isDefined = 0;
    sym->paramCount = -1; // unknown/variadic-style external function

    sym->next = ctx->globalScope->symbols;
    ctx->globalScope->symbols = sym;
    return sym;
}

static SemSymbol *semAddImplicitVariable(SemanticContext *ctx, const char *name) {
    if (!ctx || !ctx->globalScope || !name || !name[0]) return NULL;

    for (SemSymbol *s = ctx->globalScope->symbols; s; s = s->next) {
        if (strcmp(s->name, name) == 0) {
            if (!s->isFunction) return s;
            return NULL;
        }
    }

    SemSymbol *sym = (SemSymbol *)calloc(1, sizeof(SemSymbol));
    if (!sym) exit(1);

    strncpy(sym->name, name, sizeof(sym->name) - 1);
    sym->type = SEM_TYPE_UNKNOWN;
    sym->isFunction = 0;

    sym->next = ctx->globalScope->symbols;
    ctx->globalScope->symbols = sym;
    return sym;
}

static SemType semAnalyzeExpression(SemanticContext *ctx, node *expr);

static void semCollectParamTypes(node *paramList, SemType outTypes[32], int *outCount) {
    *outCount = 0;
    if (!paramList) return;
    if (semNodeIs(semChildAt(paramList, 0), PT_EPSILON)) return;

    node *cursor = paramList;
    while (cursor && semNodeIs(cursor, PT_PARAM_LIST)) {
        node *param = semChildAt(cursor, 0);
        if (semNodeIs(param, PT_PARAM) && *outCount < 32) {
            outTypes[*outCount] = semTypeFromTypeNode(semChildAt(param, 0));
            (*outCount)++;
        }

        node *tail = semChildAt(cursor, 1);
        if (!tail || !semNodeIs(tail, PT_PARAM_LIST_TAIL)) break;
        if (semNodeIs(semChildAt(tail, 0), PT_EPSILON)) break;

        node *nextParam = semChildAt(tail, 1);
        if (semNodeIs(nextParam, PT_PARAM) && *outCount < 32) {
            outTypes[*outCount] = semTypeFromTypeNode(semChildAt(nextParam, 0));
            (*outCount)++;
        }

        cursor = semChildAt(tail, 2);
    }
}

static void semRegisterParamsIntoScope(SemanticContext *ctx, node *paramList) {
    if (!paramList || semNodeIs(semChildAt(paramList, 0), PT_EPSILON)) return;

    node *cursor = paramList;
    while (cursor && semNodeIs(cursor, PT_PARAM_LIST)) {
        node *param = semChildAt(cursor, 0);
        if (semNodeIs(param, PT_PARAM)) {
            SemType t = semTypeFromTypeNode(semChildAt(param, 0));
            node *idNode = NULL;

            for (node *k = param->child; k; k = k->next) {
                if (k->type == PT_IDENTIFIER) {
                    idNode = k;
                    break;
                }
            }

            if (idNode && idNode->t) {
                semAddSymbol(ctx, idNode, idNode->t->tokenValue, t, 0);
            }
        }

        node *tail = semChildAt(cursor, 1);
        if (!tail || !semNodeIs(tail, PT_PARAM_LIST_TAIL)) break;
        if (semNodeIs(semChildAt(tail, 0), PT_EPSILON)) break;

        node *nextParam = semChildAt(tail, 1);
        if (semNodeIs(nextParam, PT_PARAM)) {
            SemType t = semTypeFromTypeNode(semChildAt(nextParam, 0));
            node *idNode = NULL;
            for (node *k = nextParam->child; k; k = k->next) {
                if (k->type == PT_IDENTIFIER) {
                    idNode = k;
                    break;
                }
            }
            if (idNode && idNode->t) {
                semAddSymbol(ctx, idNode, idNode->t->tokenValue, t, 0);
            }
        }

        cursor = semChildAt(tail, 2);
    }
}

static void semAnalyzeDeclaration(SemanticContext *ctx, node *decl);
static void semAnalyzeStatement(SemanticContext *ctx, node *stmt);
static void semAnalyzeCompoundStmt(SemanticContext *ctx, node *compound, int createNewScope);

static void semAnalyzeInitDeclarator(SemanticContext *ctx, SemType declType, node *initDecl) {
    if (!initDecl || !semNodeIs(initDecl, PT_INIT_DECLARATOR)) return;

    node *idNode = semChildAt(initDecl, 0);
    if (!semNodeIs(idNode, PT_IDENTIFIER) || !idNode->t) return;

    SemSymbol *sym = semAddSymbol(ctx, idNode, idNode->t->tokenValue, declType, 0);

    for (node *c = idNode->next; c; c = c->next) {
        if (c->type == PT_ASSIGN) {
            node *rhs = c->next;
            if (!rhs) break;
            if (rhs->type == PT_LBRACE) continue;

            SemType rhsType = semAnalyzeExpression(ctx, rhs);
            if (sym && !semTypeCompatible(sym->type, rhsType)) {
                semReport(ctx, rhs, "cannot initialize '%s' (%s) with %s",
                          sym->name, semTypeName(sym->type), semTypeName(rhsType));
            }
            break;
        }
    }
}

static void semAnalyzeInitDeclaratorList(SemanticContext *ctx, SemType declType, node *initList) {
    if (!initList || !semNodeIs(initList, PT_INIT_DECLARATOR_LIST)) return;

    node *first = semChildAt(initList, 0);
    if (semNodeIs(first, PT_INIT_DECLARATOR)) {
        semAnalyzeInitDeclarator(ctx, declType, first);
    }

    node *tail = semChildAt(initList, 1);
    while (tail && semNodeIs(tail, PT_INIT_DECLARATOR_LIST_TAIL)) {
        if (semNodeIs(semChildAt(tail, 0), PT_EPSILON)) break;

        node *decl = semChildAt(tail, 1);
        if (semNodeIs(decl, PT_INIT_DECLARATOR)) {
            semAnalyzeInitDeclarator(ctx, declType, decl);
        }

        tail = semChildAt(tail, 2);
    }
}

static void semAnalyzeDeclaration(SemanticContext *ctx, node *decl) {
    if (!decl || !semNodeIs(decl, PT_DECLARATION)) return;
    SemType declType = semTypeFromTypeNode(semChildAt(decl, 0));
    semAnalyzeInitDeclaratorList(ctx, declType, semChildAt(decl, 1));
}

static int semCountAndCheckArgs(SemanticContext *ctx, node *argList, SemType outArgTypes[32]) {
    int count = 0;
    if (!argList || !semNodeIs(argList, PT_ARG_LIST)) return 0;

    node *first = semChildAt(argList, 0);
    if (semNodeIs(first, PT_EPSILON)) return 0;

    outArgTypes[count++] = semAnalyzeExpression(ctx, first);

    node *tail = semChildAt(argList, 1);
    while (tail && semNodeIs(tail, PT_ARG_LIST_TAIL)) {
        if (semNodeIs(semChildAt(tail, 0), PT_EPSILON)) break;

        node *expr = semChildAt(tail, 1);
        if (expr && count < 32) outArgTypes[count++] = semAnalyzeExpression(ctx, expr);

        tail = semChildAt(tail, 2);
    }

    return count;
}

static SemType semAnalyzePrimary(SemanticContext *ctx, node *primary) {
    if (!primary) return SEM_TYPE_UNKNOWN;

    node *c0 = semChildAt(primary, 0);
    if (!c0) return SEM_TYPE_UNKNOWN;

    if (c0->type == PT_IDENTIFIER) {
        if (semNodeIs(semChildAt(primary, 1), PT_LPAREN)) {
            SemSymbol *fn = c0->t ? semLookupAnyScope(ctx, c0->t->tokenValue) : NULL;
            if (!fn) {
                fn = semAddImplicitFunction(ctx, c0->t ? c0->t->tokenValue : NULL);
                if (!fn) {
                    semReport(ctx, c0, "call to undeclared function '%s'", c0->t ? c0->t->tokenValue : "?");
                    return SEM_TYPE_UNKNOWN;
                }
            }
            if (!fn->isFunction) {
                semReport(ctx, c0, "'%s' is not a function", fn->name);
                return SEM_TYPE_UNKNOWN;
            }

            SemType argTypes[32] = {0};
            int argc = semCountAndCheckArgs(ctx, semChildAt(primary, 2), argTypes);
            if (fn->paramCount >= 0 && argc != fn->paramCount) {
                semReport(ctx, c0, "function '%s' expects %d args but got %d", fn->name, fn->paramCount, argc);
            } else if (fn->paramCount >= 0) {
                for (int i = 0; i < argc; i++) {
                    if (!semTypeCompatible(fn->params[i], argTypes[i])) {
                        semReport(ctx, c0, "arg %d of '%s' expects %s but got %s",
                                  i + 1, fn->name, semTypeName(fn->params[i]), semTypeName(argTypes[i]));
                    }
                }
            }

            return fn->type;
        }

        SemSymbol *sym = c0->t ? semLookupAnyScope(ctx, c0->t->tokenValue) : NULL;
        if (!sym) {
            sym = semAddImplicitVariable(ctx, c0->t ? c0->t->tokenValue : NULL);
            if (!sym) {
                semReport(ctx, c0, "use of undeclared identifier '%s'", c0->t ? c0->t->tokenValue : "?");
                return SEM_TYPE_UNKNOWN;
            }
        }
        return sym->type;
    }

    if (c0->type == PT_INTEGER_LITERAL) return SEM_TYPE_INT;
    if (c0->type == PT_CHAR_LITERAL) {
        if (c0->t && strcmp(c0->t->tokenName, "stringLit") == 0) {
            return SEM_TYPE_UNKNOWN;
        }
        return SEM_TYPE_CHAR;
    }
    if (c0->type == PT_LPAREN) return semAnalyzeExpression(ctx, semChildAt(primary, 1));
    return SEM_TYPE_UNKNOWN;
}

static SemType semAnalyzeUnary(SemanticContext *ctx, node *unary) {
    node *c0 = semChildAt(unary, 0);
    if (!c0) return SEM_TYPE_UNKNOWN;

    if (c0->type == PT_MINUS) {
        SemType t = semAnalyzeUnary(ctx, semChildAt(unary, 1));
        if (!(t == SEM_TYPE_INT || t == SEM_TYPE_CHAR || t == SEM_TYPE_UNKNOWN)) {
            semReport(ctx, c0, "unary '-' requires numeric operand");
        }
        return SEM_TYPE_INT;
    }

    if (c0->type == PT_LOGICAL_NOT) {
        SemType t = semAnalyzeUnary(ctx, semChildAt(unary, 1));
        if (!(t == SEM_TYPE_INT || t == SEM_TYPE_CHAR || t == SEM_TYPE_UNKNOWN)) {
            semReport(ctx, c0, "logical '!' requires scalar operand");
        }
        return SEM_TYPE_INT;
    }

    if (c0->type == PT_AMPERSAND) {
        (void)semAnalyzeUnary(ctx, semChildAt(unary, 1));
        return SEM_TYPE_UNKNOWN;
    }

    return semAnalyzePrimary(ctx, c0);
}

static SemType semAnalyzeMultiplicative(SemanticContext *ctx, node *mul) {
    SemType lhs = semAnalyzeUnary(ctx, semChildAt(mul, 0));
    node *tail = semChildAt(mul, 1);

    while (tail && semNodeIs(tail, PT_MULTIPLICATIVE_EXPR_TAIL)) {
        node *op = semChildAt(tail, 0);
        if (semNodeIs(op, PT_EPSILON)) break;

        SemType rhs = semAnalyzeUnary(ctx, semChildAt(tail, 1));
        if (!(lhs == SEM_TYPE_INT || lhs == SEM_TYPE_CHAR || lhs == SEM_TYPE_UNKNOWN) ||
            !(rhs == SEM_TYPE_INT || rhs == SEM_TYPE_CHAR || rhs == SEM_TYPE_UNKNOWN)) {
            semReport(ctx, op, "arithmetic operator requires numeric operands");
        }

        lhs = SEM_TYPE_INT;
        tail = semChildAt(tail, 2);
    }

    return lhs;
}

static SemType semAnalyzeAdditive(SemanticContext *ctx, node *add) {
    SemType lhs = semAnalyzeMultiplicative(ctx, semChildAt(add, 0));
    node *tail = semChildAt(add, 1);

    while (tail && semNodeIs(tail, PT_ADDITIVE_EXPR_TAIL)) {
        node *op = semChildAt(tail, 0);
        if (semNodeIs(op, PT_EPSILON)) break;

        SemType rhs = semAnalyzeMultiplicative(ctx, semChildAt(tail, 1));
        if (!(lhs == SEM_TYPE_INT || lhs == SEM_TYPE_CHAR || lhs == SEM_TYPE_UNKNOWN) ||
            !(rhs == SEM_TYPE_INT || rhs == SEM_TYPE_CHAR || rhs == SEM_TYPE_UNKNOWN)) {
            semReport(ctx, op, "arithmetic operator requires numeric operands");
        }

        lhs = SEM_TYPE_INT;
        tail = semChildAt(tail, 2);
    }

    return lhs;
}

static SemType semAnalyzeRelational(SemanticContext *ctx, node *rel) {
    SemType lhs = semAnalyzeAdditive(ctx, semChildAt(rel, 0));
    node *tail = semChildAt(rel, 1);

    while (tail && semNodeIs(tail, PT_RELATIONAL_EXPR_TAIL)) {
        node *op = semChildAt(tail, 0);
        if (semNodeIs(op, PT_EPSILON)) break;

        SemType rhs = semAnalyzeAdditive(ctx, semChildAt(tail, 1));
        if (!(lhs == SEM_TYPE_INT || lhs == SEM_TYPE_CHAR || lhs == SEM_TYPE_UNKNOWN) ||
            !(rhs == SEM_TYPE_INT || rhs == SEM_TYPE_CHAR || rhs == SEM_TYPE_UNKNOWN)) {
            semReport(ctx, op, "relational operator requires numeric operands");
        }

        lhs = SEM_TYPE_INT;
        tail = semChildAt(tail, 2);
    }

    return lhs;
}

static SemType semAnalyzeEquality(SemanticContext *ctx, node *eq) {
    SemType lhs = semAnalyzeRelational(ctx, semChildAt(eq, 0));
    node *tail = semChildAt(eq, 1);

    while (tail && semNodeIs(tail, PT_EQUALITY_EXPR_TAIL)) {
        node *op = semChildAt(tail, 0);
        if (semNodeIs(op, PT_EPSILON)) break;

        SemType rhs = semAnalyzeRelational(ctx, semChildAt(tail, 1));
        if (!semTypeCompatible(lhs, rhs)) {
            semReport(ctx, op, "cannot compare %s with %s", semTypeName(lhs), semTypeName(rhs));
        }

        lhs = SEM_TYPE_INT;
        tail = semChildAt(tail, 2);
    }

    return lhs;
}

static SemType semAnalyzeLogicalAnd(SemanticContext *ctx, node *land) {
    SemType lhs = semAnalyzeEquality(ctx, semChildAt(land, 0));
    node *tail = semChildAt(land, 1);

    while (tail && semNodeIs(tail, PT_LOGICAL_AND_EXPR_TAIL)) {
        node *op = semChildAt(tail, 0);
        if (semNodeIs(op, PT_EPSILON)) break;

        SemType rhs = semAnalyzeEquality(ctx, semChildAt(tail, 1));
        if (!(lhs == SEM_TYPE_INT || lhs == SEM_TYPE_CHAR || lhs == SEM_TYPE_UNKNOWN) ||
            !(rhs == SEM_TYPE_INT || rhs == SEM_TYPE_CHAR || rhs == SEM_TYPE_UNKNOWN)) {
            semReport(ctx, op, "logical operator requires scalar operands");
        }

        lhs = SEM_TYPE_INT;
        tail = semChildAt(tail, 2);
    }

    return lhs;
}

static SemType semAnalyzeLogicalOr(SemanticContext *ctx, node *lor) {
    SemType lhs = semAnalyzeLogicalAnd(ctx, semChildAt(lor, 0));
    node *tail = semChildAt(lor, 1);

    while (tail && semNodeIs(tail, PT_LOGICAL_OR_EXPR_TAIL)) {
        node *op = semChildAt(tail, 0);
        if (semNodeIs(op, PT_EPSILON)) break;

        SemType rhs = semAnalyzeLogicalAnd(ctx, semChildAt(tail, 1));
        if (!(lhs == SEM_TYPE_INT || lhs == SEM_TYPE_CHAR || lhs == SEM_TYPE_UNKNOWN) ||
            !(rhs == SEM_TYPE_INT || rhs == SEM_TYPE_CHAR || rhs == SEM_TYPE_UNKNOWN)) {
            semReport(ctx, op, "logical operator requires scalar operands");
        }

        lhs = SEM_TYPE_INT;
        tail = semChildAt(tail, 2);
    }

    return lhs;
}

static SemType semAnalyzeExpression(SemanticContext *ctx, node *expr) {
    if (!expr || !semNodeIs(expr, PT_EXPRESSION)) return SEM_TYPE_UNKNOWN;

    node *c0 = semChildAt(expr, 0);
    node *c1 = semChildAt(expr, 1);
    node *c2 = semChildAt(expr, 2);

    if (semNodeIs(c0, PT_IDENTIFIER) && semNodeIs(c1, PT_ASSIGN)) {
        SemSymbol *lhs = c0->t ? semLookupAnyScope(ctx, c0->t->tokenValue) : NULL;
        if (!lhs) {
            semReport(ctx, c0, "assignment to undeclared identifier '%s'", c0->t ? c0->t->tokenValue : "?");
            semAnalyzeExpression(ctx, c2);
            return SEM_TYPE_UNKNOWN;
        }

        if (lhs->isFunction) {
            semReport(ctx, c0, "cannot assign to function '%s'", lhs->name);
        }

        SemType rhs = semAnalyzeExpression(ctx, c2);
        if (!semTypeCompatible(lhs->type, rhs)) {
            semReport(ctx, c1, "cannot assign %s to %s", semTypeName(rhs), semTypeName(lhs->type));
        }

        return lhs->type;
    }

    if (semNodeIs(c0, PT_LOGICAL_OR_EXPR)) return semAnalyzeLogicalOr(ctx, c0);
    return SEM_TYPE_UNKNOWN;
}

static void semAnalyzeReturnStmt(SemanticContext *ctx, node *retStmt) {
    node *opt = semChildAt(retStmt, 1);
    node *expr = opt ? semChildAt(opt, 0) : NULL;
    int hasExpr = expr && !semNodeIs(expr, PT_EPSILON);

    if (!ctx->inFunction) {
        semReport(ctx, retStmt, "'return' used outside function");
        if (hasExpr) semAnalyzeExpression(ctx, expr);
        return;
    }

    if (ctx->currentFunctionReturn == SEM_TYPE_VOID && hasExpr) {
        semReport(ctx, retStmt, "void function should not return a value");
        semAnalyzeExpression(ctx, expr);
        return;
    }

    if (ctx->currentFunctionReturn != SEM_TYPE_VOID && !hasExpr) {
        semReport(ctx, retStmt, "non-void function must return a value");
        return;
    }

    if (hasExpr) {
        SemType got = semAnalyzeExpression(ctx, expr);
        if (!semTypeCompatible(ctx->currentFunctionReturn, got)) {
            semReport(ctx, retStmt, "return type mismatch: expected %s but got %s",
                      semTypeName(ctx->currentFunctionReturn), semTypeName(got));
        }
    }
}

static void semAnalyzeStatement(SemanticContext *ctx, node *stmt) {
    if (!stmt || !semNodeIs(stmt, PT_STATEMENT)) return;
    node *body = semChildAt(stmt, 0);
    if (!body) return;

    if (body->type == PT_COMPOUND_STMT) {
        semAnalyzeCompoundStmt(ctx, body, 1);
        return;
    }

    if (body->type == PT_DECLARATION) {
        semAnalyzeDeclaration(ctx, body);
        return;
    }

    if (body->type == PT_EXPRESSION_STMT) {
        node *expr = semChildAt(body, 0);
        if (expr && expr->type != PT_SEMICOLON) semAnalyzeExpression(ctx, expr);
        return;
    }

    if (body->type == PT_IF_STMT) {
        node *cond = semChildAt(body, 2);
        if (cond) semAnalyzeExpression(ctx, cond);

        node *thenStmt = semChildAt(body, 4);
        if (thenStmt) semAnalyzeStatement(ctx, thenStmt);

        node *opt = semChildAt(body, 5);
        if (opt && !semNodeIs(semChildAt(opt, 0), PT_EPSILON)) {
            node *elseStmt = semChildAt(opt, 1);
            if (elseStmt) semAnalyzeStatement(ctx, elseStmt);
        }
        return;
    }

    if (body->type == PT_WHILE_STMT) {
        node *cond = semChildAt(body, 2);
        if (cond) semAnalyzeExpression(ctx, cond);

        node *loopStmt = semChildAt(body, 4);
        if (loopStmt) semAnalyzeStatement(ctx, loopStmt);
        return;
    }

    if (body->type == PT_RETURN_STMT) {
        semAnalyzeReturnStmt(ctx, body);
        return;
    }
}

static void semAnalyzeStatementList(SemanticContext *ctx, node *stmtList) {
    node *cursor = stmtList;
    while (cursor && semNodeIs(cursor, PT_STATEMENT_LIST)) {
        node *first = semChildAt(cursor, 0);
        if (semNodeIs(first, PT_EPSILON)) break;

        if (semNodeIs(first, PT_STATEMENT)) semAnalyzeStatement(ctx, first);
        cursor = semChildAt(cursor, 1);
    }
}

static void semAnalyzeDeclarationList(SemanticContext *ctx, node *declList) {
    node *cursor = declList;
    while (cursor && semNodeIs(cursor, PT_DECLARATION_LIST)) {
        node *first = semChildAt(cursor, 0);
        if (semNodeIs(first, PT_EPSILON)) break;

        if (semNodeIs(first, PT_DECLARATION)) semAnalyzeDeclaration(ctx, first);
        cursor = semChildAt(cursor, 1);
    }
}

static void semAnalyzeCompoundStmt(SemanticContext *ctx, node *compound, int createNewScope) {
    if (!compound || !semNodeIs(compound, PT_COMPOUND_STMT)) return;

    if (createNewScope) semEnterScope(ctx);
    semAnalyzeDeclarationList(ctx, semChildAt(compound, 1));
    semAnalyzeStatementList(ctx, semChildAt(compound, 2));
    if (createNewScope) semExitScope(ctx);
}

static void semAnalyzeGlobalDeclRest(SemanticContext *ctx, SemType baseType, node *firstId, node *globalDeclRest) {
    if (firstId && firstId->t) {
        semAddSymbol(ctx, firstId, firstId->t->tokenValue, baseType, 0);
    }

    if (!globalDeclRest || !semNodeIs(globalDeclRest, PT_GLOBAL_DECL_REST)) return;

    node *tailNode = NULL;
    for (node *c = globalDeclRest->child; c; c = c->next) {
        if (c->type == PT_INIT_DECLARATOR_LIST_TAIL) {
            tailNode = c;
            break;
        }
    }

    node *tail = tailNode;
    while (tail && semNodeIs(tail, PT_INIT_DECLARATOR_LIST_TAIL)) {
        if (semNodeIs(semChildAt(tail, 0), PT_EPSILON)) break;

        node *decl = semChildAt(tail, 1);
        if (semNodeIs(decl, PT_INIT_DECLARATOR)) {
            semAnalyzeInitDeclarator(ctx, baseType, decl);
        }

        tail = semChildAt(tail, 2);
    }
}

static void semAnalyzeFunctionOrGlobal(SemanticContext *ctx, node *fg) {
    if (!fg || !semNodeIs(fg, PT_FUNCTION_OR_GLOBAL)) return;

    node *typeNode = semChildAt(fg, 0);
    node *idNode = semChildAt(fg, 1);
    node *restNode = semChildAt(fg, 2);

    if (!idNode || !idNode->t || !restNode) return;

    SemType baseType = semTypeFromTypeNode(typeNode);
    node *restFirst = semChildAt(restNode, 0);

    if (semNodeIs(restFirst, PT_LPAREN)) {
        node *paramList = semChildAt(restNode, 1);
        node *functionRest = semChildAt(restNode, 3);

        SemType params[32] = {0};
        int paramCount = 0;
        semCollectParamTypes(paramList, params, &paramCount);

        SemSymbol *existing = semLookupCurrentScope(ctx, idNode->t->tokenValue);
        SemSymbol *fn = existing;

        if (existing && !existing->isFunction) {
            semReport(ctx, idNode, "'%s' already declared as variable", idNode->t->tokenValue);
            return;
        }

        if (!fn) {
            fn = semAddSymbol(ctx, idNode, idNode->t->tokenValue, baseType, 1);
            if (!fn) return;
            fn->paramCount = paramCount;
            for (int i = 0; i < paramCount; i++) fn->params[i] = params[i];
        } else {
            if (fn->type != baseType || fn->paramCount != paramCount) {
                semReport(ctx, idNode, "conflicting declaration for function '%s'", fn->name);
            } else {
                for (int i = 0; i < paramCount; i++) {
                    if (fn->params[i] != params[i]) {
                        semReport(ctx, idNode, "conflicting parameter types for function '%s'", fn->name);
                        break;
                    }
                }
            }
        }

        node *frFirst = functionRest ? semChildAt(functionRest, 0) : NULL;
        if (frFirst && frFirst->type == PT_COMPOUND_STMT) {
            if (fn->isDefined) {
                semReport(ctx, idNode, "redefinition of function '%s'", fn->name);
                return;
            }

            fn->isDefined = 1;
            semEnterScope(ctx);
            semRegisterParamsIntoScope(ctx, paramList);

            SemType prevRet = ctx->currentFunctionReturn;
            int prevInFn = ctx->inFunction;

            ctx->currentFunctionReturn = baseType;
            ctx->inFunction = 1;

            semAnalyzeCompoundStmt(ctx, frFirst, 0);

            ctx->currentFunctionReturn = prevRet;
            ctx->inFunction = prevInFn;
            semExitScope(ctx);
        }
        return;
    }

    if (semNodeIs(restFirst, PT_GLOBAL_DECL_REST)) {
        semAnalyzeGlobalDeclRest(ctx, baseType, idNode, restFirst);
    }
}

static void semAnalyzeExternalDeclList(SemanticContext *ctx, node *list) {
    node *cursor = list;
    while (cursor && semNodeIs(cursor, PT_EXTERNAL_DECL_LIST)) {
        node *first = semChildAt(cursor, 0);
        if (semNodeIs(first, PT_EPSILON)) break;

        if (semNodeIs(first, PT_EXTERNAL_DECL)) {
            node *fg = semChildAt(first, 0);
            if (semNodeIs(fg, PT_FUNCTION_OR_GLOBAL)) {
                semAnalyzeFunctionOrGlobal(ctx, fg);
            }
        }

        cursor = semChildAt(cursor, 1);
    }
}

static int doSemanticChecks(node *root, FILE *errOut) {
    if (!root || root->type == PT_ERROR) return 1;

    SemanticContext ctx;
    memset(&ctx, 0, sizeof(ctx));

    ctx.errOut = errOut ? errOut : stderr;
    ctx.globalScope = semNewScope(NULL);
    ctx.currentScope = ctx.globalScope;

    if (!semNodeIs(root, PT_PROGRAM)) {
        fprintf(ctx.errOut, "Semantic error: invalid parse tree root\n");
        return 1;
    }

    semAnalyzeExternalDeclList(&ctx, semChildAt(root, 0));
    return ctx.errorCount;
}

#endif
