#ifndef GENERATOR_H
#define GENERATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>

#include "parsing.h"

typedef enum {
    GEN_TYPE_INT = 0,
    GEN_TYPE_CHAR,
    GEN_TYPE_VOID
} GenType;

typedef struct GenVar {
    char name[64];
    LLVMValueRef addr;
    GenType type;
    struct GenVar *next;
} GenVar;

typedef struct GenScope {
    struct GenScope *parent;
    GenVar *vars;
} GenScope;

typedef struct {
    LLVMContextRef context;
    LLVMModuleRef module;
    LLVMBuilderRef builder;

    LLVMValueRef currentFunction;
    GenType currentReturnType;
    LLVMValueRef returnSlot;
    LLVMBasicBlockRef returnBlock;

    GenScope *scope;
    int stringCounter;
} GenContext;

static node *genChildAt(node *n, int idx) {
    node *c = n ? n->child : NULL;
    for (int i = 0; c && i < idx; i++) c = c->next;
    return c;
}

static int genNodeIs(node *n, nodeType t) {
    return n && n->type == t;
}

static int genIsEpsilon(node *n) {
    return genNodeIs(n, PT_EPSILON);
}

static GenType genTypeFromTypeNode(node *typeNode) {
    if (!typeNode || !typeNode->t) return GEN_TYPE_INT;
    if (strcmp(typeNode->t->tokenValue, "char") == 0) return GEN_TYPE_CHAR;
    if (strcmp(typeNode->t->tokenValue, "void") == 0) return GEN_TYPE_VOID;
    return GEN_TYPE_INT;
}

static LLVMTypeRef genLLVMType(GenContext *ctx, GenType t) {
    if (t == GEN_TYPE_CHAR) return LLVMInt8TypeInContext(ctx->context);
    if (t == GEN_TYPE_VOID) return LLVMVoidTypeInContext(ctx->context);
    return LLVMInt32TypeInContext(ctx->context);
}

static int genParseIntToken(node *n) {
    if (!n || !n->t) return 0;
    if (n->t->tokenValue[0]) return atoi(n->t->tokenValue);
    if (n->t->tokenName[0]) return atoi(n->t->tokenName);
    return 0;
}

static int genParseCharToken(node *n) {
    if (!n || !n->t) return 0;
    if (n->t->tokenValue[0]) return (unsigned char)n->t->tokenValue[0];
    if (n->t->tokenName[0]) return (unsigned char)n->t->tokenName[0];
    return 0;
}

static void genPushScope(GenContext *ctx) {
    GenScope *s = (GenScope *)calloc(1, sizeof(GenScope));
    if (!s) exit(1);
    s->parent = ctx->scope;
    ctx->scope = s;
}

static void genPopScope(GenContext *ctx) {
    if (!ctx->scope) return;
    GenScope *s = ctx->scope;
    ctx->scope = s->parent;

    GenVar *v = s->vars;
    while (v) {
        GenVar *next = v->next;
        free(v);
        v = next;
    }
    free(s);
}

static void genAddVar(GenContext *ctx, const char *name, LLVMValueRef addr, GenType t) {
    if (!ctx || !ctx->scope || !name || !name[0]) return;
    GenVar *v = (GenVar *)calloc(1, sizeof(GenVar));
    if (!v) exit(1);
    strncpy(v->name, name, sizeof(v->name) - 1);
    v->addr = addr;
    v->type = t;
    v->next = ctx->scope->vars;
    ctx->scope->vars = v;
}

static GenVar *genLookupVar(GenContext *ctx, const char *name) {
    for (GenScope *s = ctx->scope; s; s = s->parent) {
        for (GenVar *v = s->vars; v; v = v->next) {
            if (strcmp(v->name, name) == 0) return v;
        }
    }
    return NULL;
}

static LLVMValueRef genToI32(GenContext *ctx, LLVMValueRef v) {
    LLVMTypeRef t = LLVMTypeOf(v);
    if (LLVMGetTypeKind(t) != LLVMIntegerTypeKind) return v;
    unsigned bw = LLVMGetIntTypeWidth(t);
    if (bw == 32) return v;
    if (bw < 32) return LLVMBuildSExt(ctx->builder, v, LLVMInt32TypeInContext(ctx->context), "sexttmp");
    return LLVMBuildTrunc(ctx->builder, v, LLVMInt32TypeInContext(ctx->context), "trunctmp");
}

static LLVMValueRef genPromoteCallArg(GenContext *ctx, LLVMValueRef v) {
    LLVMTypeRef t = LLVMTypeOf(v);
    if (LLVMGetTypeKind(t) != LLVMIntegerTypeKind) return v;
    unsigned bw = LLVMGetIntTypeWidth(t);
    if (bw < 32) return LLVMBuildSExt(ctx->builder, v, LLVMInt32TypeInContext(ctx->context), "argprom");
    return v;
}

