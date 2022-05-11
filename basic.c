#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

enum {
	STRING,
	SYMBOL,
	NEWLINE,
	KEYWORD,
	INTEGER,
	COLON,
	COMMA,
};

const char *keywords[] = {
	"IF",
	"THEN",
	"ELSE",
	"FOR",
	"NEXT",
	"=",
	"/",
	"(",
	")",
	"*",
	"+",
	"-",
	"GOSUB",
	"GOTO",
	"RETURN",
	"PRINT",
	"INPUT",
	0,
};

typedef struct token {
	int type;
	union {
		char *s;
		const char *cs;
		int i;
	} val;
} Token;

typedef struct variable {
	char *identifier;
	union {
		char *s;
		int i;
	} val;
} Variable;

typedef struct program {
	Token *tokens;
	int num_tokens;
	Variable *integers;
	int num_integers;
	Variable *strings;
	int num_strings;
	char *blank;
	char *last;
	int line;
	int do_else;
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

	/* don't touch strings >:( */
	if(t.type == STRING || t.type == SYMBOL) {
		char *s = malloc(strlen(t.val.s)+1);
		strcpy(s, t.val.s);
		t.val.s = s;
	}

	if(t.type == SYMBOL) {
		/* all caps for non-strings */
		for(char *c = t.val.s; *c; c++)
			if(*c >= 'a' && *c <= 'z')
				*c -= 'a' - 'A';

		/* check if string is a keyword */
		for(const char **kw = keywords; *kw; kw++) {
			if(strcmp(t.val.s, *kw) == 0) {
				free(t.val.s);
				t.val.cs = *kw;
				t.type = KEYWORD;
				break;
			}
		}

		/* convert integers */
		bool is_i = t.type != KEYWORD;
		int n = 0;
		for(char *c = t.val.s; *c != 0 && is_i; c++) {
			if(*c <  '0' || *c > '9')
				is_i = false;
			else
				n = n*10 + *c - '0';
		}
		if(is_i) {
			free(t.val.s);
			t.val.i = n;
			t.type = INTEGER;
		}
		/* seperators */
		else if(strcmp(t.val.s, ":") == 0) {
			free(t.val.s);
			t.type = COLON;
		}
		else if(strcmp(t.val.s, ",") == 0) {
			free(t.val.s);
			t.type = COMMA;
		}
	}

	p->tokens[p->num_tokens-1] = t;
}

Program *newProgram() {
	Program *p = malloc(sizeof(Program));
	*p = (Program){0, 0, 0, 0, 0, 0, 0};
	p->blank = malloc(1);
	p->blank[0] = 0;
	p->last = 0;
	p->do_else = -1;
	return p;
}

void freeToken(Token t) {
	if(t.type == STRING || t.type == SYMBOL)
		free(t.val.s);
}

void freeProgram(Program *p) {
	if(p->blank)
		free(p->blank);

	for(int i = 0; i < p->num_tokens; i++)
		freeToken(p->tokens[i]);
	if(p->tokens)
		free(p->tokens);

	for(int i = 0; i < p->num_strings; i++) {
		free(p->strings[i].identifier);
		free(p->strings[i].val.s);
	}
	if(p->strings)
		free(p->strings);

	for(int i = 0; i < p->num_integers; i++)
		free(p->integers[i].identifier);
	if(p->integers)
		free(p->integers);

	free(p);
}

void setStringVariable(Program *p, char *identifier, char *s) {
	for(int i = 0; i < p->num_strings; i++)
		if(strcmp(p->strings[i].identifier, identifier) == 0) {
			free(p->strings[i].val.s);
			p->strings[i].val.s = malloc(strlen(s)+1);
			strcpy(p->strings[i].val.s, s);
			return;
		}

	p->strings = realloc(p->strings, sizeof(Variable)*(++(p->num_strings)));
	Variable *v = &p->strings[p->num_strings-1];
	v->identifier = malloc(strlen(identifier)+1);
	strcpy(v->identifier, identifier);
	v->val.s = malloc(strlen(s)+1);
	strcpy(v->val.s, s);
}

