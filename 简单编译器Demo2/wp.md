## 基础信息

本代码是在以下项目的基础上进行完善：

+ [GitHub - Billy1900/compiler-course-project: 华中科技大学编译原理实验2019](https://github.com/Billy1900/compiler-course-project)

之前已经完成：词法分析，语法分析，语义分析

词法分析没有改变，语法分析有细微的变动

另外写了一个 makefile 文件：

```makefile
OBJS=main.o tool1.o tool2.o
CC=gcc
FL=flex
BI=bison

CFLAGS=-c -Wall -g

all:
	$(FL) $^ lexer.l
	$(BI) $^ -d -v parser.y
	$(CC) $^ display.c semantic_analysis.c parser.tab.c lex.yy.c -lfl -o trans
```

## 语法分析

添加了一个规则用于表示数组列表：

```c
ArrayList: LB Exp RB ArrayList {$$=mknode(ARRAY_LIST,$2,$4,NULL,yylineno);}
	| LB Exp RB {$$=mknode(ARRAY_LIST,$2,NULL,NULL,yylineno);}
	;
```

+ PS：其实这里根本没有用到第一条路径，后面会进行说明

对于数组来说，需要用到 ArrayList 的地方只有两处：

+ 数组定义

```c
VarDec:  ID          {$$=mknode(ID,NULL,NULL,NULL,yylineno);strcpy($$->type_id,$1);}   
        | VarDec ArrayList {$$=mknode(ARRAY_DEC,$1,$2,NULL,yylineno);}
        ;
```

+ 数组使用（可以把数组看做一种表达式）

```c
Exp:    Exp ASSIGNOP Exp {$$=mknode(ASSIGNOP,$1,$3,NULL,yylineno);strcpy($$->type_id,"ASSIGNOP");}
	......
      | Exp ArrayList {$$=mknode(EXP_ARRAY,$1,$2,NULL,yylineno);}
	......
      ;
```

这里有一处细节需要说明：案例 `arr[4][5][6]`

+ 分解后的结构如下：

```
arr => ID(arr)
arr[4] => ARRAY_DEC(arr) + ArrayList(Exp[4])
arr[4][5] => ARRAY_DEC(arr[4]) + ArrayList(Exp[5])
arr[4][5][6] => ARRAY_DEC(arr[4][5]) + ArrayList(Exp[6])
```

+ 在实际构造 AST 时，程序会从下往上进行遍历分析
+ 发现 ArrayList 永远是 `LB Exp RB`，不会走第一条路径（刚开始设计时害怕程序识别不了多个 `LB Exp RB` 于是就写了一个递归的规则）

## 语义分析

语义分析部分主要是对多级数组进行处理

首先对符号表结构进行了修改，添加了一个条目用于存储数组各级的大小

```c
struct symbol
{                   
    char name[33];  
    int level;      
    int type;      
    int paramnum;  
    char array[32]; //存储数组各级的大小
    char alias[10];
    char flag;     
    int offset;    
};
```

对全局数组定义的处理：

```c
void ext_var_list(struct node *T,int is_arr) /* EXT_DEC_LIST */
{
    int rtn, num = 1;
    switch (T->kind)
    {
            ......
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
    case ARRAY_DEC: /* ARRAY_DEC,ARRAY_LIST */
        T->ptr[0]->type = T->type;                
        T->ptr[0]->offset = T->offset;      
        T->ptr[0]->width = T->width * T->ptr[1]->type_int;  
        symbolTable.symbols[T->ptr[0]->place].paramnum = 0;
        ext_var_list(T->ptr[0],1);
        symbolTable.symbols[T->ptr[0]->place].paramnum += 1;
        symbolTable.symbols[T->ptr[0]->place].array[symbolTable.symbols[T->ptr[0]->place].paramnum-1] = T->ptr[1]->ptr[0]->type_int;
        break;
    default:
        break;
    }
}
```

+ 核心点其实就是一个递归调用，不断调用 `ext_var_list` 直到 T->kind 为 ID
+ 然后进行回溯，同时向 `symbolTable.symbols[place].array` 中填入各级数组的值

对局部数组定义的处理：

```c
void var_def(struct node *T) /* TYPE || DEC_LIST */
{
    int rtn, num, width;
    struct node *T0,*T1;
    struct opn opn1, opn2, result;
    T->code = NULL;
    char arr[32];
    char type[8];
    
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
        if (T0->ptr[0]->kind == ID){
            ......
        }
        else if (T0->ptr[0]->kind == ARRAY_DEC){ /* ARRAY_DEC,ARRAY_LIST */
            T1 = T0->ptr[0];
            while (T1->kind == ARRAY_DEC){
                T1 = T1->ptr[0];
            }
            T1->type = T0->type;

            rtn = fillSymbolTable(T1->type_id, newAlias(), LEV, T1->type, 'A', T->offset + T->width); 
            if (rtn == -1)
                semantic_error(T1->pos, T1->type_id, "变量重复定义");
            else
                T1->place = rtn;
            
            symbolTable.symbols[rtn].paramnum = 1;
            T1 = T0->ptr[0];
            while (T1->kind == ARRAY_DEC)
            {
                arr[symbolTable.symbols[rtn].paramnum++] = T1->ptr[1]->ptr[0]->type_int;
                T1 = T1->ptr[0];
            }
            arr[0] = 0;
            for(int i=0;i<symbolTable.symbols[rtn].paramnum;i++)
            {
                symbolTable.symbols[rtn].array[i+1] = arr[symbolTable.symbols[rtn].paramnum-i-1];
                if(i==symbolTable.symbols[rtn].paramnum-1) 
                    continue;
                width *= arr[i+1];
            }

            ShowSymbolTable(T1,0);

            T->width += width;
            result.kind = ID;

            sprintf(result.id,"%s(%s)[%d]",symbolTable.symbols[rtn].alias, check_type(symbolTable.symbols[rtn].type,&type),width);
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

+ 这里就没有采用递归的做法（因为要处理变量初始化，很难实现递归），而是直接遍历链表
+ 这样做会导致顺序问题，因为之前是在递归函数回溯时填写符号表的，相当于一个逆序操作，而遍历则是一个顺序操作
+ 为了这两者的顺序统一，就写了一个 `arr[32]` 来暂时存放各级数组的数值，然后逆序取出

对数组使用的处理：

```c
void array_exp(struct node *T,struct node **Tr,int *sum,int key)
{
    struct node *T0;
    char arr[32];
    char name[32];
    int i=0,num;
    
    T0 = T->ptr[key];
    while (T0->kind == EXP_ARRAY) /* Exp,ArrayList */
    {
        arr[i++] = T0->ptr[1]->ptr[0]->type_int;
        T0 = T0->ptr[0];
    }
    T0->offset = T->offset;
    *Tr = T0;

    Exp(T0);

    if(i!=symbolTable.symbols[T0->place].paramnum-1){
        semantic_error(T->pos, "", "多级数组的阶数不匹配");
    }
    T->type = T0->type;
    T->width = T0->width;
    T->code = T0->code;

    *sum = 0;
    for(int x=0;x<i;x++){
        num = arr[x];
        for(int y=0;y<x;y++){
            num *= symbolTable.symbols[T0->place].array[symbolTable.symbols[T0->place].paramnum-y-1];
        }
        *sum += num;
    }   
    
    T->ptr[1-key]->offset = T->offset;
    Exp(T->ptr[1-key]);
    if(symbolTable.symbols[T->ptr[1-key]->place].flag == 'A'){
        semantic_error(T->pos, T->ptr[1-key]->type_id, "是数组名，不是普通变量，语义错误");
    }

    T->code = merge(2, T->code, T->ptr[1-key]->code);
}
```

+ 传入的参数 key 用于表示 左边/右边 为数组，Tr 和 sum 是需要外传的参数
+ 使用公式把多维数组转化为一维数组
+ 数组通常在赋值语句中发挥作用，于是就有3种情况发生：
  + 赋值语句左边为数组，右边不是
  + 赋值语句右边为数组，左边不是
  + 赋值语句左右两边都是数组

在赋值语句中添加对数组的处理：

```c
void assignop_exp(struct node *T)
{
    struct opn opn1, opn2, result;
    struct node *T0;
    char arr[32];
    char name[32];
    int i=0,num=0,sum=-1;
    if (T->ptr[1]->kind == EXP_ARRAY && T->ptr[0]->kind == EXP_ARRAY){ /* 等号两端都为数组 */
        array_exp(T,&T0,&sum,1);
        opn1.kind = ID;
        sprintf(name,"%s[%d]",symbolTable.symbols[T0->place].alias,sum);
        strcpy(opn1.id, name);

        array_exp(T,&T0,&sum,0);
        result.kind = ID;
        sprintf(name,"%s[%d]",symbolTable.symbols[T0->place].alias,sum);
        strcpy(result.id, name);

        result.offset = symbolTable.symbols[T0->place].offset;
        T->code = merge(2, T->code, genIR(ASSIGNOP, opn1, opn2, result));
    }
    else if (T->ptr[1]->kind == EXP_ARRAY){ /* 等号右端为数组 */
        array_exp(T,&T0,&sum,1);

        opn1.kind = ID;
        sprintf(name,"%s[%d]",symbolTable.symbols[T0->place].alias,sum);
        strcpy(opn1.id, name);

        result.kind = ID;
        strcpy(result.id, symbolTable.symbols[T->ptr[0]->place].alias); 
        result.offset = symbolTable.symbols[T->ptr[0]->place].offset;

        result.offset = symbolTable.symbols[T0->place].offset;
        T->code = merge(2, T->code, genIR(ASSIGNOP, opn1, opn2, result));
    }
    else if (T->ptr[0]->kind == EXP_ARRAY){ /* 等号左端为数组 */
        array_exp(T,&T0,&sum,0);

        opn1.kind = ID;
        strcpy(opn1.id, symbolTable.symbols[T->ptr[1]->place].alias); 
        opn1.offset = symbolTable.symbols[T->ptr[1]->place].offset;

        result.kind = ID;
        sprintf(name,"%s[%d]",symbolTable.symbols[T0->place].alias,sum);
        strcpy(result.id, name);

        result.offset = symbolTable.symbols[T0->place].offset;
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

+ 按照常规的逻辑进行处理就好

另外此版本的编译器还修改了一些“函数参数处理”中的 BUG：

```c
int match_param(int i, struct node *T) /* PARAM_DEC,PARAM_LIST || PARAM_DEC */
{ 
    int j, num = symbolTable.symbols[i].paramnum;
    int type1, type2, pos;
    if (num == 0 && T == NULL)
        return 1;
    for (j = 0; j < num; j++)
    {
        if (T == NULL){
            semantic_error(pos, "", "函数调用参数太少");
            return 0;
        }
        type1 = symbolTable.symbols[i + j].type; //形参类型
        type2 = T->ptr[0]->type;
        if (type1 != type2){
            semantic_error(T->pos, "", "参数类型不匹配");
            return 0;
        }
        pos = T->pos;
        T = T->ptr[1];
    }
    if (T!=NULL) /* 有多余的实参表达式 */
    { 
        semantic_error(T->pos, "", "函数调用参数太多");
        return 0;
    }
    return 1;
}
```

+ 添加了报错处理

## 生成 IR 中间语言

最后修改了一下打印函数：

```c
void DisplaySymbolTable(struct node *T)
{
    fd = fopen("./inter.txt","w");
    if(fd == NULL){
        puts("缺少inter.txt文件");
        exit(-1);
    }
    symbolTable.index = 0;
    symbol_scope_TX.TX[0] = 0; //外部变量在符号表中的起始序号为0
    symbol_scope_TX.top = 1;
    T->offset = 0; // 外部变量在数据区的偏移量

    semantic_Analysis(T);
    ShowSymbolTable(T,1);

    print_IR(T->code);
    fclose(fd);
}
```

+ 打开 `inter.txt` 文件，并把中间代码写入其中

```c
void print_IR(struct codenode *head)
{
    char opnstr1[32], opnstr2[32], resultstr[32];
    char msg[64];
    struct codenode *h = head;
    if(fd==NULL){
        fd = fopen("./inter.txt","w");
    }
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
            sprintf(msg,"  %s := %s\n", resultstr, opnstr1);
            print_io(msg,fd);
            break;
        case PPLUS:
        case MMINUS:
            sprintf(msg,"  %s := %s %c 1\n", opnstr1, opnstr1,
                   h->op == PPLUS ? '+' : '-' );
            print_io(msg,fd);
            break;
        case PLUS:
        case MINUS:
        case STAR:
        case DIV:
            sprintf(msg,"  %s := %s %c %s\n", resultstr, opnstr1,
                   h->op == PLUS ? '+' : h->op == MINUS ? '-' : h->op == STAR ? '*' : '\\', opnstr2);
            print_io(msg,fd);
            break;
        case PLUSASSIGNOP:
        case MINUSASSIGNOP:
        case STARASSIGNOP:
        case DIVASSIGNOP:
            sprintf(msg,"  %s := %s %c %s\n", opnstr1, opnstr1,
                   h->op == PLUSASSIGNOP ? '+' : h->op == MINUSASSIGNOP ? '-' : h->op == STARASSIGNOP ? '*' : '\\', opnstr2);
            print_io(msg,fd);
            break;
        case FUNCTION:
            sprintf(msg,"\nFUNCTION %s :\n", h->result.id);
            print_io(msg,fd);
            break;
        case ARRAY:
            sprintf(msg,"  ARRAY %s\n", h->result.id);
            print_io(msg,fd);
            break;
        case PARAM:
            sprintf(msg,"  PARAM %s\n", h->result.id);
            print_io(msg,fd);
            break;
        case LABEL:
            sprintf(msg,"LABEL %s :\n", h->result.id);
            print_io(msg,fd);
            break;
        case GOTO:
            sprintf(msg,"  GOTO %s\n", h->result.id);
            print_io(msg,fd);
            break;
        case JLE:
            sprintf(msg,"  IF %s <= %s GOTO %s\n", opnstr1, opnstr2, resultstr);
            print_io(msg,fd);
            break;
        case JLT:
            sprintf(msg,"  IF %s < %s GOTO %s\n", opnstr1, opnstr2, resultstr);
            print_io(msg,fd);
            break;
        case JGE:
            sprintf(msg,"  IF %s >= %s GOTO %s\n", opnstr1, opnstr2, resultstr);
            print_io(msg,fd);
            break;
        case JGT:
            sprintf(msg,"  IF %s > %s GOTO %s\n", opnstr1, opnstr2, resultstr);
            print_io(msg,fd);
            break;
        case EQ:
            sprintf(msg,"  IF %s == %s GOTO %s\n", opnstr1, opnstr2, resultstr);
            print_io(msg,fd);
            break;
        case NEQ:
            sprintf(msg,"  IF %s != %s GOTO %s\n", opnstr1, opnstr2, resultstr);
            print_io(msg,fd);
            break;
        case ARG:
            sprintf(msg,"  ARG %s\n", h->result.id);
            print_io(msg,fd);
            break;
        case CALL:
            sprintf(msg,"  %s := CALL %s\n", resultstr, opnstr1);
            print_io(msg,fd);
            break;
        case RETURN:
            if (h->result.kind){
                sprintf(msg,"  RETURN %s\n", resultstr);
                print_io(msg,fd);
            }
            else{
                sprintf(msg,"  RETURN\n");
                print_io(msg,fd);
            } 
            break;
        }
        h = h->next;
    } while (h != head);
}

void print_io(char* msg,FILE* fd){
    printf(msg);
    fprintf(fd,msg);
}
```

+ 在生成的中间代码中会标识数组的类型
+ 全局变量也会被添加到中间代码中

下个版本打算添加对 “结构体” 和 “指针” 的处理

