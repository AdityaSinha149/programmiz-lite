# Grammar for Restricted C-like Language

## Language Restrictions

- No struct, union, enum
- No pointers
- No typedef
- No switch
- Functions, prototypes, and global variables allowed
- Block scoping supported
- Expressions with assignment and operators

---

## 1. Start Symbol

Program
 → ExternalDeclList EOF

ExternalDeclList
 → ExternalDecl ExternalDeclList
 | ε

ExternalDecl
 → FunctionOrGlobal

---

## 2. Function and Global Declarations

FunctionOrGlobal
 → Type IDENTIFIER FunctionOrGlobalRest

FunctionOrGlobalRest
 → '(' ParamList ')' FunctionRest
 | GlobalDeclRest

FunctionRest
 → ';'
 | CompoundStmt

GlobalDeclRest
 → InitDeclaratorListTail ';'

---

## 3. Types

Type
 → 'int'
 | 'char'
 | 'void'

---

## 4. Parameters

ParamList
 → Param ParamListTail
 | ε

ParamListTail
 → ',' Param ParamListTail
 | ε

Param
 → Type IDENTIFIER

---

## 5. Compound Statement

CompoundStmt
 → '{' DeclarationList StatementList '}'

---

## 6. Declarations

DeclarationList
 → Declaration DeclarationList
 | ε

Declaration
 → Type InitDeclaratorList ';'

InitDeclaratorList
 → InitDeclarator InitDeclaratorListTail

InitDeclaratorListTail
 → ',' InitDeclarator InitDeclaratorListTail
 | ε

InitDeclarator
 → IDENTIFIER
 | IDENTIFIER '=' Expression

---

## 7. Statements

StatementList
 → Statement StatementList
 | ε

Statement
 → ExpressionStmt
 | CompoundStmt
 | IfStmt
 | WhileStmt
 | ReturnStmt

---

## 8. Statement Forms

ExpressionStmt
 → Expression ';'
 | ';'

IfStmt
 → 'if' '(' Expression ')' Statement IfElseOpt

IfElseOpt
 → 'else' Statement
 | ε

WhileStmt
 → 'while' '(' Expression ')' Statement

ReturnStmt
 → 'return' ReturnExprOpt ';'

ReturnExprOpt
 → Expression
 | ε

---

## 9. Expressions

Expression
 → IDENTIFIER '=' Expression
 | LogicalOrExpr

LogicalOrExpr
 → LogicalAndExpr LogicalOrExpr'

LogicalOrExpr'
 → '||' LogicalAndExpr LogicalOrExpr'
 | ε

LogicalAndExpr
 → EqualityExpr LogicalAndExpr'

LogicalAndExpr'
 → '&&' EqualityExpr LogicalAndExpr'
 | ε

EqualityExpr
 → RelationalExpr EqualityExpr'

EqualityExpr'
 → '==' RelationalExpr EqualityExpr'
 | '!=' RelationalExpr EqualityExpr'
 | ε

RelationalExpr
 → AdditiveExpr RelationalExpr'

RelationalExpr'
 → '<' AdditiveExpr RelationalExpr'
 | '>' AdditiveExpr RelationalExpr'
 | '<=' AdditiveExpr RelationalExpr'
 | '>=' AdditiveExpr RelationalExpr'
 | ε

AdditiveExpr
 → MultiplicativeExpr AdditiveExpr'

AdditiveExpr'
 → '+' MultiplicativeExpr AdditiveExpr'
 | '-' MultiplicativeExpr AdditiveExpr'
 | ε

MultiplicativeExpr
 → UnaryExpr MultiplicativeExpr'

MultiplicativeExpr'
 → '*' UnaryExpr MultiplicativeExpr'
 | '/' UnaryExpr MultiplicativeExpr'
 | '%' UnaryExpr MultiplicativeExpr'
 | ε

UnaryExpr
 → '-' UnaryExpr
 | '!' UnaryExpr
 | PrimaryExpr

PrimaryExpr
 → IDENTIFIER
 | IDENTIFIER '(' ArgList ')'
 | INTEGER_LITERAL
 | CHAR_LITERAL
 | '(' Expression ')'

ArgList
 → Expression ArgListTail
 | ε

ArgListTail
 → ',' Expression ArgListTail
 | ε