static LLVMValueRef genCastForStore(GenContext *ctx, LLVMValueRef v, LLVMTypeRef dstType) {
    LLVMTypeRef srcType = LLVMTypeOf(v);
    if (srcType == dstType) return v;
    if (LLVMGetTypeKind(srcType) != LLVMIntegerTypeKind || LLVMGetTypeKind(dstType) != LLVMIntegerTypeKind) {
        return v;
    }

    unsigned srcW = LLVMGetIntTypeWidth(srcType);
    unsigned dstW = LLVMGetIntTypeWidth(dstType);
    if (srcW < dstW) return LLVMBuildSExt(ctx->builder, v, dstType, "castup");
    if (srcW > dstW) return LLVMBuildTrunc(ctx->builder, v, dstType, "castdown");
    return v;
}

static LLVMValueRef genToBool(GenContext *ctx, LLVMValueRef v) {
    LLVMValueRef i32v = genToI32(ctx, v);
    LLVMValueRef zero = LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, 0);
    return LLVMBuildICmp(ctx->builder, LLVMIntNE, i32v, zero, "tobool");
}

static LLVMValueRef genCreateEntryAlloca(GenContext *ctx, LLVMTypeRef ty, const char *name) {
    LLVMBuilderRef b = LLVMCreateBuilderInContext(ctx->context);
    LLVMBasicBlockRef entry = LLVMGetEntryBasicBlock(ctx->currentFunction);
    LLVMValueRef first = LLVMGetFirstInstruction(entry);

    if (first) LLVMPositionBuilderBefore(b, first);
    else LLVMPositionBuilderAtEnd(b, entry);

    LLVMValueRef allocaInst = LLVMBuildAlloca(b, ty, name);
    LLVMDisposeBuilder(b);
    return allocaInst;
}

static LLVMValueRef genEmitStringLiteral(GenContext *ctx, const char *text) {
    const char *s = text ? text : "";
    size_t len = strlen(s);
    if (len > 1023) len = 1023;

    char name[64];
    snprintf(name, sizeof(name), ".str.%d", ctx->stringCounter++);

    LLVMTypeRef i8Ty = LLVMInt8TypeInContext(ctx->context);
    LLVMTypeRef arrTy = LLVMArrayType(i8Ty, (unsigned)(len + 1));
    LLVMValueRef g = LLVMAddGlobal(ctx->module, arrTy, name);
    LLVMSetLinkage(g, LLVMPrivateLinkage);
    LLVMSetGlobalConstant(g, 1);

    LLVMValueRef init = LLVMConstStringInContext(ctx->context, s, (unsigned)len, 0);
    LLVMSetInitializer(g, init);

    LLVMValueRef idx[2] = {
        LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, 0),
        LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, 0)
    };

    return LLVMBuildInBoundsGEP2(ctx->builder, arrTy, g, idx, 2, "strptr");
}

static LLVMValueRef genEmitExpression(GenContext *ctx, node *expr);
static void genEmitStatement(GenContext *ctx, node *stmt);
static void genEmitCompoundStmt(GenContext *ctx, node *compound, int createScope);

static LLVMValueRef genEmitPrimary(GenContext *ctx, node *primary) {
    node *c0 = genChildAt(primary, 0);
    if (!c0) return LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, 0);

    if (c0->type == PT_IDENTIFIER) {
        node *c1 = genChildAt(primary, 1);
        if (genNodeIs(c1, PT_LPAREN)) {
            LLVMValueRef callee = c0->t ? LLVMGetNamedFunction(ctx->module, c0->t->tokenValue) : NULL;

            if (!callee && c0->t && c0->t->tokenValue[0]) {
                LLVMTypeRef fnTy = LLVMFunctionType(
                    LLVMInt32TypeInContext(ctx->context),
                    NULL,
                    0,
                    1
                );
                callee = LLVMAddFunction(ctx->module, c0->t->tokenValue, fnTy);
            }

            if (!callee) return LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, 0);

            LLVMValueRef args[32];
            unsigned argc = 0;

            node *argList = genChildAt(primary, 2);
            if (argList && genNodeIs(argList, PT_ARG_LIST)) {
                node *first = genChildAt(argList, 0);
                if (first && !genIsEpsilon(first)) {
                    args[argc++] = genPromoteCallArg(ctx, genEmitExpression(ctx, first));

                    node *tail = genChildAt(argList, 1);
                    while (tail && genNodeIs(tail, PT_ARG_LIST_TAIL)) {
                        if (genIsEpsilon(genChildAt(tail, 0))) break;
                        node *argExpr = genChildAt(tail, 1);
                        if (argExpr && argc < 32) {
                            args[argc++] = genPromoteCallArg(ctx, genEmitExpression(ctx, argExpr));
                        }
                        tail = genChildAt(tail, 2);
                    }
                }
            }

            LLVMTypeRef fnTy = LLVMGlobalGetValueType(callee);
            if (LLVMGetTypeKind(fnTy) != LLVMFunctionTypeKind) {
                fnTy = LLVMFunctionType(LLVMInt32TypeInContext(ctx->context), NULL, 0, 1);
            }

            LLVMValueRef call = LLVMBuildCall2(ctx->builder, fnTy, callee, args, argc, "calltmp");
            if (LLVMGetTypeKind(LLVMGetReturnType(fnTy)) == LLVMVoidTypeKind) {
                return LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, 0);
            }
            return genToI32(ctx, call);
        }

        if (!c0->t || !c0->t->tokenValue[0]) {
            return LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, 0);
        }

        GenVar *v = genLookupVar(ctx, c0->t->tokenValue);
        if (v) {
            LLVMValueRef loaded = LLVMBuildLoad2(ctx->builder, genLLVMType(ctx, v->type), v->addr, "loadtmp");
            return genToI32(ctx, loaded);
        }

        LLVMValueRef g = LLVMGetNamedGlobal(ctx->module, c0->t->tokenValue);
        if (g) {
            LLVMTypeRef gTy = LLVMGetElementType(LLVMTypeOf(g));
            LLVMValueRef loaded = LLVMBuildLoad2(ctx->builder, gTy, g, "gloadtmp");
            return genToI32(ctx, loaded);
        }

        return LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, 0);
    }

    if (c0->type == PT_INTEGER_LITERAL) {
        int val = genParseIntToken(c0);
        return LLVMConstInt(LLVMInt32TypeInContext(ctx->context), (uint64_t)val, 1);
    }

    if (c0->type == PT_CHAR_LITERAL) {
        if (c0->t && strcmp(c0->t->tokenName, "stringLit") == 0) {
            return genEmitStringLiteral(ctx, c0->t->tokenValue);
        }
        int val = genParseCharToken(c0);
        return LLVMConstInt(LLVMInt32TypeInContext(ctx->context), (uint64_t)val, 0);
    }

    if (c0->type == PT_LPAREN) {
        node *expr = genChildAt(primary, 1);
        return genToI32(ctx, genEmitExpression(ctx, expr));
    }

    return LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, 0);
}

