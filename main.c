#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define INIT_TOKEN_BUF_SIZE 16
#define LEX_ITER_BUF_SIZE 32

#undef EOF

// there is a lot of unnecessary allocation
// but i like it when my stuff stays in place

typedef struct _LexIter {
    char* str;
    int len;
    int cap;

    int cur;
} LexIter;

LexIter* lex_iter(char* s, int len) {
    LexIter* l = malloc(sizeof(LexIter));

    l->str = malloc(len);
    l->len = len;
    l->cap = len;

    l->cur = 0;

    // not sure if s has been allocated
    // so we memcpy to the new, allocated string
    memcpy(l->str, s, len);

    return l;
}

void free_lex_iter(LexIter* l) {
    free(l->str);
    free(l);
}

void lex_iter_buf_reserve(LexIter* liter, int len) {
    // if we dont have enough space
    if (liter->len + len > liter->cap) {
        // reserve more space
        // double cap to minimize reallocs

        // prevent reallocating unallocated memory
        if (liter->cap < 1) {
            liter->cap = 1;
        }

        liter->cap *= 2;
        liter->str = realloc(liter->str, liter->cap);

        // rerun this function to make sure doubling is enough
        lex_iter_buf_reserve(liter, len);
    }
}

void lex_iter_buf_append(LexIter* liter, char* s, int len) {
    // make sure we have space
    lex_iter_buf_reserve(liter, len);

    // memcpy new string to end of lexiter string
    memcpy(liter->str + liter->len, s, len);

    // update length
    liter->len += len;
}

// generate lexiter directly from user input 
LexIter* prompt(char* prompt) {
    // display prompt
    printf("%s", prompt);
    fflush(stdout);

    // create lexiter and buffer
    LexIter* l = lex_iter("", 0);

    char buf[LEX_ITER_BUF_SIZE];
    char c = 0;

    // fill buffer, then append it to the lexiter
    for (int i = 0;; i++) {
        // grab c and add it to the buffer
        c = getchar();
        buf[i] = c;

        // if its a newline
        if (c == '\n') {
            // then append it and break
            lex_iter_buf_append(l, buf, i);
            break;
        }

        // else if we have reached the end of the buffer
        else if (i + 1 == LEX_ITER_BUF_SIZE) {
            // append to lexiter
            lex_iter_buf_append(l, buf, LEX_ITER_BUF_SIZE);

            // reset buffer
            i = -1;
        }
    }
}

// lengthy name but 
// prints a lexiter and its info
// as long as it comes from prompt() or uses LEX_ITER_BUF_SIZE in some way
void print_lex_iter_prompt_verbose(LexIter* l) {
    int bufs = (int)(l->len / LEX_ITER_BUF_SIZE);

    // multiply it back together, if its equal to length, then its an exact number of buffers used
    // otherwise, its not exact and it will round down when converted to int, and we add one
    bufs += (LEX_ITER_BUF_SIZE * bufs == l->len) ? 0 : 1;

    printf("Prompt Info:\n len: %i\n cap: %i\n bufs used: %i\n\n", l->len, l->cap, bufs);
    printf(" :%s\n", l->str);
}

typedef struct _Span {
    int start;
    int end;
} Span;

Span span(int s, int e) {
    return (Span){ s, e };
}

// finds length of the span
// this is useful bc now we have a way to find
// both token length and value length
int span_len(Span s) {
    return s.end - s.start;
}

#define null_span (Span){-1, -1}

typedef enum _TokenType {
    TODO,
    IDENT, STR, INT, FLOAT, PLUS, MINUS, TIMES, OVER, BSLASH, DOT, COMMA, COLON, SEMI, LPAR, RPAR, LBRAC, RBRAC, LCBRAC, RCBRAC, PIPE, DPIPE, EXC, AT, HASH, QUESTION, DOLLAR, PERCENT, TILDE, CARET, TICK, AND, DAND, GTE, GT, LTE, LT, DCOLON,
    EOF,
} TokenType;

typedef struct _Token {
    TokenType type;
    void* data;
    float fdata;

    // bytes that data takes up
    int len;

    // start and end pos of token
    Span span;
} Token;