char *getStringVariable(Program *p, char *identifier) {
	for(int i = 0; i < p->num_strings; i++)
		if(strcmp(p->strings[i].identifier, identifier) == 0)
			return p->strings[i].val.s;
	return p->blank;
}

void setIntegerVariable(Program *p, char *identifier, int d) {
	for(int i = 0; i < p->num_integers; i++)
		if(strcmp(p->integers[i].identifier, identifier) == 0) {
			p->integers[i].val.i = d;
			return;
		}

	p->integers = realloc(p->integers, sizeof(Variable)*(++(p->num_integers)));
	Variable *v = &p->integers[p->num_integers-1];
	v->identifier = malloc(strlen(identifier)+1);
	strcpy(v->identifier, identifier);
	v->val.i = d;
}

int getIntegerVariable(Program *p, char *identifier) {
	for(int i = 0; i < p->num_integers; i++)
		if(strcmp(p->integers[i].identifier, identifier) == 0)
			return p->integers[i].val.i;
	return 0;
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

char *getString() {
	printf("?");
	int max = 30;
	char *s = malloc(max);
	int len = 0;
	for(scanf("%c", &s[len++]); s[len-1] != '\n'; scanf("%c", &s[len++]))
		if(len > max-10) {
			max += 20;
			s = realloc(s, max);
		}
	s[len-1] = 0;
	return s;
}

void loadFile(Program *p, const char *filename) {
	FILE *fp = fopen(filename, "r");
	if(!fp) {
		printf("failed to open %s\n", filename);
		freeProgram(p);
		exit(1);
	}

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

void printDebug(Token t) {
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
	case COLON:
		printf(": ");
		break;
	case COMMA:
		printf(", ");
		break;
	}
}

void printToken(Token t) {
	switch(t.type) {
	case NEWLINE:
		printf("\n");
		break;
	case STRING:
		printf("%s", t.val.s);
		break;
	case INTEGER:
		printf("%d", t.val.i);
		break;
	}
}

void printProgram(Program *p) {
	for(int i = 0; i < p->num_tokens; i++) {
		printDebug(p->tokens[i]);
	}
	printf("\n");
}

void syntaxError(Program *p) {
	printf("SYNTAX ERROR AT LINE %d\n", p->line);
	freeProgram(p);
	exit(1);
}

void syntaxAssert(Program *p, bool cond) {
	if(!cond)
		syntaxError(p);
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
			*operators = realloc(*operators,
					sizeof(Token)*(*num_operators));

			(*operators)[(*num_operators)-1] = tokens[i];

			if(tokens[i-1].type != KEYWORD) {
				*num_operands += 1;
				*operands = realloc(*operands,
						sizeof(Token)*(*num_operands));
				(*operands)[(*num_operands)-1] = tokens[i-1];
			}
			if(tokens[i+1].type != KEYWORD) {
				*num_operands += 1;
				*operands = realloc(*operands,
						sizeof(Token)*(*num_operands));
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
	checkOperator(tokens, n, "/", operands, num_operands,
			operators, num_operators);
	checkOperator(tokens, n, "*", operands, num_operands,
			operators, num_operators);
	checkOperator(tokens, n, "+", operands, num_operands,
			operators, num_operators);
	checkOperator(tokens, n, "-", operands, num_operands,
			operators, num_operators);
	checkOperator(tokens, n, "=", operands, num_operands,
			operators, num_operators);
}

Token doOp(Program *p, Token t1, Token t2, const char *s) {
	Token t;
	t.type = INTEGER;

	syntaxAssert(p, t1.type == INTEGER || t1.type == STRING);
	syntaxAssert(p, t2.type == INTEGER || t2.type == STRING);

	if(strcmp(s, "=") == 0) {
		if(t1.type == STRING && t2.type == INTEGER) {
			t1.type = INTEGER;
			t1.val.i = strlen(t1.val.s);
		}
		if(t1.type == INTEGER && t2.type == STRING) {
			t2.type = INTEGER;
			t2.val.i = strlen(t2.val.s);
		}

		if(t1.type == STRING)
			t.val.i = (strcmp(t1.val.s, t2.val.s) == 0);
		else
			t.val.i = (t1.val.i == t2.val.i);

		return t;
	}

	if(t1.type == STRING) {
		t1.type = INTEGER;
		t1.val.i = strlen(t1.val.s);
	}
	if(t2.type == STRING) {
		t2.type = INTEGER;
		t2.val.i = strlen(t2.val.s);
	}

	syntaxAssert(p, t1.type == INTEGER && t2.type == INTEGER);

	if(strcmp(s, "+") == 0)
		t.val.i = t1.val.i + t2.val.i;
	else if(strcmp(s, "-") == 0)
		t.val.i = t1.val.i - t2.val.i;
	else if(strcmp(s, "/") == 0)
		t.val.i = t1.val.i / t2.val.i;
	else if(strcmp(s, "*") == 0)
		t.val.i = t1.val.i * t2.val.i;
	else {
		printf("UNKNOWN OPERATOR\n");
		syntaxError(p);
	}

	return t;
}

void getVariables(Program *p, Token *tokens, int n) {
	for(int i = 0; i < n; i++)
		if(tokens[i].type == SYMBOL) {
			bool is_str = false;
			if(tokens[i].val.s[0] != 0)
				if(tokens[i].val.s[strlen(tokens[i].val.s)-1]
						== '$')
					is_str = true;
			if(is_str) {
				tokens[i].val.s = getStringVariable(p,
						tokens[i].val.s);
				tokens[i].type = STRING;
			}
			else {
				tokens[i].val.i = getIntegerVariable(p,
						tokens[i].val.s);
				tokens[i].type = INTEGER;
			}
		}
}

Token evalExpression(Program *p, Token *otokens, int n) {
	Token *tokens = malloc(sizeof(Token)*n);
	for(int i = 0; i < n; i++)
		tokens[i] = otokens[i];

	getVariables(p, tokens, n);

	/* if only 1 part, return */
	if(n == 1) {
		Token r = *tokens;
		free(tokens);
		return r;
	}

	/* convert to rpn in two seperate arrays */

	int depth = 0, bopen_max = 20;
	int *bopen = malloc(sizeof(int)*bopen_max);

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
			checkOperators(tkns, n, &operands, &num_operands,
					&operators, &num_operators);
		}

		if(depth > bopen_max-10) {
			bopen_max += 20;
			bopen = realloc(bopen, sizeof(int)*bopen_max);
		}
	}
	checkOperators(tokens, n, &operands, &num_operands,
			&operators, &num_operators);

	/* we have operators in rpn, apply operations */

	for(int i = 0; i < num_operators; i++) {
		operands[0] = doOp(p, operands[0], operands[1],
				operators[i].val.cs);
		num_operands--;
		for(int j = 1; j < num_operands; j++)
			operands[j] = operands[j+1];
	}

	Token r = operands[0];

	free(bopen);
	free(tokens);
	free(operands);
	free(operators);

	return r;
}

