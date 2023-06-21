## 基础信息

当我做完编译原理的4个 Lab 之后，感觉对编译原理各个步骤的具体细节都不太熟悉，于是我打算将编译原理 Lab 中的代码整理完善，搞出一个完整的编译器来

本代码是在以下项目的基础上进行完善：

+ [GitHub - Billy1900/compiler-course-project: 华中科技大学编译原理实验2019](https://github.com/Billy1900/compiler-course-project)

按照自己的理解对部分函数进行了重写和补充，修改了一些 BUG（本人是第一次尝试这么复杂的程序，可能有些代码有点绕，请多多见谅）

## 词法分析

原项目对词法分析的部分处理地很好，我这里没有做太多修改：

```
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
```

+ 都是一些基础的正则表达式，不需要过多赘述了

## 语法分析

语法分析部分同样也没有进行太多修改：

```
%define parse.error verbose
%locations
%{
#include "stdio.h"
#include "math.h"
#include "string.h"
#include "def.h"
extern int yylineno;
extern char *yytext;
extern FILE *yyin;
void yyerror(const char* fmt, ...);
void display(struct node *,int);
%}

%union {
    int    type_int;
    float  type_float;
    char type_char[3];
    char type_string[31];
    char   type_id[32];
    struct node *ptr;
};

%type  <ptr> program ExtDefList ExtDef Specifier StructSpecifier OptTag Tag ExtDecList FuncDec CompSt VarList VarDec ParamDec Stmt ForDec StmList DefList Def DecList Dec Exp Args

%token <type_int> INT              
%token <type_id> ID RELOP TYPE  
%token <type_float> FLOAT       
%token <type_char> CHAR       
%token <type_string> STRING    

%token STRUCT LP RP LB RB LC RC SEMI COMMA DOT  
%token PPLUS MMINUS PLUS MINUS STAR DIV ASSIGNOP MINUSASSIGNOP PLUSASSIGNOP DIVASSIGNOP STARASSIGNOP AND OR NOT IF ELSE WHILE FOR RETURN

%right ASSIGNOP MINUSASSIGNOP PLUSASSIGNOP DIVASSIGNOP STARASSIGNOP 
%left OR
%left AND
%left RELOP
%left PLUS MINUS
%left STAR DIV
%right UMINUS NOT PPLUS MMINUS
%right LB
%left RB
%left DOT
%nonassoc LOWER_THEN_ELSE
%nonassoc ELSE

%%

program: ExtDefList    {display($1,0);DisplaySymbolTable($1);}    
         ;
ExtDefList: {$$=NULL;}
          | ExtDef ExtDefList {$$=mknode(EXT_DEF_LIST,$1,$2,NULL,yylineno);}   
          ;
ExtDef:   Specifier ExtDecList SEMI   {$$=mknode(EXT_VAR_DEF,$1,$2,NULL,yylineno);}   
         | Specifier SEMI
         | Specifier FuncDec CompSt    {$$=mknode(FUNC_DEF,$1,$2,$3,yylineno);}         
         | error SEMI   {$$=NULL;}
         ;

ExtDecList:  VarDec      {$$=$1;}      
           | VarDec COMMA ExtDecList {$$=mknode(EXT_DEC_LIST,$1,$3,NULL,yylineno);}
           ;

Specifier:  TYPE    {$$=mknode(TYPE,NULL,NULL,NULL,yylineno);strcpy($$->type_id,$1);if(!strcmp($1, "int"))$$->type=INT;if(!strcmp($1, "float"))$$->type=FLOAT;if(!strcmp($1, "char"))$$->type=CHAR;if(!strcmp($1, "string"))$$->type=STRING;}
           | StructSpecifier {$$=$1;}
           ;

StructSpecifier: STRUCT OptTag LC DefList RC {$$=mknode(STRUCT_DEF,$2,$4,NULL,yylineno);}
          | STRUCT Tag  {$$=mknode(STRUCT_DEC,$2,NULL,NULL,yylineno);}
          ;

OptTag: {$$=NULL;}
       | ID {$$=mknode(STRUCT_TAG,NULL,NULL,NULL,yylineno);strcpy($$->struct_name,$1);}
       ;
Tag: ID {$$=mknode(STRUCT_TAG,NULL,NULL,NULL,yylineno);strcpy($$->struct_name,$1);}
    ;

VarDec:  ID          {$$=mknode(ID,NULL,NULL,NULL,yylineno);strcpy($$->type_id,$1);}   
        | VarDec LB INT RB {struct node *temp=mknode(INT,NULL,NULL,NULL,yylineno);temp->type_int=$3;$$=mknode(ARRAY_DEC, $1, temp, NULL,yylineno);}
         ;
FuncDec: ID LP VarList RP   {$$=mknode(FUNC_DEC,$3,NULL,NULL,yylineno);strcpy($$->type_id,$1);}
	|ID LP  RP   {$$=mknode(FUNC_DEC,NULL,NULL,NULL,yylineno);strcpy($$->type_id,$1);}
        ;
VarList: ParamDec  {$$=mknode(PARAM_LIST,$1,NULL,NULL,yylineno);}
        | ParamDec COMMA  VarList  {$$=mknode(PARAM_LIST,$1,$3,NULL,yylineno);}
        ;
ParamDec: Specifier VarDec         {$$=mknode(PARAM_DEC,$1,$2,NULL,yylineno);}
         ;

CompSt: LC DefList StmList RC    {$$=mknode(COMP_STM,$2,$3,NULL,yylineno);}
       ;
       
StmList: {$$=NULL; }
        | Stmt StmList  {$$=mknode(STM_LIST,$1,$2,NULL,yylineno);}
        ;
        
Stmt:   Exp SEMI    {$$=mknode(EXP_STMT,$1,NULL,NULL,yylineno);}
      | CompSt      {$$=$1;}      
      | RETURN Exp SEMI   {$$=mknode(RETURN,$2,NULL,NULL,yylineno);}
      | IF LP Exp RP Stmt %prec LOWER_THEN_ELSE   {$$=mknode(IF_THEN,$3,$5,NULL,yylineno);}
      | IF LP Exp RP Stmt ELSE Stmt   {$$=mknode(IF_THEN_ELSE,$3,$5,$7,yylineno);}
      | WHILE LP Exp RP Stmt {$$=mknode(WHILE,$3,$5,NULL,yylineno);}
      | FOR LP ForDec RP Stmt {$$=mknode(FOR,$3,$5,NULL,yylineno);}
      ;

ForDec: Exp SEMI Exp SEMI Exp {$$=mknode(FOR_DEC,$1,$3,$5,yylineno);}
       | SEMI Exp SEMI {$$=mknode(FOR_DEC,NULL,$2,NULL,yylineno);}
       ;

DefList: {$$=NULL; }
        | Def DefList {$$=mknode(DEF_LIST,$1,$2,NULL,yylineno);}
        ;
Def:    Specifier DecList SEMI {$$=mknode(VAR_DEF,$1,$2,NULL,yylineno);}
        ;
DecList: Dec  {$$=mknode(DEC_LIST,$1,NULL,NULL,yylineno);}
       | Dec COMMA DecList  {$$=mknode(DEC_LIST,$1,$3,NULL,yylineno);}
	;
Dec:     VarDec  {$$=$1;}
       | VarDec ASSIGNOP Exp  {$$=mknode(ASSIGNOP,$1,$3,NULL,yylineno);strcpy($$->type_id,"ASSIGNOP");}
       ;
Exp:    Exp ASSIGNOP Exp {$$=mknode(ASSIGNOP,$1,$3,NULL,yylineno);strcpy($$->type_id,"ASSIGNOP");}
      | Exp AND Exp   {$$=mknode(AND,$1,$3,NULL,yylineno);strcpy($$->type_id,"AND");}
      | Exp OR Exp    {$$=mknode(OR,$1,$3,NULL,yylineno);strcpy($$->type_id,"OR");}
      | Exp RELOP Exp {$$=mknode(RELOP,$1,$3,NULL,yylineno);strcpy($$->type_id,$2);}  
      | Exp PLUS Exp  {$$=mknode(PLUS,$1,$3,NULL,yylineno);strcpy($$->type_id,"PLUS");}
      | Exp PLUS PLUS  {$$=mknode(PPLUS,$1,NULL,NULL,yylineno);strcpy($$->type_id,"PPLUS");}
      | Exp PLUS ASSIGNOP Exp {$$=mknode(PLUSASSIGNOP,$1,$4,NULL,yylineno);strcpy($$->type_id,"PLUSASSIGNOP");}
      | Exp MINUS Exp {$$=mknode(MINUS,$1,$3,NULL,yylineno);strcpy($$->type_id,"MINUS");}
      | Exp MINUS MINUS  {$$=mknode(MMINUS,$1,NULL,NULL,yylineno);strcpy($$->type_id,"MMINUS");}
      | Exp MINUS ASSIGNOP Exp {$$=mknode(MINUSASSIGNOP,$1,$4,NULL,yylineno);strcpy($$->type_id,"MINUSASSIGNOP");}
      | Exp STAR Exp  {$$=mknode(STAR,$1,$3,NULL,yylineno);strcpy($$->type_id,"STAR");}
      | Exp STAR ASSIGNOP Exp {$$=mknode(STARASSIGNOP,$1,$4,NULL,yylineno);strcpy($$->type_id,"STARASSIGNOP");}
      | Exp DIV Exp   {$$=mknode(DIV,$1,$3,NULL,yylineno);strcpy($$->type_id,"DIV");}
      | Exp DIV ASSIGNOP Exp {$$=mknode(DIVASSIGNOP,$1,$4,NULL,yylineno);strcpy($$->type_id,"DIVASSIGNOP");}
      | LP Exp RP     {$$=$2;}
      | MINUS Exp %prec UMINUS   {$$=mknode(UMINUS,$2,NULL,NULL,yylineno);strcpy($$->type_id,"UMINUS");}
      | NOT Exp       {$$=mknode(NOT,$2,NULL,NULL,yylineno);strcpy($$->type_id,"NOT");}
      | ID LP Args RP {$$=mknode(FUNC_CALL,$3,NULL,NULL,yylineno);strcpy($$->type_id,$1);}
      | ID LP RP      {$$=mknode(FUNC_CALL,NULL,NULL,NULL,yylineno);strcpy($$->type_id,$1);}
      | Exp LB Exp RB {$$=mknode(EXP_ARRAY,$1,$3,NULL,yylineno);}
      | Exp DOT ID {struct node *temp=mknode(ID,NULL,NULL,NULL,yylineno);strcpy($$->type_id,$3);$$=mknode(EXP_ELE,$1,temp,NULL,yylineno);}//$$=mknode(EXP_ELE,$1,$3,NULL,yylineno);
      | ID            {$$=mknode(ID,NULL,NULL,NULL,yylineno);strcpy($$->type_id,$1);}
      | INT           {$$=mknode(INT,NULL,NULL,NULL,yylineno);$$->type_int=$1;$$->type=INT;}
      | FLOAT         {$$=mknode(FLOAT,NULL,NULL,NULL,yylineno);$$->type_float=$1;$$->type=FLOAT;}
      | CHAR           {$$=mknode(CHAR,NULL,NULL,NULL,yylineno);$$->type_char=$1[1];$$->type=CHAR;}
      | STRING         {$$=mknode(STRING,NULL,NULL,NULL,yylineno);strcpy($$->type_string,$1);$$->type=STRING;}
      ;

Args:    Exp COMMA Args    {$$=mknode(ARGS,$1,$3,NULL,yylineno);}
       | Exp               {$$=mknode(ARGS,$1,NULL,NULL,yylineno);}
       ;

%%

int main(int argc, char *argv[]){
	yyin=fopen(argv[1],"r");
	if (!yyin) return 0;
	yylineno=1;
	yyparse();
	return 0;
}

#include<stdarg.h>
void yyerror(const char* fmt, ...){
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "===>ERROR:Grammar Error at Line %d Column %d: ", yylloc.first_line,yylloc.first_column);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, ".\n");
}
```

## 语义分析

语义分析是我最头痛的部分

首先原项目对基本表达式并不完善，我在此基础上添加了对 CHAR，FLOAT 类型的处理，并扩展了对算数运算符的处理

```c
void Exp(struct node *T)
{
    if (T)
    {
        switch (T->kind)
        {
            case ID:
                id_exp(T);
                break;
            case CHAR: 
                char_exp(T); /* 新添 */
                break;
            case INT:
                int_exp(T);
                break;
            case FLOAT:
                float_exp(T); /* 新添 */
                break;
            case ASSIGNOP:
                assignop_exp(T); /* 扩展 */
                break;
            case PPLUS:
            case MMINUS:
                oop_exp(T); /* 新添 */
                break;
            case AND:
            case OR:
            case RELOP:
                relop_exp(T);
                break;
            case PLUSASSIGNOP:
            case MINUSASSIGNOP:
            case STARASSIGNOP:
            case DIVASSIGNOP:
            case PLUS:
            case MINUS:
            case STAR:
            case DIV:
                op_exp(T); /* 扩展 */
                break;
            case FUNC_CALL: 
                func_call_exp(T);
                break;
            case ARGS: 
                args_exp(T);
                break;
            case EXP_ARRAY: /* EXP_ARRAY在赋值语句中才有意义 */
                break;
        }
    }
}
```

CHAR：

```c
void char_exp(struct node *T){
    struct opn opn1, opn2, result;
    T->place = temp_add(newTemp(), LEV, T->type, 'T', T->offset); 
    T->type = CHAR;
    T->width = 1;
}
```

FLOAT：

```c
void float_exp(struct node *T){
    struct opn opn1, opn2, result;
    T->place = temp_add(newTemp(), LEV, T->type, 'T', T->offset); //为整常量生成一个临时变量
    T->type = FLOAT;
    opn1.kind = FLOAT;
    opn1.const_float = T->type_float;
    result.kind = ID;
    strcpy(result.id, symbolTable.symbols[T->place].alias);
    result.offset = symbolTable.symbols[T->place].offset;
    T->code = genIR(ASSIGNOP, opn1, opn2, result);
    T->width = 4;
}
```

PPLUS，MMINUS：

```c
void oop_exp(struct node *T){
    struct opn opn1, opn2, result;
    T->ptr[0]->offset = T->offset;
    Exp(T->ptr[0]);

    T->type = INT, T->width = T->ptr[0]->width;

    opn1.kind = ID;
    strcpy(opn1.id, symbolTable.symbols[T->ptr[0]->place].alias);
    opn1.type = T->ptr[0]->type;
    opn1.offset = symbolTable.symbols[T->ptr[0]->place].offset;

    T->code = merge(2, T->ptr[0]->code, genIR(T->kind, opn1, opn2, result));
    T->width = T->ptr[0]->width;
}
```

接着就是对数组的特殊处理，这里需要解决的第一个问题就是如何在中间语言 TAC 中表示数组

我的处理方式也比较粗暴，直接新添一个数组类型 `A`，当 `print_IR` 函数遇到 `A` 类型时，就会打印一个 `ARRAY h->result.id`

```c
        case ARRAY:
            printf("  ARRAY %s\n", h->result.id);
            break;
```

+ 为了方便在生成汇编代码时开辟栈空间

数组有两个节点类型：EXP_ARRAY 数组引用，ARRAY_DEC 数组定义

对于 ARRAY_DEC 又分为局部数组定义和全局数组定义，我对它们的操作体现在如下函数中：

局部数组定义：

```c
void var_def(struct node *T) /* TYPE || DEC_LIST */
{
    ......
    while (T0) /* 处理DEC_LIST结点 */
    { 
        num++;
        T0->ptr[0]->type = T0->type; //类型属性向下传递
        if (T0->ptr[1])
            T0->ptr[1]->type = T0->type;

        T0->ptr[0]->offset = T0->offset; //类型属性向下传递
        if (T0->ptr[1])
            T0->ptr[1]->offset = T0->offset + width;
        if (T0->ptr[0]->kind == ID)
        {
            ......
        }
        else if (T0->ptr[0]->kind == ARRAY_DEC){
            rtn = fillSymbolTable(T0->ptr[0]->ptr[0]->type_id, newAlias(), LEV, T0->ptr[0]->ptr[0]->type, 'A', T->offset + T->width); //此处偏移量未计算，暂时为0
            if (rtn == -1)
                semantic_error(T0->ptr[0]->ptr[0]->pos, T0->ptr[0]->ptr[0]->type_id, "变量重复定义");
            else
                T0->ptr[0]->ptr[0]->place = rtn;
            
            symbolTable.symbols[rtn].paramnum = T0->ptr[0]->ptr[1]->type_int;
            T->width += width * T0->ptr[0]->ptr[1]->type_int;

            result.kind = ID;
            sprintf(result.id,"%s[%d]",symbolTable.symbols[rtn].alias,T0->ptr[0]->ptr[1]->type_int);
            result.offset = T->offset;
            T->code = merge(2, T->code, genIR(ARRAY, opn1, opn2, result));
        }
        else if (T0->ptr[0]->kind == ASSIGNOP)
        {
            ......
        }

        T0 = T0->ptr[1];
    }
}
```

+ 模仿 ID 分支写的，进行了一些特殊处理
+ 也因为这些特殊处理导致了展示没法定义多维数组（在以后的版本中可能会进行改进）

全局数组定义：

```c
void ext_var_list(struct node *T,int is_arr) /* EXT_DEC_LIST */
{
    int rtn, num = 1;
    switch (T->kind)
    {
    case EXT_DEC_LIST:
        T->ptr[0]->type = T->type;                //将类型属性向下传递变量结点
        T->ptr[0]->offset = T->offset;            //外部变量的偏移量向下传递
        T->ptr[1]->type = T->type;                //将类型属性向下传递变量结点
        T->ptr[1]->offset = T->offset + T->width; //外部变量的偏移量向下传递
        T->ptr[1]->width = T->width;
        ext_var_list(T->ptr[0],0);
        ext_var_list(T->ptr[1],0);
        T->num = T->ptr[1]->num + 1;
        break;
    case ID:
        if(is_arr==1){
            rtn = fillSymbolTable(T->type_id, newAlias(), LEV, T->type, 'A', T->offset); 
        }
        else{
            rtn = fillSymbolTable(T->type_id, newAlias(), LEV, T->type, 'V', T->offset); 
        }
        if (rtn == -1)
            semantic_error(T->pos, T->type_id, "变量重复定义，语义错误");
        else
            T->place = rtn;
        T->num = 1;
        symbolTable.symbols[T->place].paramnum = 1;
        break;
    case ARRAY_DEC:
        T->ptr[0]->type = T->type;                
        T->ptr[0]->offset = T->offset;      
        T->ptr[0]->width = T->width * T->ptr[1]->type_int;    
        ext_var_list(T->ptr[0],1);
        symbolTable.symbols[T->ptr[0]->place].paramnum = T->ptr[1]->type_int;
        break;
    default:
        break;
    }
}
```

+ 全局数组的定义充分利用了 `ext_var_list` 函数的递归，理论上有多维数组的可能

数组引用：

```c
void assignop_exp(struct node *T)
{
    struct opn opn1, opn2, result;
    char name[0x20];
    if(T->ptr[0]->kind == EXP_ARRAY){ /* EXP_ARRAY ASSIGNOP Exp */
        T->ptr[0]->offset = T->offset;
        Exp(T->ptr[0]->ptr[0]);
        T->type = T->ptr[0]->ptr[0]->type;
        Exp(T->ptr[0]->ptr[1]);
        T->ptr[0]->code = T->ptr[0]->ptr[0]->code;

        T->ptr[1]->offset = T->offset;
        Exp(T->ptr[1]);
        T->type = T->ptr[0]->ptr[0]->type;
        T->width = T->ptr[1]->width;
        T->code = merge(2, T->ptr[0]->code, T->ptr[1]->code);

        opn1.kind = ID;
        strcpy(opn1.id, symbolTable.symbols[T->ptr[1]->place].alias); 
        opn1.offset = symbolTable.symbols[T->ptr[1]->place].offset;
        result.kind = ID;
        sprintf(name,"%s[%d]",symbolTable.symbols[T->ptr[0]->ptr[0]->place].alias,T->ptr[0]->ptr[1]->type_int);
        strcpy(result.id, name);
        result.offset = symbolTable.symbols[T->ptr[0]->ptr[0]->place].offset;
        T->code = merge(2, T->code, genIR(ASSIGNOP, opn1, opn2, result));
    }
    else if (T->ptr[0]->kind != ID)
    {
        semantic_error(T->pos, "", "赋值语句没有左值，语义错误");
    }
    else
    {
        ......
    }
}
```

+ 这里目前只能选择特殊处理（使用递归的难度有点高），也就不支持多维数组

目前我是利用 `symbol->paramnum` 来存储数组大小（当标记为 `F` 时，该值代表函数参数个数），而没有使用 `node->width`

这是因为我把控不好 `node->width` 在递归调用中的变化，一不小心就可能漏写造成 BUG，目前还需要大量调试来对各个函数中的 `node->width` 进行修正

另外还完善了 FOR 语句：

```c
        case FOR:
            for_dec(T);
            break; 
```

```c
void for_list(struct node* T){
    if(T->ptr[0]!=NULL){
        T->ptr[0]->offset = T->ptr[2]->offset = T->offset;
        if(T->ptr[0]->kind == ASSIGNOP){
            semantic_Analysis(T->ptr[0]);
        }
        else{
            semantic_error(T->pos, "", "for wrong\n");
        }
        if(T->ptr[2]->kind != ASSIGNOP){
            semantic_Analysis(T->ptr[2]);
        }
        else{
            semantic_error(T->pos, "", "for wrong\n");
        }
    }

    T->ptr[1]->offset = T->offset; 
    strcpy(T->ptr[1]->Etrue, T->Etrue); 
    strcpy(T->ptr[1]->Efalse, T->Efalse);
    boolExp(T->ptr[1]);
    T->width = T->ptr[0]->width+T->ptr[1]->width+T->ptr[2]->width;
}
```

```c
void for_dec(struct node* T)
{
    strcpy(T->ptr[0]->Etrue, newLabel()); 
    strcpy(T->ptr[0]->Efalse, T->Snext);
    T->ptr[0]->offset = T->ptr[1]->offset = T->offset;
    semantic_Analysis(T->ptr[0]); 
    strcpy(T->ptr[1]->Snext, newLabel());
    semantic_Analysis(T->ptr[1]); 
    if (T->width < T->ptr[1]->width)
        T->width = T->ptr[1]->width;
    T->code = merge(7, T->ptr[0]->ptr[0]->code, genLabel(T->ptr[1]->Snext), T->ptr[0]->ptr[1]->code,
                    genLabel(T->ptr[0]->ptr[1]->Etrue), T->ptr[1]->code, T->ptr[0]->ptr[2]->code, genGoto(T->ptr[1]->Snext));
}
```

+ 基本上就是拆分为3个 Exp 和1个复合语句

剩下的代码和原项目差不多，就不多赘述了

## 生成 IR 中间语言

生成中间代码有两大核心函数：`genIR` 和 `print_IR`

函数 `genIR` 没有什么变化，函数 `print_IR` 也只修改了数组和算数运算的相关代码：

```c
void print_IR(struct codenode *head)
{
    char opnstr1[32], opnstr2[32], resultstr[32];
    struct codenode *h = head;
    do
    {
        if (h->opn1.kind == INT)
            sprintf(opnstr1, "#%d", h->opn1.const_int);
        if (h->opn1.kind == FLOAT)
            sprintf(opnstr1, "#%f", h->opn1.const_float);
        if (h->opn1.kind == ID)
            sprintf(opnstr1, "%s", h->opn1.id);
        if (h->opn2.kind == INT)
            sprintf(opnstr2, "#%d", h->opn2.const_int);
        if (h->opn2.kind == FLOAT)
            sprintf(opnstr2, "#%f", h->opn2.const_float);
        if (h->opn2.kind == ID)
            sprintf(opnstr2, "%s", h->opn2.id);
        sprintf(resultstr, "%s", h->result.id);
        switch (h->op)
        {
        case ASSIGNOP:
            printf("  %s := %s\n", resultstr, opnstr1);
            break;
        case PPLUS:
        case MMINUS:
            printf("  %s := %s %c 1\n", opnstr1, opnstr1,
                   h->op == PPLUS ? '+' : '-' );
            break;
        case PLUS:
        case MINUS:
        case STAR:
        case DIV:
            printf("  %s := %s %c %s\n", resultstr, opnstr1,
                   h->op == PLUS ? '+' : h->op == MINUS ? '-' : h->op == STAR ? '*' : '\\', opnstr2);
            break;
        case PLUSASSIGNOP:
        case MINUSASSIGNOP:
        case STARASSIGNOP:
        case DIVASSIGNOP:
            printf("  %s := %s %c %s\n", opnstr1, opnstr1,
                   h->op == PLUSASSIGNOP ? '+' : h->op == MINUSASSIGNOP ? '-' : h->op == STARASSIGNOP ? '*' : '\\', opnstr2);
            break;
        case FUNCTION:
            printf("\nFUNCTION %s :\n", h->result.id);
            break;
        case ARRAY:
            printf("  ARRAY %s\n", h->result.id);
            break;
        case PARAM:
            printf("  PARAM %s\n", h->result.id);
            break;
        case LABEL:
            printf("LABEL %s :\n", h->result.id);
            break;
        case GOTO:
            printf("  GOTO %s\n", h->result.id);
            break;
        case JLE:
            printf("  IF %s <= %s GOTO %s\n", opnstr1, opnstr2, resultstr);
            break;
        case JLT:
            printf("  IF %s < %s GOTO %s\n", opnstr1, opnstr2, resultstr);
            break;
        case JGE:
            printf("  IF %s >= %s GOTO %s\n", opnstr1, opnstr2, resultstr);
            break;
        case JGT:
            printf("  IF %s > %s GOTO %s\n", opnstr1, opnstr2, resultstr);
            break;
        case EQ:
            printf("  IF %s == %s GOTO %s\n", opnstr1, opnstr2, resultstr);
            break;
        case NEQ:
            printf("  IF %s != %s GOTO %s\n", opnstr1, opnstr2, resultstr);
            break;
        case ARG:
            printf("  ARG %s\n", h->result.id);
            break;
        case CALL:
            printf("  %s := CALL %s\n", resultstr, opnstr1);
            break;
        case RETURN:
            if (h->result.kind)
                printf("  RETURN %s\n", resultstr);
            else
                printf("  RETURN\n");
            break;
        }
        h = h->next;
    } while (h != head);
}
```

目前 IR 转汇编的部分还没有实现，这次我打算直接转化为 AT&T 格式的汇编指令（MIPS 虚拟机不好调试）