Token* token(TokenType type, void* data, int len, Span span) {
    Token* t = malloc(sizeof(Token));

    t->type = type;
    t->len = len;

    t->span = span;

    t->fdata = 0.0;

    t->data = malloc(len);
    memcpy(t->data, data, len);

    return t;
}

typedef struct _TokenBuf {
    // token array
    Token** tokens;
    int cur;

    int len;
    int cap;
} TokenBuf;

#define tok_sized(count) (count * sizeof(Token*))

TokenBuf* token_buf() {
    TokenBuf* tb = malloc(sizeof(TokenBuf));

    tb->len = 0;
    tb->cap = INIT_TOKEN_BUF_SIZE;
    tb->cur = 0;

    tb->tokens = malloc(tok_sized(tb->cap));

    return tb;
}

void free_token(Token* t) {
    free(t->data);
    free(t);
}

void free_token_buf(TokenBuf* tb) {
    for (int i = 0; i < tb->len; i++) {
        Token* t = tb->tokens[i];
        free_token(t);
    }
    free(tb);
}

void print_token(Token* t, FILE* out) {
    fprintf(out,
        "Tok {type: %i, data: %s, len: %i, fdata: %f span: %i -> %i, slen: %i}\n",
        t->type, (char*)t->data, t->len, t->fdata, t->span.start, t->span.end, span_len(t->span)
    );
}

void print_token_buf_verbose(TokenBuf* tb, FILE* out) {
    for (int i = 0; i < tb->len; i++) {
        fprintf(out, "%i. ", i + 1);
        print_token(tb->tokens[i], out);
    }
}

void token_buf_reserve(TokenBuf* tb, int tok_count) {
    // if we dont have enough space
    if (tb->len + tok_count > tb->cap) {
        // then realloc
        // double buf size for miminum reallocs
        tb->cap *= 2;
        tb->tokens = realloc(tb->tokens, tok_sized(tb->cap));

        // rerun this function to make sure doubling was enough
        token_buf_reserve(tb, tok_count);
    }
}

void tok_append(TokenBuf* tb, Token* t) {
    // make sure we have enough space
    token_buf_reserve(tb, 1);

    // append token
    tb->tokens[tb->len] = t;

    // update length
    tb->len++;
}

// if the char is contained in the string
// string has to have a terminating NUL
int is_in(char c, char* str) {
    for (int i = 0; str[i] != 0; i++) {
        if (c == str[i]) {
            return 1;
        }
    }

    return 0;
}

// if the char is contained in the string
// does not have to have terminating NUL
int is_in_len(char c, char* str, int len) {
    for (int i = 0; i < len; i++) {
        if (c == str[i]) {
            return 1;
        }
    }

    return 0;
}