static LLVMValueRef genEmitUnary(GenContext *ctx, node *unary) {
    node *c0 = genChildAt(unary, 0);
    if (!c0) return LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, 0);

    if (c0->type == PT_MINUS) {
        LLVMValueRef rhs = genToI32(ctx, genEmitUnary(ctx, genChildAt(unary, 1)));
        return LLVMBuildNeg(ctx->builder, rhs, "negtmp");
    }

    if (c0->type == PT_LOGICAL_NOT) {
        LLVMValueRef rhsBool = genToBool(ctx, genEmitUnary(ctx, genChildAt(unary, 1)));
        LLVMValueRef notv = LLVMBuildNot(ctx->builder, rhsBool, "nottmp");
        return LLVMBuildZExt(ctx->builder, notv, LLVMInt32TypeInContext(ctx->context), "booltoi32");
    }

    return genEmitPrimary(ctx, c0);
}

static LLVMValueRef genEmitMultiplicative(GenContext *ctx, node *mul) {
    LLVMValueRef lhs = genToI32(ctx, genEmitUnary(ctx, genChildAt(mul, 0)));
    node *tail = genChildAt(mul, 1);

    while (tail && genNodeIs(tail, PT_MULTIPLICATIVE_EXPR_TAIL)) {
        node *op = genChildAt(tail, 0);
        if (genIsEpsilon(op)) break;

        LLVMValueRef rhs = genToI32(ctx, genEmitUnary(ctx, genChildAt(tail, 1)));

        if (op->type == PT_STAR) lhs = LLVMBuildMul(ctx->builder, lhs, rhs, "multmp");
        else if (op->type == PT_SLASH) lhs = LLVMBuildSDiv(ctx->builder, lhs, rhs, "divtmp");
        else lhs = LLVMBuildSRem(ctx->builder, lhs, rhs, "modtmp");

        tail = genChildAt(tail, 2);
    }

    return lhs;
}

static LLVMValueRef genEmitAdditive(GenContext *ctx, node *add) {
    LLVMValueRef lhs = genToI32(ctx, genEmitMultiplicative(ctx, genChildAt(add, 0)));
    node *tail = genChildAt(add, 1);

    while (tail && genNodeIs(tail, PT_ADDITIVE_EXPR_TAIL)) {
        node *op = genChildAt(tail, 0);
        if (genIsEpsilon(op)) break;

        LLVMValueRef rhs = genToI32(ctx, genEmitMultiplicative(ctx, genChildAt(tail, 1)));
        if (op->type == PT_MINUS) lhs = LLVMBuildSub(ctx->builder, lhs, rhs, "subtmp");
        else lhs = LLVMBuildAdd(ctx->builder, lhs, rhs, "addtmp");

        tail = genChildAt(tail, 2);
    }

    return lhs;
}

