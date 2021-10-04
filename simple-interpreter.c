#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

struct Expression;
typedef struct Expression* (*operator_t)(struct Expression*, struct Expression*);

typedef struct Expression {
    operator_t op;
    struct Expression* lhs;
    struct Expression* rhs;
}*expression_t;

expression_t execute(expression_t expression)
{
    assert(expression != NULL);

    if (expression->op == NULL) {
        assert(expression->rhs == NULL);
        assert(expression->lhs != NULL);
        return expression;
    }
    else {
        assert(expression->rhs != NULL);
        assert(expression->lhs != NULL);
        return expression->op(
            execute(expression->lhs)->lhs,
            execute(expression->rhs)->lhs
        );
    }
}

void* alloc_table[1024];
size_t alloc_index;

void* alloc(size_t sz)
{
    return alloc_table[alloc_index++] = calloc(sz, 1);
}

void free_all()
{
    while (alloc_index--)
        free(alloc_table[alloc_index]);
    alloc_index = 0;
}

void* temp_alloc_table[256];
size_t temp_alloc_index;

void* temp_alloc(size_t sz)
{
    return temp_alloc_table[temp_alloc_index++] = calloc(sz, 1);
}

void temp_free_all()
{
    while (alloc_index--)
        free(temp_alloc_table[temp_alloc_index]);
    temp_alloc_index = 0;
}

expression_t value(int v)
{
    expression_t p = alloc(sizeof(struct Expression));
    assert(p != NULL);
    p->op = NULL;
    p->lhs = alloc(sizeof(int));
    p->rhs = NULL;
    *((int*)p->lhs) = v;
    return p;
}

expression_t temp_value(int v)
{
    expression_t p = alloc(sizeof(struct Expression));
    assert(p != NULL);
    p->op = NULL;
    p->lhs = temp_alloc(sizeof(int));
    p->rhs = NULL;
    *((int*)p->lhs) = v;
    return p;
}

expression_t operation(operator_t op, expression_t lhs, expression_t rhs)
{
    expression_t p = alloc(sizeof(struct Expression));
    assert(p != NULL);
    p->op = op;
    p->lhs = lhs;
    p->rhs = rhs;

    return p;
}

expression_t add(expression_t rhs, expression_t lhs)
{
    return temp_value(*(int*)lhs + *(int*)rhs);
}

expression_t mul(expression_t rhs, expression_t lhs)
{
    return temp_value(*(int*)lhs * *(int*)rhs);
}

expression_t sub(expression_t rhs, expression_t lhs)
{
    return temp_value(*(int*)lhs - *(int*)rhs);
}

expression_t divide(expression_t rhs, expression_t lhs)
{
    if (*(int*)rhs == 0)
        return temp_value(*(int*)rhs);
    else
        return temp_value(*(int*)lhs / *(int*)rhs);
}

expression_t set(expression_t rhs, expression_t lhs)
{
    *(int*)lhs = *(int*)rhs;
    return temp_value(*(int*)lhs);
}

enum {
    VARIABLE_TABLE_MAX = 1024,
    IDENTIFIER_LENGTH_MAX = 32
};

typedef struct ExpressionNode {
    char key[IDENTIFIER_LENGTH_MAX];
    expression_t value;
    struct ExpressionNode* next;
}expression_node_t;

expression_node_t* variable_table[VARIABLE_TABLE_MAX] = { NULL };

uint32_t hash(const char* str)
{
    register long x;
    register size_t i;
    register const unsigned char* p = (const unsigned char*)str;
    x = *p << 7;
    for (i = 0; str[i]; i++)
        x = (1000003 * x) ^ *p++;
    x ^= i;
    return x;
}

expression_t value_of_identifier(const char* identifier)
{
    expression_node_t** list = &variable_table[hash(identifier) % VARIABLE_TABLE_MAX];

    assert(list);

    while (*list != NULL && strcmp(identifier, (*list)->key) != 0)
        list = &((*list)->next);

    if (*list == NULL) {
        const size_t len = strlen(identifier);
        assert(len <= IDENTIFIER_LENGTH_MAX);

        expression_node_t* node = *list = alloc(sizeof(expression_node_t));
        strcpy(node->key, identifier);
        node->next = NULL;

        return node->value = value(0);
    }
    else {
        return (*list)->value;
    }
}

int is_alpha(char c)
{
    c |= 32;
    return 'a' <= c && c <= 'z';
}

int is_digit(char c)
{
    return '0' <= c && c <= '9';
}

int run()
{
    char code[256];
    if(scanf("%256[^\n]%*c", code) <= 0)
        return 0;

    expression_t stack[256];
    size_t stack_index = 0;
    size_t i = 0;
    while (code[i] != '\0' && code[i] != '\r') {
        switch (code[i]) {
        case '\r': ++i;
            break;
        case '(': ++i;
            stack[stack_index++] = NULL;
            break;
        case ')': ++i;
            assert(stack_index >= 4);
            expression_t lhs = stack[--stack_index];
            expression_t rhs = stack[--stack_index];
            expression_t op = stack[--stack_index];
            assert(stack[--stack_index] == NULL);
            stack[stack_index++] = operation((operator_t)op, lhs, rhs);
            break;
        case '+': ++i;
            stack[stack_index++] = (expression_t)add;
            break;
        case '*': ++i;
            stack[stack_index++] = (expression_t)mul;
            break;
        case '-': ++i;
            stack[stack_index++] = (expression_t)sub;
            break;
        case '/': ++i;
            stack[stack_index++] = (expression_t)divide;
            break;
        case ' ': ++i;
            break;
        case '=': ++i;
            stack[stack_index++] = (expression_t)set;
            break;
        default:
            if ('0' <= code[i] && code[i] <= '9') {
                int v = 0;
                while ('0' <= code[i] && code[i] <= '9')
                    v = v * 10 + code[i++] - '0';

                stack[stack_index++] = temp_value(v);
                break;
            }
            else {
                char identifier[IDENTIFIER_LENGTH_MAX] = { '\0' };
                size_t j = 0;
                while (is_alpha(code[i]) || is_digit(code[i])) {
                    identifier[j++] = code[i++];
                    assert(j < IDENTIFIER_LENGTH_MAX);
                }
                assert(j != 0); // length of identifier is 0
                stack[stack_index++] = value_of_identifier(identifier);
            }
        }
    }

    for (size_t j = 0; j != stack_index; ++j)
        printf("%d\n", *(int*)execute(stack[j])->lhs);

    temp_free_all();
    return 1;
}

int main()
{
    while (run());
    free_all();
    return 0;
}

#if 0
대입 연산자가 추가되었습니다.

in:
(= a (+3 4))
(= b (-3 4))
(* a b)

out:
7
-1
-7
#endif