// generate tokens from an input in the form of a lexiter
TokenBuf* lex(LexIter* l) {
    char c;

    TokenBuf* tb = token_buf();

    // define rules for lexing
    #define rule(match, type, data, size, span) else if (c == match) { tok_append(tb, token(type, data, size, span)); }
    #define rule_str(match, t, d, s, span) else if (is_in(c, match)) { tok_append(tb, token(t, d, s, span)); }

    #define tappend(type, data, size, span) tok_append(tb, token(type, data, size, span))

    #define idx l->cur
    #define sspan(length) span(idx, idx + length)

    #define default_rargs(len) &c, len, sspan(len)

    #define lex_advance() c = l->str[++(l->cur)]
    #define reverse() c = l->str[--(l->cur)]

    #define rule2char(first, firsttype, second, secondtype) if (c == first) { lex_advance(); if (c == second) { tappend(secondtype, &c, 2, sspan(2)); } else { reverse(); tappend(firsttype, &c, 1, sspan(1)); } }

    int start = l->cur;

    // iterate over the whole string
    for (; l->cur < l->len; l->cur++) {
        // grab current char
        c = l->str[l->cur];
        start = l->cur;

        // placeholder to let elseifs happen
        if (0) {}

        rule('+', PLUS, &c, 1, sspan(1))
        rule('-', MINUS, &c, 1, sspan(1))
        rule('*', TIMES, &c, 1, sspan(1))
        rule('/', OVER, &c, 1, sspan(1))
        rule('\\', BSLASH, &c, 1, sspan(1))
        rule(';', SEMI, &c, 1, sspan(1))
        rule('.', DOT, &c, 1, sspan(1))
        rule(',', COMMA, &c, 1, sspan(1))

        rule('(', LPAR, &c, 1, sspan(1))
        rule(')', RPAR, &c, 1, sspan(1))
        rule('{', LCBRAC, &c, 1, sspan(1))
        rule('}', RCBRAC, &c, 1, sspan(1))
        rule('[', LBRAC, &c, 1, sspan(1))
        rule(']', RBRAC, &c, 1, sspan(1))
        rule('!', EXC, &c, 1, sspan(1))
        rule('@', AT, &c, 1, sspan(1))
        rule('#', HASH, &c, 1, sspan(1))
        rule('$', DOLLAR, &c, 1, sspan(1))
        rule('%', PERCENT, &c, 1, sspan(1))
        rule('^', CARET, &c, 1, sspan(1))
        rule('~', TILDE, &c, 1, sspan(1))
        rule('`', TICK, &c, 1, sspan(1))
        rule('?', QUESTION, &c, 1, sspan(1))

        // comparisons/equality && ||
        rule2char('|', PIPE, '|', DPIPE)
        rule2char('&', AND, '&', DAND)
        rule2char('>', GT, '=', GTE)
        rule2char('<', LT, '=', LTE)
        rule2char(':', COLON, ':', DCOLON)

        else {
            // use lexiter as buffer for these things
            LexIter* buf = lex_iter("", 0);

            // whiles for things that take any number of characters

            if (is_in(c, "abcdefghijklmnopqrstuvwxyz_")) {
                while (is_in(c, "abcdefghijklmnopqrstuvwxyz_0123456789")) {
                    lex_iter_buf_append(buf, &c, 1);
                    lex_advance();
                }

                reverse();

                tok_append(tb, token(IDENT, buf->str, buf->len, span(start, l->cur + 1)));
            }

            // numbers.
            // these are a big fat issue
            // we will have two number types
            // integer (no decimals)
            // float (one decimal point)
            // more than one decimal point will throw an error
            // we ignore underscores bc they can be used to make it easier to read  numbers for the coder
            else if (is_in(c, "0123456789")) {
                // gather all of the numbers and period info
                int dots = 0;

                while (is_in(c, "0123456789._")) {
                    if (c != '_') {
                        if (c == '.') {
                            dots++;
                        }
                        lex_iter_buf_append(buf, &c, 1);
                    }
                    lex_advance();
                }

                reverse();

                // now we process all of it
                if (dots == 0) {
                    // process int
                    int res = 0;

                    for (int i = 0, chars = buf->len; i < chars; i++) {
                        res *= 10;
                        res += (buf->str[i] - '0');
                    }

                    tok_append(tb, token(INT, &res, sizeof(int), span(start, l->cur + 1)));
                }
                else if (dots == 1) {
                    // process float
                    double res = 0.0;
                    int e = 0;
                    int i;
                    int chars = buf->len;

                    // int part
                    for (i = 0; i < chars && buf->str[i] != '.'; i++) {
                        res = (res * 10.0) + (buf->str[i] - '0');
                    }
                    // error check
                    if (buf->str[i] == '.') {
                        lex_advance();
                    } else {
                        // otherwise we have an error bc something besides a period got in
                        fprintf(stderr, "Error: poorly parsed float starting at %i", start);
                        exit(-1);
                    }
                    // dec part
                    for (; i < chars; i++) {
                        res = (res * 10.0) + (buf->str[i] - '0');
                        e--;
                    }
                    // correct dec part with e
                    while (e < 0) {
                        res *= 0.1;
                        e++;
                    }

                    // correct even more
                    reverse();

                    // now we have final float in res
                    // append as token
                    tok_append(tb, token(FLOAT, (void*)0, 0, span(start, l->cur + 1)));
                    tb->tokens[tb->len-1]->fdata = 1.001 * (res > 0 ? res : -res);
                }
                else if (dots > 1) {
                    fprintf(stderr, "Error: too many decimal points in tok starting at %i", start);
                    exit(-1);
                }
                else if (dots < 0) {
                    fprintf(stderr, "Error: negative periods????");
                    exit(-1);
                }
            }

            else if (c == '\"') {
                char last;

                lex_advance();

                do {
                    lex_iter_buf_append(buf, &c, 1);

                    // check if weve gone past the buffer
                    if (l->cur > l->len) {
                        // print error and exit
                        fprintf(stderr, "Error: Mismatched quotes starting at %i\n", start);
                        exit(-1);
                    }

                    lex_advance();
                } while (c != '\"');

                tok_append(tb, token(STR, buf->str, buf->len, span(start, l->cur + 1)));
            }

            // whitespace
            if (is_in(c, " \t\n")) {
                while (is_in(c, " \t\n")) {
                    lex_advance();
                }

                reverse();
            }

            free_lex_iter(buf);
        }
    }

    // add EOF
    tok_append(tb, token(EOF, NULL, 0, span(l->len + 1, l->len + 1)));

    return tb;
}

