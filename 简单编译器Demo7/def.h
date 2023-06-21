#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "stdarg.h"
#include "parser.tab.h"
#include <sys/types.h>
#include <sys/stat.h>

enum node_kind
{
    EXT_DEF_LIST,
    EXT_VAR_DEF,
    FUNC_DEF,
    FUNC_DEC,
    EXT_STRUCT_DEF,
    STRUCT_DEF,
    STRUCT_DEC,
    STRUCT_TAG,
    EXP_ELE,
    EXP_IDS,
    EXP_IDA,
    EXP_ARRAY,
    ARRAY_DEC,
    EXT_DEC_LIST,
    PARAM_LIST,
    ARRAY_LIST,
    PARAM_DEC,
    VAR_DEF,
    DEC_LIST,
    DEF_LIST,
    COMP_STM,
    STM_LIST,
    EXP_STMT,
    FOR_DEC,
    IF_THEN,
    IF_THEN_ELSE,
    FUNC_CALL,
    ARGS,
    FUNCTION,
    PARAM,
    ARG,
    ARRAY,
    CALL,
    LABEL,
    GOTO,
    JLT,
    JLE,
    JGT,
    JGE,
    EQ,
    NEQ
};

#define ALIGN(x, aligin) ((x + aligin -1) & ~(aligin -1))
#define MAXLENGTH 1000     //定义符号表的大小
#define DX 3 * sizeof(int) //活动记录控制信息需要的单元数

struct opn
{
    int kind; //标识操作的类型
    int type; //标识操作数的类型
    union {
        int const_int;     //整常数值，立即数
        float const_float; //浮点常数值，立即数
        char const_char;   //字符常数值，立即数
        char *const_string;
        char id[33]; //变量或临时变量的别名或标号字符串
    };
    struct Array *type_array;
    struct Struct *type_struct;
    int level;  //变量的层号，0表示是全局变量，数据保存在静态数据区
    int offset; //变量单元偏移量，或函数在符号表的定义位置序号，目标代码生成时用
};

struct codenode
{                                  //三地址TAC代码结点,采用双向循环链表存放中间语言代码
    int op;                        //TAC代码的运算符种类
    struct opn opn1, opn2, result; //2个操作数和运算结果
    struct codenode *next, *prior;
};

union Value {
    char type_id[33]; //由标识符生成的叶结点
    int type_int;     //由整常数生成的叶结点
    float type_float; //由浮点常数生成的叶结点
    char type_char;
    char type_string[31];
};

// 使用链表存储多个变量
struct Array
{
    int kind;
    int index;
    int offset;
    union Value value;
    char name[32];
    struct Array *next;
};

// 使用链表存储多个变量
struct Struct
{
    int kind;
    char *name; // 字段名字
    union Value value;
    struct Struct *next;
};

struct node
{                        //以下对结点属性定义没有考虑存储效率，只是简单地列出要用到的一些属性
    enum node_kind kind; //结点类型
    char struct_name[33];
    union {
        char type_id[33]; //由标识符生成的叶结点
        int type_int;     //由整常数生成的叶结点
        float type_float; //由浮点常数生成的叶结点
        char type_char;
        char type_string[31];
    };
    struct Array *type_array;
    struct Struct *type_struct;
    struct node *ptr[3];        //子树指针，由kind确定有多少棵子树
    int level;                  //层号
    int place;                  //表示结点对应的变量或运算结果符号表的位置序号
    char Etrue[15], Efalse[15]; //对布尔表达式的翻译时，真假转移目标的标号
    char Snext[15];             //该结点对饮语句执行后的下一条语句位置标号
    struct codenode *code;      //该结点中间代码链表头指针
    char op[10];
    int type;   //结点对应值的类型
    int pos;    //语法单位所在位置行号
    int offset; //偏移量
    int width;  //占数据字节数
    int num;
};

struct symbol
{                   
    char name[33];  
    int level;     
    int type;       
    int paramnum;   
    char array[32]; 
    char stname[32];
    char alias[10]; 
    char flag;      
    int offset;    
};
//符号表，是一个顺序栈，index初值为0
struct symboltable
{
    struct symbol symbols[MAXLENGTH];
    int index;
} symbolTable;

struct symbol_scope_begin
{ /*当前作用域的符号在符号表的起始位置序号,这是一个栈结构，/每到达一个复合语句，将符号表的index值进栈，离开复合语句时，取其退栈值修改符号表的index值，完成删除该复合语句中的所有变量和临时变量*/
    int TX[30];
    int top;
} symbol_scope_TX;


/*generate AST*/
struct node *mknode(int kind, struct node *first, struct node *second, struct node *third, int pos);

/*semantic analysis*/
void semantic_error(int line, char *msg1, char *msg2);
int searchSymbolTable(char *name);
int fillSymbolTable(char *name, char *alias, int level, int type, char flag, int offset);
void Exp(struct node *T);
void boolExp(struct node *T);
void semantic_Analysis(struct node *T);
void DisplaySymbolTable(struct node *T);

/*inner code generation*/
char *str_catch(char *s1, char *s2);
char *newAlias();
char *newAliasEx();
char *newLabel();
char *newTemp();
struct codenode *genIR(int op, struct opn opn1, struct opn opn2, struct opn result);
struct codenode *genLabel(char *label);
struct codenode *genGoto(char *label);
struct codenode *merge(int num, ...);
void print_IR(struct codenode *head);
void print_io(char* msg,FILE* fd);

void id_exp(struct node *T);
void int_exp(struct node *T);
void char_exp(struct node *T);
void float_exp(struct node *T);
void string_exp(struct node* T);
void assignop_exp(struct node *T);
void relop_exp(struct node *T);
void args_exp(struct node *T);
void op_exp(struct node *T);
void oop_exp(struct node *T);
void func_call_exp(struct node *T);
void not_exp(struct node *T);
void ext_var_list(struct node *T,int key);
void ext_def_list(struct node *T);
void ext_var_def(struct node *T);
void func_def(struct node *T);
void func_dec(struct node *T);
void param_list(struct node *T);
void param_dec(struct node *T);
void comp_stm(struct node *T);
void def_list(struct node *T);
void var_def(struct node *T);
void stmt_list(struct node *T);
void if_then(struct node *T);
void if_then_else(struct node *T);
void while_dec(struct node *T);
void for_dec(struct node *T);
void for_list(struct node* T);
void exp_stmt(struct node *T);
void struct_def(struct node *T);
void return_dec(struct node *T);
void struct_exp(struct node *T);
int array_exp(struct node *,struct node **,int *,int);

FILE* fd;
size_t stack_base;
size_t num_buf;
size_t is_arrVar[2];
char global_buf[0x80];
char temp_buf[4][0x30];
