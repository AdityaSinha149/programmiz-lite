#ifndef STACK_H
#define STACK_H

#define STACK_SIZE 100
#include "lexAnalyszer.h"

typedef struct stack{
    char arr[STACK_SIZE];
    int top;
}stack;

// prototypes
static void pushStack(stack *s, char c);
static char popStack(stack *s);
static int isEmptyStack(stack *s);

static void pushStack(stack *s, char c) {
    if (s->top == STACK_SIZE - 1)
        return;

    s->arr[++(s->top)] = c;
}

static char popStack(stack *s) {
    if (s->top == -1)
        return '\0';

    return s->arr[(s->top)--];
}

static int isEmptyStack(stack *s) {
    return s->top == -1;
}

#endif