// iterate through all of the tokens provided
// check if they are valid 
// each invalidity will reduce the result
int tokens_valid(TokenBuf* tb) {
    int res = 1;

    for (int i = 0; i < tb->len; i++) {
        Token* t = tb->tokens[i];

        // checks
        if (t->type <= TODO || t->type > EOF) {
            res--;
        }
        if (t->type == FLOAT) {
            // could be an issue if user uses 0.0 as a float value
            if (t->fdata == 0.0) {
                res--;
            }
        }
    }
}

// parser
typedef enum _NodeType {
    BinOp, UnOp, Value, 
} NodeType;

struct _Node {
    NodeType type;

    struct Node** children;
    int child_count;

    Token* data;

    Token* op;
};
typedef struct _Node Node;
typedef struct _Node AST;

Node* node(NodeType type, int children, Token* op, Token* data) {
    Node* n = malloc(sizeof(Node));

    n->type = type;
    n->child_count = children;
    n->op = op;

    n->data = data;

    n->children = malloc(sizeof(Node*) * n->child_count);

    return n;
}

Node* binopnode(Node* child1, Node* child2, Token* op) {
    Node* n = node(BinOp, 2, op, NULL);

    n->children[0] = child1;
    n->children[1] = child2;

    return n;
}

Node* unopnode(Node* child, Token* op) {
    Node* n = node(UnOp, 1, op, NULL);

    n->children[0] = child;

    return n;
}

Node* valnode(Token* data) {
    Node* n = node(Value, 0, NULL, data);

    return n;
}

// validate node
int validate_node(Node* n) {
    // 
}

// eat token
// dont know if we want to have that ++ or not
// since its eating it, it implies advancing the token buffer so idk
void eattok(TokenBuf* tb, TokenType expected) {
    TokenType found;

    if ((found = tb->tokens[tb->cur++]->type) != expected) {
        // then we have an error bc we have the wrong token type
        fprintf(stderr, "Error: expected %i, found %i", expected, found);
    }
}

Token* peek(TokenBuf* tb) {
    return tb->tokens[tb->cur + 1];
}

// grab next token and step tokenbuf
Token* advance(TokenBuf* tb) {
    return tb->tokens[++tb->cur];
}

// variadic args take what kind of token it can be
// essentially peek()
int nextis(TokenBuf* tb, int ct, ...) {
    va_list ptr;
    va_start(ptr, ct);

    TokenType curtype;
    TokenType type = peek(tb)->type;

    for (int i = 0; i < ct; i++) {
        curtype = va_arg(ptr, TokenType);

        if (type == curtype) {
            va_end(ptr);
            return 1;
        }
    }

    va_end(ptr);

    return 0;
}

// tokens get parsed in tiers
// each priority tier of operation will be generated by one function
// thats explained terrible but
// basically since addition and subtraction are done last, they will be in the highest tier
// this way they will be done last when the interpreter works its way up the tree from the bottom

// the token buffer serves as the iterator/parser object