void runLine(Program *p, Token *tokens, int n) {
	if(tokens[0].type == KEYWORD)
		if(strcmp(tokens[0].val.cs, "ELSE") == 0) {
			syntaxAssert(p, p->do_else != -1);
			if(p->do_else) {
				tokens++;
				n--;
			}
			else
				return;
		}

	/* multiple statements on one line */
	int multi = 0;
	int mn;

	for(int i = 0; i < n && !multi; i++)
		if(tokens[i].type == COLON)
			multi = i;

	if(multi) {
		mn = n-multi-1;
		n = multi;
		multi++;
	}

	int inp = 0;
	for(int i = 0; i < n; i++)
		if(tokens[i].type == KEYWORD)
			if(strcmp(tokens[i].val.cs, "INPUT") == 0)
				if(inp++)
					syntaxError(p);

	/* variable assignments */
	if(tokens[0].type == SYMBOL) {
		syntaxAssert(p, n >= 3);
		syntaxAssert(p, tokens[1].type == KEYWORD);
		syntaxAssert(p, strcmp(tokens[1].val.cs, "=") == 0);

		Token t;
		t.type = SYMBOL;
		if(tokens[2].type == KEYWORD)
			if(strcmp(tokens[2].val.cs, "INPUT") == 0) {
				if(n > 3) {
					printToken(evalExpression(p, tokens+3, n-3));
					printf("\n");
				}

				t.val.s = getString();
				t.type = STRING;
			}
		if(t.type == SYMBOL)
			t = evalExpression(p, tokens+2, n-2);

		if(tokens[0].val.s[strlen(tokens[0].val.s)-1] == '$') {
			syntaxAssert(p, t.type == STRING);
			setStringVariable(p, tokens[0].val.s, t.val.s);
		}
		else {
			syntaxAssert(p, t.type == INTEGER);
			setIntegerVariable(p, tokens[0].val.s, t.val.i);
		}

		if(p->last) {
			free(p->last);
			p->last = 0;
		}
		if(multi)
			runLine(p, tokens+multi, mn);
		return;
	}

	if(tokens[0].type != KEYWORD)
		syntaxError(p);

	if(strcmp(tokens[0].val.cs, "IF") == 0) {
		int found = 0;
		for(int i = 0; i < n && !found; i++)
			if(tokens[i].type == KEYWORD)
				if(strcmp(tokens[i].val.cs, "THEN") == 0)
					found = i;
		if(!found) {
			printf("EXPECT THEN AFTER IF\n");
			syntaxError(p);
		}
		Token t = evalExpression(p, tokens+1, found-1);
		syntaxAssert(p, t.type == INTEGER);
		if(t.val.i == 0) {
			if(p->last) {
				free(p->last);
				p->last = 0;
			}
			p->do_else = 1;

			return;
		}
		else {
			p->do_else = 0;

			tokens += found+1;
			n -= found+1;
			if(multi)
				multi -= found+1;
		}
	}

	if(strcmp(tokens[0].val.cs, "PRINT") == 0) {
		int c = 1;
		while(c < n) {
			int oc = c;
			for(int i = c; i < n && c == oc; i++)
				if(tokens[i].type == COMMA)
					c = i;
			if(c == oc)
				c = n;
			printToken(evalExpression(p, tokens+oc, c-oc));
			c++;
		}
		printf("\n");
	}
	else if(strcmp(tokens[0].val.cs, "INPUT") == 0) {
		if(n > 1) {
			Token t = evalExpression(p, tokens+1, n-1);
			printToken(t);
			printf("\n");
		}
		free(getString());
		p->last = 0;
	}

	/* skip this for if false */
	if(p->last) {
		free(p->last);
		p->last = 0;
	}
	if(multi)
		runLine(p, tokens+multi, mn);
}

void runProgram(Program *p) {
	p->line = 0;
	for(int i = 0; i < p->num_tokens; i++) {
		p->line += 1;
		int l = lineLength(p, i);

		if(p->tokens[i].type == KEYWORD) {
			if(strcmp(p->tokens[i].val.cs, "IF") != 0 &&
					strcmp(p->tokens[i].val.cs, "ELSE"))
				p->do_else = -1;
		}
		else
			p->do_else = -1;

		runLine(p, p->tokens+i, l);
		i += l;
	}
}

int main(int argc, char **args) {
	if(argc <= 1) {
		printf("BASIC Interpreter - tdwsl 2022\n");
		printf("usage: %s <file>\n", args[0]);
		return 0;
	}
	if(argc > 2) {
		printf("too many arguments\n");
		return 1;
	}

	Program *p = newProgram();
	loadFile(p, args[1]);
	/*printProgram(p);*/
	runProgram(p);
	freeProgram(p);
	return 0;
}