static LLVMValueRef genEmitRelational(GenContext *ctx, node *rel) {
    LLVMValueRef lhs = genToI32(ctx, genEmitAdditive(ctx, genChildAt(rel, 0)));
    node *tail = genChildAt(rel, 1);

    while (tail && genNodeIs(tail, PT_RELATIONAL_EXPR_TAIL)) {
        node *op = genChildAt(tail, 0);
        if (genIsEpsilon(op)) break;

        LLVMValueRef rhs = genToI32(ctx, genEmitAdditive(ctx, genChildAt(tail, 1)));
        LLVMValueRef cmp = NULL;

        if (op->type == PT_LT) cmp = LLVMBuildICmp(ctx->builder, LLVMIntSLT, lhs, rhs, "lttmp");
        else if (op->type == PT_GT) cmp = LLVMBuildICmp(ctx->builder, LLVMIntSGT, lhs, rhs, "gttmp");
        else if (op->type == PT_LE) cmp = LLVMBuildICmp(ctx->builder, LLVMIntSLE, lhs, rhs, "letmp");
        else cmp = LLVMBuildICmp(ctx->builder, LLVMIntSGE, lhs, rhs, "getmp");

        lhs = LLVMBuildZExt(ctx->builder, cmp, LLVMInt32TypeInContext(ctx->context), "booltoi32");
        tail = genChildAt(tail, 2);
    }

    return lhs;
}

static LLVMValueRef genEmitEquality(GenContext *ctx, node *eq) {
    LLVMValueRef lhs = genToI32(ctx, genEmitRelational(ctx, genChildAt(eq, 0)));
    node *tail = genChildAt(eq, 1);

    while (tail && genNodeIs(tail, PT_EQUALITY_EXPR_TAIL)) {
        node *op = genChildAt(tail, 0);
        if (genIsEpsilon(op)) break;

        LLVMValueRef rhs = genToI32(ctx, genEmitRelational(ctx, genChildAt(tail, 1)));
        LLVMValueRef cmp = (op->type == PT_EQ)
            ? LLVMBuildICmp(ctx->builder, LLVMIntEQ, lhs, rhs, "eqtmp")
            : LLVMBuildICmp(ctx->builder, LLVMIntNE, lhs, rhs, "netmp");

        lhs = LLVMBuildZExt(ctx->builder, cmp, LLVMInt32TypeInContext(ctx->context), "booltoi32");
        tail = genChildAt(tail, 2);
    }

    return lhs;
}

static LLVMValueRef genEmitLogicalAnd(GenContext *ctx, node *land) {
    LLVMValueRef lhs = genToI32(ctx, genEmitEquality(ctx, genChildAt(land, 0)));
    node *tail = genChildAt(land, 1);

    while (tail && genNodeIs(tail, PT_LOGICAL_AND_EXPR_TAIL)) {
        node *op = genChildAt(tail, 0);
        if (genIsEpsilon(op)) break;
        (void)op;

        LLVMValueRef rhs = genToI32(ctx, genEmitEquality(ctx, genChildAt(tail, 1)));
        LLVMValueRef lb = genToBool(ctx, lhs);
        LLVMValueRef rb = genToBool(ctx, rhs);
        LLVMValueRef both = LLVMBuildAnd(ctx->builder, lb, rb, "andtmp");
        lhs = LLVMBuildZExt(ctx->builder, both, LLVMInt32TypeInContext(ctx->context), "booltoi32");

        tail = genChildAt(tail, 2);
    }

    return lhs;
}

static LLVMValueRef genEmitLogicalOr(GenContext *ctx, node *lor) {
    LLVMValueRef lhs = genToI32(ctx, genEmitLogicalAnd(ctx, genChildAt(lor, 0)));
    node *tail = genChildAt(lor, 1);

    while (tail && genNodeIs(tail, PT_LOGICAL_OR_EXPR_TAIL)) {
        node *op = genChildAt(tail, 0);
        if (genIsEpsilon(op)) break;
        (void)op;

        LLVMValueRef rhs = genToI32(ctx, genEmitLogicalAnd(ctx, genChildAt(tail, 1)));
        LLVMValueRef lb = genToBool(ctx, lhs);
        LLVMValueRef rb = genToBool(ctx, rhs);
        LLVMValueRef either = LLVMBuildOr(ctx->builder, lb, rb, "ortmp");
        lhs = LLVMBuildZExt(ctx->builder, either, LLVMInt32TypeInContext(ctx->context), "booltoi32");

        tail = genChildAt(tail, 2);
    }

    return lhs;
}

static LLVMValueRef genEmitExpression(GenContext *ctx, node *expr) {
    if (!expr || !genNodeIs(expr, PT_EXPRESSION)) {
        return LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, 0);
    }

    node *c0 = genChildAt(expr, 0);
    node *c1 = genChildAt(expr, 1);
    node *c2 = genChildAt(expr, 2);

    if (genNodeIs(c0, PT_IDENTIFIER) && genNodeIs(c1, PT_ASSIGN) && c0->t) {
        LLVMValueRef rhs = genToI32(ctx, genEmitExpression(ctx, c2));

        GenVar *v = genLookupVar(ctx, c0->t->tokenValue);
        if (v) {
            LLVMTypeRef dstTy = genLLVMType(ctx, v->type);
            LLVMValueRef casted = genCastForStore(ctx, rhs, dstTy);
            LLVMBuildStore(ctx->builder, casted, v->addr);
            return genToI32(ctx, casted);
        }

        LLVMValueRef g = LLVMGetNamedGlobal(ctx->module, c0->t->tokenValue);
        if (g) {
            LLVMTypeRef dstTy = LLVMGetElementType(LLVMTypeOf(g));
            LLVMValueRef casted = genCastForStore(ctx, rhs, dstTy);
            LLVMBuildStore(ctx->builder, casted, g);
            return genToI32(ctx, casted);
        }

        return rhs;
    }

    if (genNodeIs(c0, PT_LOGICAL_OR_EXPR)) {
        return genEmitLogicalOr(ctx, c0);
    }

    return LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, 0);
}

