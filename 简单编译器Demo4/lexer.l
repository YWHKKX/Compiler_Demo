%{
#include "parser.tab.h"
#include "string.h"
#include "def.h"
int yycolumn=1;
#define YY_USER_ACTION    	yylloc.first_line=yylloc.last_line=yylineno; \
	yylloc.first_column=yycolumn;	yylloc.last_column=yycolumn+yyleng-1; yycolumn+=yyleng;
typedef union {
	int type_int;
	float type_float;
	char type_char[3];
	char type_string[31];
	char type_id[32];
	struct node *ptr;
} YYLVAL;
#define YYSTYPE YYLVAL

%}
%option yylineno

id   [A-Za-z][A-Za-z0-9]*
int    [0-9]+
float  ([0-9]*\.[0-9]+)|([0-9]+\.)
char   ('[A-Za-z0-9]')
string (\"[A-Za-z0-9]*\")

%%
{int}        {yylval.type_int=atoi(yytext);/*printf("type_int:%d\n",yytext);*/return INT;}
{float}      {yylval.type_float=atof(yytext);/*printf("type_float:%f\n",yytext);*/return FLOAT;}
{char}        {strcpy(yylval.type_char,yytext);/*printf("type_char:%s\n",yytext);*/return CHAR;}
{string}      {strcpy(yylval.type_string,yytext);/*printf("type_string:%s\n",yytext);*/return STRING;}

"int"        {strcpy(yylval.type_id,  yytext);/*printf("(TYPE,int)");*/return TYPE;}
"float"      {strcpy(yylval.type_id,  yytext);/*printf("(TYPE,float)");*/return TYPE;}
"char"        {strcpy(yylval.type_id,  yytext);/*printf("(TYPE,char)");*/return TYPE;}
"string"      {strcpy(yylval.type_id,  yytext);/*printf("(TYPE,string)");*/return TYPE;}
"struct"     {/*printf("(STRUCT,struct)");*/return STRUCT;}

"return"     {/*printf("(RETURN,return)");*/return RETURN;}
"if"         {/*printf("(IF,if)");*/return IF;}
"else"       {/*printf("(ELSE,else)");*/return ELSE;}
"while"      {/*printf("(WHILE),while");*/return WHILE;}
"for"        {/*printf("(FOR,for)");*/return FOR;}

{id}       {strcpy(yylval.type_id, yytext);/*printf("(ID,%s)",yylval.type_id);*/return ID;}
";"		{/*printf("(SEMI,;)");*/return SEMI;}
","		{/*printf("(COMMA,,)");*/return COMMA;}
">"|"<"|">="|"<="|"=="|"!=" {strcpy(yylval.type_id, yytext);/*printf("(RELOP,%s)",yylval.type_id);*/return RELOP;}
"="		{/*printf("(ASSIGNOP,=)");*/return ASSIGNOP;}
"+"		{/*printf("(PLUS,+)");*/return PLUS;}
"-"		{/*printf("(MINUS,-)");*/return MINUS;}
"*"		{/*printf("(STAR,*)");*/return STAR;}
"/"		{/*printf("(DIV,/)");*/return DIV;}
"&&"        {/*printf("(AND,&&)");*/return AND;}
"&"		{/*printf("(ADDR,&)");*/return ADDR;}
"||"        {/*printf("(OR,||)");*/return OR;}
"."         {/*printf("(DOT,.)");*/return DOT;}
"!"		{/*printf("(NOT,!)");*/return NOT;}
"("		{/*printf("(LP,()");*/return LP;}
")"		{/*printf("(RP,))");*/return RP;}
"["         {/*printf("(LB,[)");*/return LB;}
"]"         {/*printf("(RB,])");*/return RB;}
"{"		{/*printf("(LC,{)");*/return LC;}
"}"		{/*printf("(RC,})\n");*/return RC;}
[\n]        	{yycolumn=1;}
[ \r\t]     	{}
"//"[^\n]*  	{}
"/*"([^\*]|(\*)*[^\*/])*(\*)*"*/" {}
.		{printf("\n==>ERROR:Mysterious character \"%s\" at Line %d\n",yytext,yylineno);}
%%


int yywrap()
{
	return 1;
}
