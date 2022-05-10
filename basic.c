#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

enum {
	STRING,
	SYMBOL,
	NEWLINE,
	KEYWORD,
	INTEGER,
};

const char *keywords[] = {
	"IF",
	"THEN",
	"ELSE,",
	"FOR",
	"NEXT",
	"=",
	"/",
	"(",
	")",
	"*",
	"+",
	"-",
	":",
	",",
	"GOSUB",
	"RETURN",
	"PRINT",
	"INPUT",
	0,
};

typedef struct token {
	int type;
	union val {
		char *s;
		const char *cs;
		int i;
	} val;
} Token;

typedef struct program {
	Token *tokens;
	int num_tokens;
} Program;

char *addChar(char *s, int *len, int *max, char c) {
	s[(*len)++] = c;
	s[*len] = 0;
	if(*len > (*max)-10) {
		*max += 20;
		s = realloc(s, *max);
	}
	return s;
}

void addToken(Program *p, Token t) {
	p->tokens = realloc(p->tokens, (++(p->num_tokens))*sizeof(Token));

	if(t.type == STRING || t.type == SYMBOL) {
		char *s = malloc(strlen(t.val.s)+1);
		strcpy(s, t.val.s);
		t.val.s = s;
	}

	if(t.type == SYMBOL) {
		for(char *c = t.val.s; *c; c++)
			if(*c >= 'a' && *c <= 'z')
				*c -= 'a' - 'A';

		for(const char **kw = keywords; *kw; kw++) {
			if(strcmp(t.val.s, *kw) == 0) {
				free(t.val.s);
				t.val.cs = *kw;
				t.type = KEYWORD;
				break;
			}
		}

		/*bool is_i = t.type != KEYWORD;
		for(char *c = t.val.s; *c != 0 && is_i; c++)
			if(*c <  '0' || *c > '9')
				is_i = false;
		if(is_i) {
			t.val.i = atoi(t.val.s);
			free(t.val.s);
			t.type = INTEGER;
		}*/
	}

	p->tokens[p->num_tokens-1] = t;
}

Program *newProgram() {
	Program *p = malloc(sizeof(Program));
	*p = (Program){0, 0};
	return p;
}

void freeToken(Token t) {
	if(t.type == STRING || t.type == SYMBOL)
		free(t.val.s);
}

void freeProgram(Program *p) {
	for(int i = 0; i < p->num_tokens; i++)
		freeToken(p->tokens[i]);
	free(p->tokens);
	free(p);
}

void loadString(Program *p, char *text) {
	int max = 20;
	char *s = malloc(max);
	int len = 0;

	Token t;
	bool quote = false;

	const char *schars = "+-/*(),:";

	for(char *c = text; *c; c++) {
		if(*c == '\n') {
			if(len) {
				t.val.s = s;
				t.type = (quote) ? STRING : SYMBOL;
				addToken(p, t);
				len = 0;
			}
			quote = false;
			t.type = NEWLINE;
			addToken(p, t);
			continue;
		}

		if(quote) {
			if(*c == '"') {
				t.val.s[len] = 0;
				t.val.s = s;
				t.type = STRING;
				addToken(p, t);
				len = 0;
				quote = false;
			}
			else
				s = addChar(s, &len, &max, *c);
			continue;
		}

		if(*c == '"') {
			if(len) {
				t.val.s = s;
				t.type = SYMBOL;
				addToken(p, t);
				len = 0;
			}
			quote = true;
			continue;
		}

		bool spec = false;
		for(const char *h = schars; *h && !spec; h++)
			if(*c == *h)
				spec = true;
		if(spec) {
			if(len) {
				t.val.s = s;
				t.type = SYMBOL;
				addToken(p, t);
				len = 0;
			}
			s[0] = *c;
			s[1] = 0;
			t.type = SYMBOL;
			t.val.s = s;
			addToken(p, t);
			continue;
		}

		if(*c == ' ' || *c == '\t') {
			if(len) {
				t.type = SYMBOL;
				t.val.s = s;
				addToken(p, t);
				len = 0;
			}
			continue;
		}

		s = addChar(s, &len, &max, *c);
	}

	if(len) {
		t.val.s = s;
		t.type = SYMBOL;
		addToken(p, t);
	}

	free(s);
}

void loadFile(Program *p, const char *filename) {
	FILE *fp = fopen(filename, "r");
	assert(fp != NULL);

	int len = 0;
	int max = 200;
	char *s = malloc(max);
	for(char c = fgetc(fp); !feof(fp); c = fgetc(fp)) {
		s[len++] = c;
		s[len] = 0;
		if(len > max-10) {
			max += 20;
			s = realloc(s, max);
		}
	}
	fclose(fp);

	loadString(p, s);
	free(s);
}