static void genEmitInitDeclarator(GenContext *ctx, GenType declType, node *initDecl) {
    if (!initDecl || !genNodeIs(initDecl, PT_INIT_DECLARATOR)) return;

    node *idNode = genChildAt(initDecl, 0);
    if (!idNode || !genNodeIs(idNode, PT_IDENTIFIER) || !idNode->t) return;

    LLVMTypeRef ty = genLLVMType(ctx, declType);
    LLVMValueRef addr = genCreateEntryAlloca(ctx, ty, idNode->t->tokenValue);
    genAddVar(ctx, idNode->t->tokenValue, addr, declType);

    LLVMBuildStore(ctx->builder, LLVMConstInt(ty, 0, 0), addr);

    for (node *c = idNode->next; c; c = c->next) {
        if (c->type == PT_ASSIGN) {
            node *rhsNode = c->next;
            if (!rhsNode) break;
            if (rhsNode->type == PT_LBRACE) break;
            LLVMValueRef rhs = genEmitExpression(ctx, rhsNode);
            rhs = genCastForStore(ctx, rhs, ty);
            LLVMBuildStore(ctx->builder, rhs, addr);
            break;
        }
    }
}

static void genEmitInitDeclaratorList(GenContext *ctx, GenType declType, node *initList) {
    if (!initList || !genNodeIs(initList, PT_INIT_DECLARATOR_LIST)) return;

    node *first = genChildAt(initList, 0);
    if (genNodeIs(first, PT_INIT_DECLARATOR)) {
        genEmitInitDeclarator(ctx, declType, first);
    }

    node *tail = genChildAt(initList, 1);
    while (tail && genNodeIs(tail, PT_INIT_DECLARATOR_LIST_TAIL)) {
        if (genIsEpsilon(genChildAt(tail, 0))) break;
        node *decl = genChildAt(tail, 1);
        if (genNodeIs(decl, PT_INIT_DECLARATOR)) {
            genEmitInitDeclarator(ctx, declType, decl);
        }
        tail = genChildAt(tail, 2);
    }
}

static void genEmitDeclaration(GenContext *ctx, node *decl) {
    if (!decl || !genNodeIs(decl, PT_DECLARATION)) return;
    GenType t = genTypeFromTypeNode(genChildAt(decl, 0));
    genEmitInitDeclaratorList(ctx, t, genChildAt(decl, 1));
}

static void genEmitStatementList(GenContext *ctx, node *stmtList) {
    node *cursor = stmtList;
    while (cursor && genNodeIs(cursor, PT_STATEMENT_LIST)) {
        node *first = genChildAt(cursor, 0);
        if (genIsEpsilon(first)) break;
        if (genNodeIs(first, PT_STATEMENT)) genEmitStatement(ctx, first);
        cursor = genChildAt(cursor, 1);
    }
}

static void genEmitDeclarationList(GenContext *ctx, node *declList) {
    node *cursor = declList;
    while (cursor && genNodeIs(cursor, PT_DECLARATION_LIST)) {
        node *first = genChildAt(cursor, 0);
        if (genIsEpsilon(first)) break;
        if (genNodeIs(first, PT_DECLARATION)) genEmitDeclaration(ctx, first);
        cursor = genChildAt(cursor, 1);
    }
}

