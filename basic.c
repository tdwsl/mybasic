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
	LABEL,
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
	"TO",
	"REM",
	"AND",
	"OR",
	"DIM",
	"EXIT",
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

typedef struct integerArray {
	char *identifier;
	int *integers;
	int num_integers;
} IntegerArray;

typedef struct forLoop {
	int i1, i2;
	char *s;
	int line;
} ForLoop;

typedef struct program {
	Token *tokens;
	int num_tokens;

	Variable *integers;
	int num_integers;
	Variable *strings;
	int num_strings;
	Variable *labels;
	int num_labels;
	IntegerArray *integerArrays;
	int num_integerArrays;

	char *blank;
	char *last;
	int line;
	int do_else;
	ForLoop *forLoops;
	int num_forLoops;
	int max_forLoops;
	int *returnLines;
	int num_returnLines;
	int max_returnLines;
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
		/* label */
		else if(t.val.s[strlen(t.val.s)-1] == ':') {
			t.val.s[strlen(t.val.s)-1] = 0;
			t.type = LABEL;
		}
	}

	p->tokens[p->num_tokens-1] = t;
}

Program *newProgram() {
	Program *p = malloc(sizeof(Program));
	*p = (Program){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	p->blank = malloc(1);
	p->blank[0] = 0;
	p->last = 0;
	p->do_else = -1;
	p->max_forLoops = 20;
	p->forLoops = malloc(p->max_forLoops*sizeof(ForLoop));
	p->num_forLoops = 0;
	p->max_returnLines = 20;
	p->returnLines = malloc(p->max_returnLines*sizeof(int));
	p->num_returnLines = 0;
	return p;
}

void freeToken(Token t) { /* something to do with gosub doesnt work... */
	if(t.type == STRING || t.type == LABEL /*|| t.type == SYMBOL*/)
		free(t.val.s);
}

void freeProgram(Program *p) {
	free(p->forLoops);
	free(p->returnLines);

	if(p->labels)
		free(p->labels);

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

	for(int i = 0; i < p->num_integerArrays; i++) {
		free(p->integerArrays[i].identifier);
		free(p->integerArrays[i].integers);
	}
	if(p->integerArrays)
		free(p->integerArrays);

	free(p);
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

	p->integers = realloc(p->integers, sizeof(Variable)*
			(++(p->num_integers)));
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

void dimIntegerArray(Program *p, char *identifier, int sz) {
	for(int i = 0; i < p->num_integerArrays; i++) {
		if(strcmp(p->integerArrays[i].identifier, identifier) == 0) {
			IntegerArray *a = &p->integerArrays[i];
			free(a->integers);
			a->integers = malloc(sizeof(int)*sz);
			a->num_integers = sz;
			for(int j = 0; j < sz; j++)
				a->integers[j] = 0;
			return;
		}
	}

	p->integerArrays = realloc(p->integerArrays,
			sizeof(IntegerArray)*(++(p->num_integerArrays)));
	IntegerArray *a = &p->integerArrays[p->num_integerArrays-1];
	a->integers = malloc(sizeof(int)*sz);
	a->num_integers = sz;
	for(int i = 0; i < sz; i++)
		a->integers[i] = 0;
	a->identifier = malloc(strlen(identifier)+1);
	strcpy(a->identifier, identifier);
}

int *pIntegerArrayVal(Program *p, char *identifier, int d) {
	IntegerArray *a = 0;
	for(int i = 0; i < p->num_integerArrays && !a; i++)
		if(strcmp(p->integerArrays[i].identifier, identifier)
				== 0)
			a = &p->integerArrays[i];
	if(!a) {
		printf("COULD NOT FIND %s\n", identifier);
		syntaxError(p);
	}
	if(d < 1 || d > a->num_integers) {
		printf("INVALID ARRAY INDEX %d\n", d);
		syntaxError(p);
	}
	return &a->integers[d-1];
}

void setIntegerArrayVal(Program *p, char *identifier, int d, int v) {
	*pIntegerArrayVal(p, identifier, d) = v;
}

int getIntegerArrayVal(Program *p, char *identifier, int d) {
	int n = *pIntegerArrayVal(p, identifier, d);
	return n;
}

void addLabel(Program *p, char *s, int line) {
	p->labels = realloc(p->labels, sizeof(Variable)*(++(p->num_labels)));
	Variable v;
	v.identifier = s;
	v.val.i = line;
	p->labels[p->num_labels-1] = v;
}

int getLabelLine(Program *p, char *s) {
	for(int i = 0; i < p->num_labels; i++)
		if(strcmp(p->labels[i].identifier, s) == 0)
			return p->labels[i].val.i;
	syntaxError(p);
}

void loadString(Program *p, char *text) {
	int max = 20;
	char *s = malloc(max);
	int len = 0;

	Token t;
	bool quote = false;

	const char *schars = "+-/*(),";

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

	if(p->tokens[p->num_tokens-1].type != NEWLINE) {
		Token t;
		t.type = NEWLINE;
		addToken(p, t);
	}

	free(s);

	/* collect labels */

	int line = 0;
	for(int i = 0; i < p->num_tokens; i++) {
		line++;
		if(p->tokens[i].type == LABEL)
			addLabel(p, p->tokens[i].val.s, line);
		int l = lineLength(p, i);
		i += l;
	}
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
		printf("[%s] ", t.val.cs);
		break;
	case INTEGER:
		printf("%d ", t.val.i);
		break;
	case COLON:
		printf(": ");
		break;
	case LABEL:
		printf("%s: ", t.val.s);
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
	checkOperator(tokens, n, "AND", operands, num_operands,
			operators, num_operators);
	checkOperator(tokens, n, "OR", operands, num_operands,
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
	else if(strcmp(s, "AND") == 0)
		t.val.i = t1.val.i & t2.val.i;
	else if(strcmp(s, "OR") == 0)
		t.val.i = t1.val.i | t2.val.i;
	else {
		printf("UNKNOWN OPERATOR\n");
		syntaxError(p);
	}

	return t;
}

Token *getVariables(Program *p, Token *tokens, int *n);

Token evalExpression(Program *p, Token *otokens, int n) {
	Token *tokens = malloc(sizeof(Token)*n);
	for(int i = 0; i < n; i++)
		tokens[i] = otokens[i];

	tokens = getVariables(p, tokens, &n);

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

	syntaxAssert(p, num_operands == 1);
	Token r = operands[0];

	free(bopen);
	free(tokens);
	free(operands);
	free(operators);

	return r;
}

Token *getVariables(Program *p, Token *tokens, int *n) {
	Token *ntokens = 0;
	int nn = 0;

	for(int i = 0; i < *n; i++) {
		Token t = tokens[i];

		if(tokens[i].type == SYMBOL) {
			bool is_arr = false;
			if(i+1 < *n)
			if(tokens[i+1].type == KEYWORD)
			if(strcmp(tokens[i+1].val.cs, "(") == 0)
				is_arr = true;

			if(is_arr) {
				int found = 0;
				for(int j = i+1; j < *n && !found; j++)
					if(tokens[j].type == KEYWORD)
					if(strcmp(tokens[j].val.cs, ")") == 0)
						found = j;
				int sz = found-i-2;
				syntaxAssert(p, sz > 0);
				Token d = evalExpression(p, tokens+i+2, sz);
				syntaxAssert(p, d.type == INTEGER);
				t.type = INTEGER;
				t.val.i = getIntegerArrayVal(p,
						tokens[i].val.s, d.val.i);
				i = found;
			}
			else {
				bool is_str = false;
				if(t.val.s[0] != 0)
					if(t.val.s[strlen(t.val.s)-1]
							== '$')
						is_str = true;
				if(is_str) {
					t.val.s = getStringVariable(p,
							t.val.s);
					t.type = STRING;
				}
				else {
					t.val.i = getIntegerVariable(p,
							t.val.s);
					t.type = INTEGER;
				}
			}
		}
		ntokens = realloc(ntokens, sizeof(Token)*(++nn));
		ntokens[nn-1] = t;
	}

	free(tokens);
	*n = nn;
	return ntokens;
}

void pushForLoop(Program *p, ForLoop l) {
	p->forLoops[p->num_forLoops++] = l;
	if(p->num_forLoops > p->max_forLoops-10) {
		p->max_forLoops += 20;
		p->forLoops = realloc(p->forLoops,
				p->max_forLoops*sizeof(ForLoop));
	}
}

ForLoop popForLoop(Program *p) {
	syntaxAssert(p, p->num_forLoops != 0);
	return p->forLoops[--(p->num_forLoops)];
}

void pushReturnLine(Program *p, int line) {
	p->returnLines[p->num_returnLines++] = line;
	if(p->num_returnLines > p->max_returnLines-10) {
		p->max_returnLines += 20;
		p->returnLines = realloc(p->returnLines,
				p->max_returnLines*sizeof(int));
	}
}

int popReturnLine(Program *p) {
	syntaxAssert(p, p->num_returnLines != 0);
	return p->returnLines[--(p->num_returnLines)];
}

/* returns 1 to jump to another line */
int runLine(Program *p, Token *tokens, int n) {
	if(n <= 0)
		return 0;

	if(tokens[0].type == KEYWORD) {
		if(strcmp(tokens[0].val.cs, "ELSE") == 0) {
			syntaxAssert(p, p->do_else != -1);
			if(p->do_else) {
				tokens++;
				n--;
			}
			else
				return 0;
		}
		else if(strcmp(tokens[0].val.cs, "REM") == 0)
			return 0;
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

		/* array variable */
		if(strcmp(tokens[1].val.cs, "(") == 0) {
			int found = 0;
			for(int i = 2; i < n && !found; i++)
				if(tokens[i].type == KEYWORD)
					if(strcmp(tokens[i].val.cs, ")")
							== 0)
						found = i;
			if(!found) {
				printf("EXPECTED CLOSING BRACE\n");
				syntaxError(p);
			}

			syntaxAssert(p, tokens[found+1].type == KEYWORD);
			syntaxAssert(p, strcmp(tokens[found+1].val.cs, "=")
					== 0);

			Token t1 = evalExpression(p, tokens+2, found-2);
			Token t2 = evalExpression(p,
					tokens+found+2, n-found-2);
			syntaxAssert(p, t1.type == INTEGER
					&& t2.type == INTEGER);

			setIntegerArrayVal(p, tokens[0].val.s,
					t1.val.i, t2.val.i);

			if(p->last) {
				free(p->last);
				p->last = 0;
			}
			if(multi)
				runLine(p, tokens+multi, mn);
			return 0;
		}

		/* non-array variable */
		syntaxAssert(p, strcmp(tokens[1].val.cs, "=") == 0);

		Token t;
		t.type = SYMBOL;
		if(tokens[2].type == KEYWORD)
			if(strcmp(tokens[2].val.cs, "INPUT") == 0) {
				if(n > 3) {
					printToken(evalExpression(p, tokens+3,
								n-3));
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
		return 0;
	}

	if(tokens[0].type == LABEL) {
		syntaxAssert(p, n == 1);
		return 0;
	}

	syntaxAssert(p, tokens[0].type == KEYWORD);

	/* keywords */

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

			return 0;
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
	else if(strcmp(tokens[0].val.cs, "FOR") == 0) {
		syntaxAssert(p, n >= 6);

		int found = 0;
		for(int i = 0; i < n && !found; i++)
			if(tokens[i].type == KEYWORD)
				if(strcmp(tokens[i].val.cs, "TO") == 0)
					found = i;
		if(!found) {
			printf("EXPECT TO AFTER FOR\n");
			syntaxError(p);
		}

		syntaxAssert(p, tokens[1].type == SYMBOL);
		syntaxAssert(p, tokens[1].val.s[strlen(tokens[1].val.s)-1]
				!= '$');
		syntaxAssert(p, tokens[2].type == KEYWORD);
		syntaxAssert(p, strcmp(tokens[2].val.cs, "=") == 0);

		Token t1 = evalExpression(p, tokens+3, found-3);
		Token t2 = evalExpression(p, tokens+found+1, n-found-1);
		syntaxAssert(p, t1.type == INTEGER && t2.type == INTEGER);

		ForLoop f = (ForLoop) {
			t1.val.i, t2.val.i, tokens[1].val.s, p->line,
		};
		pushForLoop(p, f);

		setIntegerVariable(p, tokens[1].val.s, t1.val.i);
	}
	else if(strcmp(tokens[0].val.cs, "NEXT") == 0) {
		syntaxAssert(p, n == 1);
		ForLoop f = popForLoop(p);

		int i = getIntegerVariable(p, f.s);
		bool g = false;
		if(f.i1 < f.i2) {
			i++;
			if(i > f.i2)
				g = true;
		}
		else if(f.i1 > f.i2) {
			i--;
			if(i < f.i2)
				g = true;
		}
		setIntegerVariable(p, f.s, i);

		if(!g) {
			pushForLoop(p, f);
			p->line = f.line;
			if(p->last)
				free(p->last);
			return 1;
		}
	}
	else if(strcmp(tokens[0].val.cs, "GOTO") == 0) {
		syntaxAssert(p, n == 2);
		syntaxAssert(p, tokens[1].type == SYMBOL);
		p->line = getLabelLine(p, tokens[1].val.s);
		return 1;
	}
	else if(strcmp(tokens[0].val.cs, "GOSUB") == 0) {
		syntaxAssert(p, n == 2);
		syntaxAssert(p, tokens[1].type == SYMBOL);
		pushReturnLine(p, p->line);
		p->line = getLabelLine(p, tokens[1].val.s);
		return 1;
	}
	else if(strcmp(tokens[0].val.cs, "RETURN") == 0) {
		syntaxAssert(p, n == 1);
		p->line = popReturnLine(p);
		return 1;
	}
	else if(strcmp(tokens[0].val.cs, "DIM") == 0) {
		syntaxAssert(p, n >= 5);
		syntaxAssert(p, tokens[1].type == SYMBOL);
		syntaxAssert(p, tokens[2].type == KEYWORD);
		syntaxAssert(p, tokens[n-1].type == KEYWORD);
		syntaxAssert(p, strcmp(tokens[2].val.s, "(") == 0);
		syntaxAssert(p, strcmp(tokens[n-1].val.s, ")") == 0);
		Token t = evalExpression(p, tokens+3, n-4);
		syntaxAssert(p, t.type == INTEGER);
		if(t.val.i <= 0) {
			printf("ARRAY SIZE MUST BE > 0\n");
			syntaxError(p);
		}
		dimIntegerArray(p, tokens[1].val.s, t.val.i);
		return 0;
	}
	else if(strcmp(tokens[0].val.cs, "EXIT") == 0) {
		freeProgram(p);
		exit(0);
	}

	/* skip this for if false */
	if(p->last) {
		free(p->last);
		p->last = 0;
	}
	if(multi)
		runLine(p, tokens+multi, mn);

	return 0;
}

void runProgram(Program *p) {
	p->line = 0;
	for(int i = 0; i < p->num_tokens; i++) {
		p->line++;
		int l = lineLength(p, i);

		if(p->tokens[i].type == KEYWORD) {
			if(strcmp(p->tokens[i].val.cs, "IF") != 0 &&
					strcmp(p->tokens[i].val.cs, "ELSE"))
				p->do_else = -1;
		}
		else
			p->do_else = -1;

		if(runLine(p, p->tokens+i, l) == 0)
			i += l;
		else {
			int line = 0;
			for(i = 0; i < p->num_tokens && line < p->line; i++) {
				line++;
				int l = lineLength(p, i);
				i += l;
			}
			i--;
		}
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