// highest tier - expression: addition, subtraction
// second highest - term: multiplication, division, unary operators
// lowest tier - factor: integers, grouping with an expression inside
Node* parse_expr(TokenBuf* tb);
Node* parse_term(TokenBuf* tb);
Node* parse_factor(TokenBuf* tb);

AST* parse(TokenBuf* tb) {
    return parse_expr(tb);
}

// highest tier
Node* parse_expr(TokenBuf* tb) {
    Node* first = parse_term(tb);

    // eat add or subtract

    // grab second node

    // return combined node
}

// second highest
Node* parse_term(TokenBuf* tb) {
    // grab factor
    Node* first = parse_factor(tb);
    Token* next = peek(tb);

    // if we have unary operator
    if (next->type == CARET || next->type == )

    // if we have multiplication or division
}

// lowest tier
Node* parse_factor(TokenBuf* tb) {
    Token* next = peek(tb);

    // if we have an integer
    if (next->type == INT || next->type == IDENT) {
        return valnode(next);
    }

    // if we have a unary operator
    if (next->type)

    // if we have grouping
    if (next->type == LPAR || next->type == LBRAC || next->type == LCBRAC || next->type == PIPE) {
        advance(tb);
        Node* expr_as_fact = parse_expr(tb);
        eattok(tb, next->type);

        return expr_as_fact;
    }
}

// file stuff
int streq(char* one, char* two) {
    int len1;
    int len2;

    // grab length
    for (len1 = 0; one[len1] != '\0'; len1++);
    for (len2 = 0; two[len2] != '\0'; len2++);
    // strings cant be equal if the lengths arent
    if (len1 != len2) {
        return -1;
    }

    // for each character, check if equal
    for (int i = 0; i < len1; i++) {
        if (one[i] != two[i]) {
            return -1;
        }
    }

    return 1;
}

int main(int argc, char** argv) {

    FILE* in = NULL;
    FILE* out = NULL;

    typedef struct _ARGS {
        int o : 2;
        int i : 2;
        int args : 3;
        int usingstdin : 2;
        int usingstdout : 2;
    } ARGS;
    
    ARGS args = {0, 0, 0, 0, 0};
    
    // parse arguments
    for (int i = 0; i < argc; i++) {
        char* arg = argv[i];

        if (i == 0) continue;

        if (streq(arg, "-o") == 1) {
            // if we have alr gotten an output
            if (args.o > 0) {
                fprintf(stderr, "Error: too many outputs");
                exit(-1);
            }
            args.o = 1;
        }
        // if o == 1, then this is the output file
        else if (args.o == 1) {
            out = fopen(arg, "w");
            args.o = 2;
        } 

        // otherwise, this is the input file
        else {
            if (args.i > 0) {
                fprintf(stderr, "Error: too many inputs");
                exit(-1);
            }
            in = fopen(arg, "r");
            args.i++;
        }
    }

    // if we have no input
    if (args.i == 0) {
        // use stdin
        in = stdin;
        args.usingstdin = 1;
    }
    // if we have no output
    if (args.o == 0) {
        // use stdout
        out = stdout;
        args.usingstdout = 1;
    }

    // validate files
    if (!args.usingstdin && in == NULL) {
        fprintf(stderr, "Error: Invalid input file\n");
        exit(-1);
    }
    if (out == NULL) {
        fprintf(stderr, "Error: Invalid output file\n");
        exit(-1);
    }

    // actual run loop
    while (1) {
        LexIter* l;

        // if using file, read file into lexiter
        // otherwise prompt the user
        if (!args.usingstdin) {
            l = lex_iter("", 0);
            for (char c = fgetc(in); !feof(in); c = fgetc(in)) lex_iter_buf_append(l, &c, 1);
        } else {
            l = prompt("> ");

            // get rid of the newline
            l->len--;
        }

        TokenBuf* tb = lex(l);
        free_lex_iter(l);
        AST* ast = parse(tb);
        //free_token_buf(tb);
        // interpret ast
        // free ast
        // print output

        // test output
        print_token_buf_verbose(tb, out);
        fflush(out);

        // if we are taking input from user, keep looping
        // otherwise we have to exit
        if (!args.usingstdin) {
            exit(1);
        }
    }

    return 0;
}