static void genEmitStatement(GenContext *ctx, node *stmt) {
    if (!stmt || !genNodeIs(stmt, PT_STATEMENT)) return;
    node *body = genChildAt(stmt, 0);
    if (!body) return;

    if (body->type == PT_COMPOUND_STMT) {
        genEmitCompoundStmt(ctx, body, 1);
        return;
    }

    if (body->type == PT_DECLARATION) {
        genEmitDeclaration(ctx, body);
        return;
    }

    if (body->type == PT_EXPRESSION_STMT) {
        node *expr = genChildAt(body, 0);
        if (expr && expr->type != PT_SEMICOLON) {
            (void)genEmitExpression(ctx, expr);
        }
        return;
    }

    if (body->type == PT_IF_STMT) {
        LLVMValueRef condV = genEmitExpression(ctx, genChildAt(body, 2));
        LLVMValueRef condB = genToBool(ctx, condV);

        LLVMBasicBlockRef thenBB = LLVMAppendBasicBlockInContext(ctx->context, ctx->currentFunction, "if.then");
        LLVMBasicBlockRef mergeBB = LLVMAppendBasicBlockInContext(ctx->context, ctx->currentFunction, "if.end");

        node *opt = genChildAt(body, 5);
        int hasElse = opt && !genIsEpsilon(genChildAt(opt, 0));
        LLVMBasicBlockRef elseBB = hasElse
            ? LLVMAppendBasicBlockInContext(ctx->context, ctx->currentFunction, "if.else")
            : NULL;

        LLVMBuildCondBr(ctx->builder, condB, thenBB, hasElse ? elseBB : mergeBB);

        LLVMPositionBuilderAtEnd(ctx->builder, thenBB);
        genEmitStatement(ctx, genChildAt(body, 4));
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
            LLVMBuildBr(ctx->builder, mergeBB);
        }

        if (hasElse) {
            LLVMPositionBuilderAtEnd(ctx->builder, elseBB);
            genEmitStatement(ctx, genChildAt(opt, 1));
            if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
                LLVMBuildBr(ctx->builder, mergeBB);
            }
        }

        LLVMPositionBuilderAtEnd(ctx->builder, mergeBB);
        return;
    }

    if (body->type == PT_WHILE_STMT) {
        LLVMBasicBlockRef condBB = LLVMAppendBasicBlockInContext(ctx->context, ctx->currentFunction, "while.cond");
        LLVMBasicBlockRef bodyBB = LLVMAppendBasicBlockInContext(ctx->context, ctx->currentFunction, "while.body");
        LLVMBasicBlockRef endBB = LLVMAppendBasicBlockInContext(ctx->context, ctx->currentFunction, "while.end");

        LLVMBuildBr(ctx->builder, condBB);

        LLVMPositionBuilderAtEnd(ctx->builder, condBB);
        LLVMValueRef condV = genEmitExpression(ctx, genChildAt(body, 2));
        LLVMValueRef condB = genToBool(ctx, condV);
        LLVMBuildCondBr(ctx->builder, condB, bodyBB, endBB);

        LLVMPositionBuilderAtEnd(ctx->builder, bodyBB);
        genEmitStatement(ctx, genChildAt(body, 4));
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
            LLVMBuildBr(ctx->builder, condBB);
        }

        LLVMPositionBuilderAtEnd(ctx->builder, endBB);
        return;
    }

    if (body->type == PT_RETURN_STMT) {
        node *opt = genChildAt(body, 1);
        node *expr = opt ? genChildAt(opt, 0) : NULL;

        if (ctx->currentReturnType != GEN_TYPE_VOID) {
            LLVMTypeRef retTy = genLLVMType(ctx, ctx->currentReturnType);
            LLVMValueRef v = expr && !genIsEpsilon(expr)
                ? genEmitExpression(ctx, expr)
                : LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, 0);
            v = genCastForStore(ctx, v, retTy);
            LLVMBuildStore(ctx->builder, v, ctx->returnSlot);
        }
        LLVMBuildBr(ctx->builder, ctx->returnBlock);
        return;
    }
}

static void genEmitCompoundStmt(GenContext *ctx, node *compound, int createScope) {
    if (!compound || !genNodeIs(compound, PT_COMPOUND_STMT)) return;

    if (createScope) genPushScope(ctx);
    genEmitDeclarationList(ctx, genChildAt(compound, 1));
    genEmitStatementList(ctx, genChildAt(compound, 2));
    if (createScope) genPopScope(ctx);
}

static void genCollectParams(node *paramList, GenType types[32], char names[32][64], int *count) {
    *count = 0;
    if (!paramList) return;
    if (genIsEpsilon(genChildAt(paramList, 0))) return;

    node *cursor = paramList;
    while (cursor && genNodeIs(cursor, PT_PARAM_LIST)) {
        node *param = genChildAt(cursor, 0);
        if (param && genNodeIs(param, PT_PARAM) && *count < 32) {
            types[*count] = genTypeFromTypeNode(genChildAt(param, 0));
            node *idNode = NULL;
            for (node *c = param->child; c; c = c->next) {
                if (c->type == PT_IDENTIFIER) {
                    idNode = c;
                    break;
                }
            }
            if (idNode && idNode->t && idNode->t->tokenValue[0]) {
                strncpy(names[*count], idNode->t->tokenValue, 63);
                names[*count][63] = '\0';
            } else {
                names[*count][0] = '\0';
            }
            (*count)++;
        }

        node *tail = genChildAt(cursor, 1);
        if (!tail || !genNodeIs(tail, PT_PARAM_LIST_TAIL)) break;
        if (genIsEpsilon(genChildAt(tail, 0))) break;

        node *nextParam = genChildAt(tail, 1);
        if (nextParam && genNodeIs(nextParam, PT_PARAM) && *count < 32) {
            types[*count] = genTypeFromTypeNode(genChildAt(nextParam, 0));
            node *idNode = NULL;
            for (node *c = nextParam->child; c; c = c->next) {
                if (c->type == PT_IDENTIFIER) {
                    idNode = c;
                    break;
                }
            }
            if (idNode && idNode->t && idNode->t->tokenValue[0]) {
                strncpy(names[*count], idNode->t->tokenValue, 63);
                names[*count][63] = '\0';
            } else {
                names[*count][0] = '\0';
            }
            (*count)++;
        }

        cursor = genChildAt(tail, 2);
    }
}