void printProgram(Program *p) {
	for(int i = 0; i < p->num_tokens; i++) {
		Token t = p->tokens[i];
		switch(t.type) {
		case NEWLINE:
			printf("\n");
			break;
		case STRING:
			printf("\"%s\" ", t.val.s);
			break;
		case SYMBOL:
			printf("%s ", t.val.s);
			break;
		case KEYWORD:
			printf("%s ", t.val.cs);
			break;
		case INTEGER:
			printf("%d ", t.val.i);
			break;
		}
	}
	printf("\n");
}

int lineLength(Program *p, int d) {
	for(int i = d; i < p->num_tokens; i++)
		if(p->tokens[i].type == NEWLINE)
			return (i-d);
	return (p->num_tokens-d);
}

void checkOperator(Token *tokens, int n, const char *s,
		Token **operands, int *num_operands,
		Token **operators, int *num_operators)
{
	int depth = 0;

	for(int i = 0; i < n; i++) {
		if(tokens[i].type != KEYWORD)
			continue;

		if(strcmp(tokens[i].val.cs, "(") == 0)
			depth++;
		else if(strcmp(tokens[i].val.cs, ")") == 0)
			depth--;
		if(depth)
			continue;

		if(strcmp(tokens[i].val.s, s) == 0) {
			*num_operators += 1;
			*operators = realloc(*operators, sizeof(Token)*(*num_operators));

			(*operators)[(*num_operators)-1] = tokens[i];

			if(tokens[i-1].type != KEYWORD) {
				*num_operands += 1;
				*operands = realloc(*operands, sizeof(Token)*(*num_operands));
				(*operands)[(*num_operands)-1] = tokens[i-1];
			}
			if(tokens[i+1].type != KEYWORD) {
				*num_operands += 1;
				*operands = realloc(*operands, sizeof(Token)*(*num_operands));
				(*operands)[(*num_operands)-1] = tokens[i+1];
			}

			Token t;
			t.type = KEYWORD;
			t.val.cs = "(";
			tokens[i-1] = t;
			t.val.cs = ")";
			tokens[i+1] = t;
		}
	}
}

void checkOperators(Token *tokens, int n,
		Token **operands, int *num_operands,
		Token **operators, int *num_operators)
{
	checkOperator(tokens, n, "=", operands, num_operands, operators, num_operators);
	checkOperator(tokens, n, "/", operands, num_operands, operators, num_operators);
	checkOperator(tokens, n, "*", operands, num_operands, operators, num_operators);
	checkOperator(tokens, n, "+", operands, num_operands, operators, num_operators);
	checkOperator(tokens, n, "-", operands, num_operands, operators, num_operators);
}

Token evalExpression(Token *otokens, int n) {
	int depth = 0, bopen_max = 20;
	int *bopen = malloc(sizeof(int)*bopen_max);

	Token *tokens = malloc(sizeof(Token)*n);
	for(int i = 0; i < n; i++)
		tokens[i] = otokens[i];

	Token *operands = 0;
	int num_operands = 0;
	Token *operators = 0;
	int num_operators = 0;

	for(int i = 0; i < n; i++) {
		if(tokens[i].type != KEYWORD)
			continue;

		if(strcmp(tokens[i].val.cs, "(") == 0)
			bopen[depth++] = i;
		else if(strcmp(tokens[i].val.cs, ")") == 0) {
			int open = bopen[--depth];
			Token *tkns = tokens+open+1;
			int n = i-open-1;
			checkOperators(tkns, n, &operands, &num_operands, &operators, &num_operators);
		}

		if(depth > bopen_max-10) {
			bopen_max += 20;
			bopen = realloc(bopen, sizeof(int)*bopen_max);
		}
	}
	checkOperators(tokens, n, &operands, &num_operands, &operators, &num_operators);

	for(int i = 0; i < num_operands; i++)
		printf("%s ", operands[i].val.s);
	for(int i = 0; i < num_operators; i++)
		printf("%s ", operators[i].val.cs);

	exit(1);

	free(bopen);
	free(tokens);
	free(operands);
	free(operators);
}

void runLine(Program *p, Token *tokens, int n) {
	assert(tokens[0].type == KEYWORD);
	evalExpression(tokens+1, n-1);
}

void runProgram(Program *p) {
	for(int i = 0; i < p->num_tokens; i++) {
		int l = lineLength(p, i);
		runLine(p, p->tokens+i, l);
		i += l;
	}
}

int main() {
	Program *p = newProgram();
	/*loadFile(p, "test.bas");*/
	loadString(p, "print 10 * (5 - 2)");
	printProgram(p);
	runProgram(p);
	freeProgram(p);
	return 0;
}
