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

void runProgram(Program *p) {
	for(int i = 0; i < p->num_tokens; i++) {
	}
}

int main() {
	Program *p = newProgram();
	loadFile(p, "test.bas");
	printProgram(p);
	freeProgram(p);
	return 0;
}
