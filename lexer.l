%x XQUOTE
%option noyywrap nodefault yylineno
ID	[a-zA-Z_][a-zA-Z_0-9]*
%{
#include <assert.h>
#include "tokens.h"
int tokval;
static int qlevel;
static int qsym;
#define MAX_QUOTE 255
char qtext[MAX_QUOTE+1];
int qlen;
%}
%%
<XQUOTE>{
	\<	{ assert(qlen < MAX_QUOTE); qtext[qlen++] = '<'; qlevel++; }
	\>	{ if (!--qlevel) { qtext[qlen] = 0; BEGIN(INITIAL); return QUOTE; } assert(qlen < MAX_QUOTE); qtext[qlen++] = '>'; }
	[^<>]*	{ assert(qlen+yyleng <= MAX_QUOTE); memcpy(qtext+qlen, yytext, yyleng); qlen += yyleng; }
}
::=	return IS;
'.'	{ tokval = ((unsigned char *)yytext)[1]; return CHAR; }
{ID}	return IDENT;
[ \t]+
\<	{ BEGIN(XQUOTE); qlen = 0; qlevel = 1; }
\n	return '\n';
.	return *yytext;