static LLVMValueRef genEnsureFunctionDecl(GenContext *ctx, const char *name, GenType retType, GenType paramTypes[32], int paramCount) {
    LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, name);
    if (fn) return fn;

    LLVMTypeRef params[32];
    for (int i = 0; i < paramCount; i++) {
        params[i] = genLLVMType(ctx, paramTypes[i]);
    }

    LLVMTypeRef fnTy = LLVMFunctionType(genLLVMType(ctx, retType), params, (unsigned)paramCount, 0);
    fn = LLVMAddFunction(ctx->module, name, fnTy);
    return fn;
}

static void genEmitFunctionDef(GenContext *ctx, LLVMValueRef fn, GenType retType, GenType paramTypes[32], char paramNames[32][64], int paramCount, node *body) {
    if (!fn || !body) return;

    if (LLVMCountBasicBlocks(fn) > 0) return;

    ctx->currentFunction = fn;
    ctx->currentReturnType = retType;

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx->context, fn, "entry");
    ctx->returnBlock = LLVMAppendBasicBlockInContext(ctx->context, fn, "return");

    LLVMPositionBuilderAtEnd(ctx->builder, entry);

    genPushScope(ctx);

    for (int i = 0; i < paramCount; i++) {
        LLVMValueRef p = LLVMGetParam(fn, (unsigned)i);
        LLVMTypeRef pTy = genLLVMType(ctx, paramTypes[i]);
        LLVMValueRef slot = genCreateEntryAlloca(ctx, pTy, paramNames[i][0] ? paramNames[i] : "arg");
        LLVMBuildStore(ctx->builder, p, slot);
        if (paramNames[i][0]) genAddVar(ctx, paramNames[i], slot, paramTypes[i]);
    }

    if (retType != GEN_TYPE_VOID) {
        LLVMTypeRef retTy = genLLVMType(ctx, retType);
        ctx->returnSlot = genCreateEntryAlloca(ctx, retTy, "retval");
        LLVMBuildStore(ctx->builder, LLVMConstInt(retTy, 0, 0), ctx->returnSlot);
    } else {
        ctx->returnSlot = NULL;
    }

    genEmitCompoundStmt(ctx, body, 0);

    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))) {
        LLVMBuildBr(ctx->builder, ctx->returnBlock);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, ctx->returnBlock);
    if (retType == GEN_TYPE_VOID) {
        LLVMBuildRetVoid(ctx->builder);
    } else {
        LLVMTypeRef retTy = genLLVMType(ctx, retType);
        LLVMValueRef rv = LLVMBuildLoad2(ctx->builder, retTy, ctx->returnSlot, "retload");
        LLVMBuildRet(ctx->builder, rv);
    }

    genPopScope(ctx);
    ctx->currentFunction = NULL;
    ctx->returnSlot = NULL;
}

