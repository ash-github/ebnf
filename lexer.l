%x XQUOTE
%x XINLINE
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
<XINLINE>{
	\{	{ assert(qlen < MAX_QUOTE); qtext[qlen++] = '{'; qlevel++; }
	\}	{ if (!--qlevel) { qtext[qlen] = 0; BEGIN(INITIAL); return qsym; } assert(qlen < MAX_QUOTE); qtext[qlen++] = '}'; }
	[^{}]*	{ assert(qlen+yyleng <= MAX_QUOTE); memcpy(qtext+qlen, yytext, yyleng); qlen += yyleng; }
}
::=	return IS;
↑	return OUT;
↓	return IN;
'.'	{ tokval = ((unsigned char *)yytext)[1]; return CHAR; }
{ID}	return IDENT;
@{ID}	return NAMED_ACTION;
[ \t]+
\<	{ BEGIN(XQUOTE); qlen = 0; qlevel = 1; }
@\{	{ BEGIN(XINLINE);qlen = 0; qlevel = 1; qsym = INLINE_ACTION; }
\$\{	{ BEGIN(XINLINE);qlen = 0; qlevel = 1; qsym = ATTACHED_ACTION; }
\n	return '\n';
.	return *yytext;