static void genEmitGlobalFromInitDecl(GenContext *ctx, GenType t, node *initDecl) {
    if (!initDecl || !genNodeIs(initDecl, PT_INIT_DECLARATOR)) return;

    node *idNode = genChildAt(initDecl, 0);
    if (!idNode || !genNodeIs(idNode, PT_IDENTIFIER) || !idNode->t) return;

    LLVMValueRef g = LLVMGetNamedGlobal(ctx->module, idNode->t->tokenValue);
    if (!g) {
        g = LLVMAddGlobal(ctx->module, genLLVMType(ctx, t), idNode->t->tokenValue);
        LLVMSetInitializer(g, LLVMConstInt(genLLVMType(ctx, t), 0, 0));
    }

    for (node *c = idNode->next; c; c = c->next) {
        if (c->type == PT_ASSIGN) {
            node *rhs = c->next;
            if (!rhs || rhs->type == PT_LBRACE) break;
            if (rhs->type == PT_EXPRESSION) {
                node *r0 = genChildAt(rhs, 0);
                if (r0 && r0->type == PT_LOGICAL_OR_EXPR) {
                    int val = 0;
                    node *land = genChildAt(r0, 0);
                    if (land) {
                        node *eq = genChildAt(land, 0);
                        if (eq) {
                            node *rel = genChildAt(eq, 0);
                            if (rel) {
                                node *add = genChildAt(rel, 0);
                                if (add) {
                                    node *mul = genChildAt(add, 0);
                                    if (mul) {
                                        node *un = genChildAt(mul, 0);
                                        if (un) {
                                            node *pr = genChildAt(un, 0);
                                            if (pr) {
                                                node *lit = genChildAt(pr, 0);
                                                if (lit && lit->type == PT_INTEGER_LITERAL) val = genParseIntToken(lit);
                                                if (lit && lit->type == PT_CHAR_LITERAL) val = genParseCharToken(lit);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    LLVMSetInitializer(g, LLVMConstInt(genLLVMType(ctx, t), (uint64_t)val, 1));
                }
            }
            break;
        }
    }
}

static void genEmitGlobalDeclRest(GenContext *ctx, GenType t, node *firstId, node *rest) {
    if (firstId && firstId->t) {
        LLVMValueRef g = LLVMGetNamedGlobal(ctx->module, firstId->t->tokenValue);
        if (!g) {
            LLVMTypeRef ty = genLLVMType(ctx, t);
            node *rb = rest ? genChildAt(rest, 2) : NULL;
            node *sizeNode = rest ? genChildAt(rest, 1) : NULL;
            node *lb = rest ? genChildAt(rest, 0) : NULL;
            if (lb && lb->type == PT_LBRACKET && sizeNode && sizeNode->type == PT_INTEGER_LITERAL && rb && rb->type == PT_RBRACKET) {
                int n = genParseIntToken(sizeNode);
                if (n < 1) n = 1;
                ty = LLVMArrayType(ty, (unsigned)n);
            }

            g = LLVMAddGlobal(ctx->module, ty, firstId->t->tokenValue);
            LLVMSetInitializer(g, LLVMConstNull(ty));
        }
    }

    node *tailNode = NULL;
    for (node *c = rest ? rest->child : NULL; c; c = c->next) {
        if (c->type == PT_INIT_DECLARATOR_LIST_TAIL) {
            tailNode = c;
            break;
        }
    }

    node *tail = tailNode;
    while (tail && genNodeIs(tail, PT_INIT_DECLARATOR_LIST_TAIL)) {
        if (genIsEpsilon(genChildAt(tail, 0))) break;
        node *decl = genChildAt(tail, 1);
        if (genNodeIs(decl, PT_INIT_DECLARATOR)) {
            genEmitGlobalFromInitDecl(ctx, t, decl);
        }
        tail = genChildAt(tail, 2);
    }
}

static void genEmitFunctionOrGlobal(GenContext *ctx, node *fg) {
    if (!fg || !genNodeIs(fg, PT_FUNCTION_OR_GLOBAL)) return;

    node *typeNode = genChildAt(fg, 0);
    node *idNode = genChildAt(fg, 1);
    node *restNode = genChildAt(fg, 2);

    if (!idNode || !idNode->t || !restNode) return;

    GenType baseType = genTypeFromTypeNode(typeNode);
    node *restFirst = genChildAt(restNode, 0);

    if (genNodeIs(restFirst, PT_LPAREN)) {
        node *paramList = genChildAt(restNode, 1);
        node *functionRest = genChildAt(restNode, 3);

        GenType paramTypes[32];
        char paramNames[32][64];
        int paramCount = 0;
        genCollectParams(paramList, paramTypes, paramNames, &paramCount);

        LLVMValueRef fn = genEnsureFunctionDecl(ctx, idNode->t->tokenValue, baseType, paramTypes, paramCount);

        node *frFirst = functionRest ? genChildAt(functionRest, 0) : NULL;
        if (frFirst && frFirst->type == PT_COMPOUND_STMT) {
            genEmitFunctionDef(ctx, fn, baseType, paramTypes, paramNames, paramCount, frFirst);
        }
        return;
    }

    if (genNodeIs(restFirst, PT_GLOBAL_DECL_REST)) {
        genEmitGlobalDeclRest(ctx, baseType, idNode, restFirst);
    }
}

static void genEmitExternalDeclList(GenContext *ctx, node *list) {
    node *cursor = list;
    while (cursor && genNodeIs(cursor, PT_EXTERNAL_DECL_LIST)) {
        node *first = genChildAt(cursor, 0);
        if (genIsEpsilon(first)) break;

        if (genNodeIs(first, PT_EXTERNAL_DECL)) {
            node *fg = genChildAt(first, 0);
            if (genNodeIs(fg, PT_FUNCTION_OR_GLOBAL)) {
                genEmitFunctionOrGlobal(ctx, fg);
            }
        }

        cursor = genChildAt(cursor, 1);
    }
}

static LLVMModuleRef generateLLVMModuleFromParseTree(node *root, LLVMContextRef context, const char *moduleName) {
    if (!root || !genNodeIs(root, PT_PROGRAM)) return NULL;

    GenContext gc;
    memset(&gc, 0, sizeof(gc));

    gc.context = context;
    gc.module = LLVMModuleCreateWithNameInContext(moduleName ? moduleName : "c_compiler_module", context);
    gc.builder = LLVMCreateBuilderInContext(context);

    genPushScope(&gc);
    genEmitExternalDeclList(&gc, genChildAt(root, 0));
    genPopScope(&gc);

    char *err = NULL;
    LLVMVerifyModule(gc.module, LLVMPrintMessageAction, &err);
    if (err) LLVMDisposeMessage(err);

    LLVMDisposeBuilder(gc.builder);
    return gc.module;
}

static int writeModuleIRToFile(LLVMModuleRef module, const char *path) {
    if (!module || !path) return 1;

    char *ir = LLVMPrintModuleToString(module);
    if (!ir) return 1;

    FILE *out = fopen(path, "w");
    if (!out) {
        LLVMDisposeMessage(ir);
        return 1;
    }

    fputs(ir, out);
    fclose(out);
    LLVMDisposeMessage(ir);
    return 0;
}

#endif
