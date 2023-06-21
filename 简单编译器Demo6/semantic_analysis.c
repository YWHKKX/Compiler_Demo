#include "def.h"
int LEV = 0;   
int func_size; 

void semantic_error(int line, char *msg1, char *msg2){
    printf("第%d行：%s%s\n", line, msg1, msg2);
}

int searchSymbolTable(char *name)
{
    int i;
    for (i = symbolTable.index - 1; i >= 0; i--)
        if (!strcmp(symbolTable.symbols[i].name, name))
            return i;
    return -1;
}

int fillSymbolTable(char *name, char *alias, int level, int type, char flag, int offset)
{
    int i;
    /*符号查重，考虑外部变量声明前有函数定义，
    其形参名还在符号表中，这时的外部变量与前函数的形参重名是允许的*/
    for (i = symbolTable.index - 1; symbolTable.symbols[i].level == level || (level == 0 && i >= 0); i--)
    {
        if (level == 0 && symbolTable.symbols[i].level == 1)
            continue; //外部变量和形参不必比较重名
        if (!strcmp(symbolTable.symbols[i].name, name))
            return -1;
    }
    //填写符号表内容
    strcpy(symbolTable.symbols[symbolTable.index].name, name);
    strcpy(symbolTable.symbols[symbolTable.index].alias, alias);
    symbolTable.symbols[symbolTable.index].level = level;
    symbolTable.symbols[symbolTable.index].type = type;
    symbolTable.symbols[symbolTable.index].flag = flag;
    symbolTable.symbols[symbolTable.index].offset = offset;
    return symbolTable.index++; //返回的是符号在符号表中的位置序号，中间代码生成时可用序号取到符号别名
}

int temp_add(char *name, int level, int type, char flag, int offset)
{
    strcpy(symbolTable.symbols[symbolTable.index].name, "");
    strcpy(symbolTable.symbols[symbolTable.index].alias, name);
    symbolTable.symbols[symbolTable.index].level = level;
    symbolTable.symbols[symbolTable.index].type = type;
    symbolTable.symbols[symbolTable.index].flag = flag;
    symbolTable.symbols[symbolTable.index].offset = offset;
    return symbolTable.index++; //返回的是临时变量在符号表中的位置序号
}

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

void boolExp(struct node *T)
{
    struct opn opn1, opn2, result;
    int op;
    int rtn;
    if (T)
    {
        switch (T->kind)
        {
            case INT:
                if (T->type_int != 0)
                    T->code = genGoto(T->Etrue);
                else
                    T->code = genGoto(T->Efalse);
                T->width = 0;
                break;
            case FLOAT:
                if (T->type_float != 0.0)
                    T->code = genGoto(T->Etrue);
                else
                    T->code = genGoto(T->Efalse);
                T->width = 0;
                break;
            case ID: //查符号表,获得符号表中的位置(不支持函数调用)
                rtn = searchSymbolTable(T->type_id);
                if (rtn == -1)
                    semantic_error(T->pos, T->type_id, "变量未定义，语义错误");
                if (symbolTable.symbols[rtn].flag == 'F'){
                    semantic_error(T->pos, T->type_id, "是函数名，不是普通变量，语义错误");
                }
                else
                {
                    opn1.kind = ID;
                    strcpy(opn1.id, symbolTable.symbols[rtn].alias);
                    opn1.offset = symbolTable.symbols[rtn].offset;
                    opn2.kind = INT;
                    opn2.const_int = 0;
                    result.kind = ID;
                    strcpy(result.id, T->Etrue);
                    T->code = genIR(NEQ, opn1, opn2, result);
                    T->code = merge(2, T->code, genGoto(T->Efalse));
                }
                T->width = 0;
                break;
            case RELOP: //处理关系运算表达式,2个操作数都按基本表达式处理
                T->ptr[0]->offset = T->ptr[1]->offset = T->offset;
                Exp(T->ptr[0]);
                T->width = T->ptr[0]->width;
                Exp(T->ptr[1]);
                if (T->width < T->ptr[1]->width)
                    T->width = T->ptr[1]->width;
                opn1.kind = ID;
                strcpy(opn1.id, symbolTable.symbols[T->ptr[0]->place].alias);
                opn1.offset = symbolTable.symbols[T->ptr[0]->place].offset-8;
                opn2.kind = ID;
                strcpy(opn2.id, symbolTable.symbols[T->ptr[1]->place].alias);
                opn2.offset = symbolTable.symbols[T->ptr[1]->place].offset-8;
                result.kind = ID;
                strcpy(result.id, T->Etrue);
                if (strcmp(T->type_id, "<") == 0)
                    op = JLT;
                else if (strcmp(T->type_id, "<=") == 0)
                    op = JLE;
                else if (strcmp(T->type_id, ">") == 0)
                    op = JGT;
                else if (strcmp(T->type_id, ">=") == 0)
                    op = JGE;
                else if (strcmp(T->type_id, "==") == 0)
                    op = EQ;
                else if (strcmp(T->type_id, "!=") == 0)
                    op = NEQ;
                T->code = genIR(op, opn1, opn2, result);
                T->code = merge(4, T->ptr[0]->code, T->ptr[1]->code, T->code, genGoto(T->Efalse));
                break;
            case AND:
            case OR:
                if (T->kind == AND)
                {
                    strcpy(T->ptr[0]->Etrue, newLabel());
                    strcpy(T->ptr[0]->Efalse, T->Efalse);
                }
                else
                {
                    strcpy(T->ptr[0]->Etrue, T->Etrue);
                    strcpy(T->ptr[0]->Efalse, newLabel());
                }
                strcpy(T->ptr[1]->Etrue, T->Etrue);
                strcpy(T->ptr[1]->Efalse, T->Efalse);
                T->ptr[0]->offset = T->ptr[1]->offset = T->offset;
                boolExp(T->ptr[0]);
                T->width = T->ptr[0]->width;
                boolExp(T->ptr[1]);
                if (T->width < T->ptr[1]->width)
                    T->width = T->ptr[1]->width;
                if (T->kind == AND)
                    T->code = merge(3, T->ptr[0]->code, genLabel(T->ptr[0]->Etrue), T->ptr[1]->code);
                else
                    T->code = merge(3, T->ptr[0]->code, genLabel(T->ptr[0]->Efalse), T->ptr[1]->code);
                break;
            case NOT:
                strcpy(T->ptr[0]->Etrue, T->Efalse);
                strcpy(T->ptr[0]->Efalse, T->Etrue);
                boolExp(T->ptr[0]);
                T->code = T->ptr[0]->code;
                break;
            case FUNC_CALL:
                semantic_error(T->pos, T->type_id, "暂时不支持函数调用");
                break;
            default:
                break;
        }
    }
}

//处理基本表达式
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
                char_exp(T);
                break;
            case INT:
                int_exp(T);
                break;
            case FLOAT:
                float_exp(T);
                break;
            case STRING:
                string_exp(T);
                break;
            case ASSIGNOP:
                assignop_exp(T);
                break;
            case PPLUS:
            case MMINUS:
                oop_exp(T);
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
                op_exp(T);
                break;
            case FUNC_CALL: //根据T->type_id查出函数的定义，如果语言中增加了实验教材的read，write需要单独处理一下
                func_call_exp(T);
                break;
            case ARGS: //此处仅处理各实参表达式的求值的代码序列，不生成ARG的实参系列
                args_exp(T);
                break;
            case EXP_ARRAY:
                break;
            case EXP_ELE:
                struct_exp(T);
                break;
            case EXP_IDS:
            case EXP_IDA:
                id_exp(T);
                break;
        }
    }
}

//对抽象语法树的先根遍历,按display的控制结构修改完成符号表管理和语义检查和TAC生成（语句部分）
void semantic_Analysis(struct node *T){
    if (T)
    {
        switch (T->kind)
        {
        case EXT_DEF_LIST:
            ext_def_list(T);
            break;
        case EXT_VAR_DEF: //处理外部说明,将第一个孩子(TYPE结点)中的类型送到第二个孩子的类型域
            ext_var_def(T);
            break;
        case FUNC_DEF:
            func_def(T);
            break;
        case FUNC_DEC: //根据返回类型，函数名填写符号表
            func_dec(T);
            break;
        case STRUCT_DEF:
            struct_def(T);
            break;
        case PARAM_LIST: //处理函数形式参数列表
            param_list(T);
            break;
        case PARAM_DEC:
            param_dec(T);
            break;
        case COMP_STM:
            comp_stm(T);
            break;
        case DEF_LIST:
            def_list(T);
            break;
        case VAR_DEF: // TYPE || DEC_LIST
            var_def(T);
            break;
        case STM_LIST:
            stmt_list(T);
            break;
        case IF_THEN:
            if_then(T);
            break; //控制语句都还没有处理offset和width属性
        case IF_THEN_ELSE:
            if_then_else(T);
            break;
        case WHILE:
            while_dec(T);
            break;
        case FOR:
            for_dec(T);
            break; 
        case FOR_DEC:
            for_list(T);
            break;          
        case EXP_STMT:
            exp_stmt(T);
            break;
        case RETURN:
            return_dec(T);
            break;
        case ID:
        case IDS:
        case STRUCT_TAG:
        case INT:
        case FLOAT:
        case CHAR:
        case STRING:
        case ASSIGNOP:
        case AND:
        case OR:
        case RELOP:
        case PLUS:
        case PPLUS:
        case PLUSASSIGNOP:
        case MINUS:
        case MMINUS:
        case MINUSASSIGNOP:
        case STAR:
        case STARASSIGNOP:
        case DIV:
        case DIVASSIGNOP:
        case NOT:
        case UMINUS:
        case FUNC_CALL:
        case EXP_ARRAY:
            Exp(T); //处理基本表达式
            break;
        }
    }
}

char* check_type(int type_int,char* type_str){
    if(type_int==INT)
        strcpy(type_str,"int");
    else if(type_int==FLOAT)
        strcpy(type_str,"float");
    else
        strcpy(type_str,"char");
    return type_str;
}

int get_typelen(int type_int){
    if(type_int==INT)
        return 8;
    else if(type_int==FLOAT)
        return 8;
    else
        return 1;
}

void ShowSymbolTable(struct node *T){
    int i,j,z,n,sum,offset;
    char *msg = global_buf;
    char type[8];
    if(fd==NULL){
        fd = fopen("./inter.txt","w");
    }
    printf("\n\n\t\tSymbol Table\n");
    printf("-------------------------------------------------------------------\n");
    printf("%s\t%-15s\t%s\t%s\t%s\t%s\t%s\n","Index","Name","Level","Type","Flag","Num","Array");
    printf("-------------------------------------------------------------------\n");
    for(i=0;i<symbolTable.index;i++){
        printf("%d\t",i);
        printf("%-15s\t",symbolTable.symbols[i].name);
        printf("%d\t",symbolTable.symbols[i].level);
        if(symbolTable.symbols[i].type==INT)
            printf("%s\t","int");
        else if(symbolTable.symbols[i].type==FLOAT)
            printf("%s\t","float");
        else if(symbolTable.symbols[i].type==STRUCT)
            printf("%s\t","stru");
        else
            printf("%s\t","char");
        printf("%c\t",symbolTable.symbols[i].flag);
        if(symbolTable.symbols[i].flag=='a' || symbolTable.symbols[i].flag=='A')
            printf("%d\t",symbolTable.symbols[i].paramnum-1);
        else if(symbolTable.symbols[i].flag!='l')
            printf("%d\t",symbolTable.symbols[i].paramnum);
        if(symbolTable.symbols[i].flag=='a' || symbolTable.symbols[i].flag=='A'){
            for(j=0;j<symbolTable.symbols[i].paramnum-1;j++){
                printf("%d",symbolTable.symbols[i].array[j+1]);
                if(symbolTable.symbols[i].array[j+2]!=0)
                    printf("*");
            }
        }
        printf("\n");
    }
    printf("-------------------------------------------------------------------\n");
    printf("\n");
}

void ShowIR(struct node *T){
    int i,j,z,n,sum,offset;
    char *msg = global_buf;
    char type[8];

    for(z=0;z<symbolTable.index;z++){
        if(symbolTable.symbols[z].flag == 'S'){
            sprintf(msg,"STRUCT %s :\n", symbolTable.symbols[z].name);
            print_io(msg,fd);

            n = strlen(symbolTable.symbols[z].name);
            for(i=0;i<z;i++){
                if(!strncmp(symbolTable.symbols[i].name, symbolTable.symbols[z].name, n)){
                    offset = symbolTable.symbols[i].offset;
                    if(symbolTable.symbols[i].flag == 'A'){
                        sum = 1;
                        for(j=0;j<symbolTable.symbols[i].paramnum-1;j++){
                            sum *= symbolTable.symbols[i].array[j+1];
                        }
                        sprintf(msg,"  ARRAY %s(%s)[%d]<+%d>\n", &symbolTable.symbols[i].name[n],check_type(symbolTable.symbols[i].type,type),sum,offset);
                        print_io(msg,fd);
                    }
                    else if(symbolTable.symbols[i].flag == 'V'){
                        sprintf(msg,"  VAR %s(%s)<+%d>\n", &symbolTable.symbols[i].name[n],check_type(symbolTable.symbols[i].type,type),offset);
                        print_io(msg,fd);
                    }
                    else if(symbolTable.symbols[i].flag == 'P'){
                        sprintf(msg,"  POINTER %s(*%s)<+%d>\n", &symbolTable.symbols[i].name[n],check_type(symbolTable.symbols[i].type,type),offset);
                        print_io(msg,fd);
                    }
                }
            }
        }
    } 

    sprintf(msg,"\n");
    print_io(msg,fd);

    for(i=0;i<symbolTable.index;i++){
        if(symbolTable.symbols[i].flag == 'a'){
            sum = 1;
            for(j=0;j<symbolTable.symbols[i].paramnum-1;j++){
                sum *= symbolTable.symbols[i].array[j+1];
            }
            sprintf(msg,"ARRAY %s(%s)[%d]\n", symbolTable.symbols[i].alias,check_type(symbolTable.symbols[i].type,type),sum);
            print_io(msg,fd);
        }
        else if(symbolTable.symbols[i].flag == 'v'){
            sprintf(msg,"VAR %s(%s)\n", symbolTable.symbols[i].alias,check_type(symbolTable.symbols[i].type,type));
            print_io(msg,fd);
        }
        else if(symbolTable.symbols[i].flag == 's'){
            sprintf(msg,"VAR %s(%s)\n", symbolTable.symbols[i].alias,symbolTable.symbols[i].stname);
            print_io(msg,fd);
        }
    }
}

//打印函数
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
    ShowSymbolTable(T);
    ShowIR(T);

    print_IR(T->code);
    fclose(fd);
}

/* inner code generation */
char *str_catch(char *s1, char *s2)
{
    static char result[10];
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

char *newAlias()
{
    static int no = 1;
    char s[10];
    snprintf(s, 10, "%d", no++);
    return str_catch("var", s);
}

char *newAliasEx()
{
    static int no = 1;
    char s[10];
    snprintf(s, 10, "%d", no++);
    return str_catch("exvar", s);
}

char *newLabel()
{
    static int no = 1;
    char s[10];
    snprintf(s, 10, "%d", no++);
    // itoa(no++, s, 10);
    return str_catch("label", s);
}

char *newTemp()
{
    static int no = 1;
    char s[10];
    snprintf(s, 10, "%d", no++);
    return str_catch("temp", s);
}

//生成一条TAC代码的结点组成的双向循环链表，返回头指针
struct codenode *genIR(int op, struct opn opn1, struct opn opn2, struct opn result)
{
    struct codenode *h = (struct codenode *)malloc(sizeof(struct codenode));
    h->op = op;
    h->opn1 = opn1;
    h->opn2 = opn2;
    h->result = result;
    h->next = h->prior = h;
    return h;
}

//生成一条标号语句，返回头指针
struct codenode *genLabel(char *label)
{
    struct codenode *h = (struct codenode *)malloc(sizeof(struct codenode));
    h->op = LABEL;
    strcpy(h->result.id, label);
    h->next = h->prior = h;
    return h;
}

//生成GOTO语句，返回头指针
struct codenode *genGoto(char *label)
{
    struct codenode *h = (struct codenode *)malloc(sizeof(struct codenode));
    h->op = GOTO;
    strcpy(h->result.id, label);
    h->next = h->prior = h;
    return h;
}

//合并多个中间代码的双向循环链表，首尾相连
struct codenode *merge(int num, ...)
{
    struct codenode *h1, *h2, *t1, *t2;
    va_list ap;//指向参数的指针
    va_start(ap, num);//宏初始化va_list变量，使其指向第一个可变参数的地址
    h1 = va_arg(ap, struct codenode *);//返回可变参数，va_arg的第二个参数是要返回的参数的类型,如果多个可变参数，依次调用va_arg获取各个参数
    while (--num > 0)
    {
        h2 = va_arg(ap, struct codenode *);
        if (h1 == NULL)
            h1 = h2;
        else if (h2)
        {
            t1 = h1->prior;
            t2 = h2->prior;
            t1->next = h2;
            t2->next = h1;
            h1->prior = t2;
            h2->prior = t1;
        }
    }
    va_end(ap);//使用va_end宏结束可变参数的获取
    return h1;
}

void print_io(char* msg,FILE* fd){
    printf(msg);
    fprintf(fd,msg);
}

void print_IR(struct codenode *head)
{
    char opnstr1[32], opnstr2[32], resultstr[32];
    char* msg = global_buf;
    char* buf = global_buf;
    char type[8];
    int rtn;
    struct codenode *h = head;
    if(fd==NULL){
        fd = fopen("./inter.txt","w");
    }
    do
    {
        memset(opnstr1,0,sizeof(opnstr1));
        memset(opnstr2,0,sizeof(opnstr2));
        memset(resultstr,0,sizeof(resultstr));
        if (h->opn1.kind == INT)
            sprintf(opnstr1, "#%d", h->opn1.const_int);
        if (h->opn1.kind == FLOAT)
            sprintf(opnstr1, "#%f", h->opn1.const_float);
        if (h->opn1.kind == ID)
            sprintf(opnstr1, "%s", h->opn1.id);
        if (h->opn1.kind == ARRAY)
            sprintf(opnstr1, "%s", h->opn1.id);
        if (h->opn1.kind == STRING)
            sprintf(opnstr1, "%s", h->opn1.id);
        if (h->opn2.kind == INT)
            sprintf(opnstr2, "#%d", h->opn2.const_int);
        if (h->opn2.kind == FLOAT)
            sprintf(opnstr2, "#%f", h->opn2.const_float);
        if (h->opn2.kind == ID)
            sprintf(opnstr2, "%s", h->opn2.id);
        if (h->opn2.kind == ARRAY)
            sprintf(opnstr2, "%s", h->opn2.id);
        if (h->opn2.kind == STRING)
            sprintf(opnstr2, "%s", h->opn2.id);
 
        sprintf(resultstr, "%s", h->result.id);


        if(h->op==NOT && h->result.type_array!=NULL){
            struct Array * temp = h->result.type_array;
            while (temp){
                num_buf=get_typelen(symbolTable.symbols[temp->kind].type);
                for(int i=0;i<temp->index-1;i++){
                    num_buf*=symbolTable.symbols[temp->kind].array[temp->index-i];
                }
                sprintf(buf,"{#%d}*{%s<+%d>}+",num_buf,temp->value.type_id,temp->offset);
                strcat(resultstr,buf);
                temp = temp->next;
            }
            resultstr[strlen(resultstr)-1] = 0;
            sprintf(msg,"#!tempa := %s\n", resultstr);
            print_io(msg,fd);  
            is_arrVar = 1;
        }
    
        if(!strncmp(resultstr,"var",3)){
            if(h->result.offset>=0)
                sprintf(resultstr,"%s<+%d>",resultstr,h->result.offset);
            else
                sprintf(resultstr,"%s<-%d>",resultstr,-h->result.offset);
            if(is_arrVar == 1){
                strcat(resultstr,"<+tempa>");
            }
        }

        if(!strncmp(opnstr1,"var",3)){
            if(h->opn1.offset>=0)
                sprintf(opnstr1,"%s<+%d>",opnstr1,h->opn1.offset);
            else
                sprintf(opnstr1,"%s<-%d>",opnstr1,-h->opn1.offset);
            if(is_arrVar == 1){
                strcat(opnstr1,"<+tempa>");
            }
        }    

        if(!strncmp(opnstr2,"var",3)){
            if(h->opn2.offset>=0)
                sprintf(opnstr2,"%s<+%d>",opnstr2,h->opn2.offset);
            else
                sprintf(opnstr2,"%s<-%d>",opnstr2,-h->opn2.offset);
            if(is_arrVar == 1){
                strcat(opnstr2,"<+tempa>");
            }
        }

        memset(msg,0,sizeof(msg));
        switch (h->op)
        {
        case ASSIGNOP:
            sprintf(msg,"  %s := %s\n", resultstr, opnstr1);
            print_io(msg,fd);
            break;
        case PPLUS:
        case MMINUS:
            sprintf(msg,"  %s := %s %c %s\n", resultstr, opnstr1,h->op == PPLUS ? '+' : '-' ,opnstr2);
            print_io(msg,fd);
            break;
        case PLUS:
        case MINUS:
        case STAR:
        case DIV:
            sprintf(msg,"  %s := %s %c %s\n",resultstr,opnstr1,h->op == PLUS ? '+' : h->op == MINUS ? '-' : h->op == STAR ? '*' : '\\',opnstr2);
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
            rtn = searchSymbolTable(h->result.id);
            sprintf(msg,"\nFUNCTION %s - %d :\n", h->result.id,symbolTable.symbols[rtn].offset);
            print_io(msg,fd);
            break;
        case ARRAY:
            sprintf(msg,"  ARRAY %s\n", resultstr);
            print_io(msg,fd);
            break;
        case STRUCT:
            sprintf(msg,"  STRUCT %s\n", h->result.id);
            print_io(msg,fd);
            break;
        case IDS:
            sprintf(msg,"  POINTER %s\n", h->result.id);
            print_io(msg,fd);   
            break;
        case PARAM:
            sprintf(msg,"  PARAM %s\n", resultstr);
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
            sprintf(msg,"  ARG %s\n", resultstr);
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

void string_exp(struct node* T){
    struct opn opn1, opn2, result;
    T->place = temp_add(newTemp(), LEV, T->type, 't', T->offset); //为字符串常量生成一个临时变量
    T->type = STRING;
    opn1.kind = STRING;
    strcpy(opn1.id,T->type_string);
    printf("%s\n",T->type_string);
    result.kind = ID;
    strcpy(result.id, symbolTable.symbols[T->place].alias);
    result.offset = symbolTable.symbols[T->place].offset;
    T->code = genIR(ASSIGNOP, opn1, opn2, result);
    T->width = strlen(T->type_string);
}

void id_exp(struct node *T){
    int rtn;

    rtn = searchSymbolTable(T->type_id);
    if (rtn == -1){
        semantic_error(T->pos, T->type_id, "变量未声明定义就引用，语义错误");
    }
    if (symbolTable.symbols[rtn].flag == 'F'){
        semantic_error(T->pos, T->type_id, "是函数名，不是普通变量，语义错误");
    }
    else{
        T->place = rtn; 
        T->code = NULL;
        T->type = symbolTable.symbols[rtn].type;
        T->offset = symbolTable.symbols[rtn].offset;
        T->width = 0; 
        strcpy(T->struct_name,symbolTable.symbols[rtn].stname);
    }
}

void float_exp(struct node *T){
    struct opn opn1, opn2, result;
    T->place = temp_add(newTemp(), LEV, T->type, 't', T->offset); //为整常量生成一个临时变量
    T->type = FLOAT;
    opn1.kind = FLOAT;
    opn1.const_float = T->type_float;
    result.kind = ID;
    strcpy(result.id, symbolTable.symbols[T->place].alias);
    result.offset = symbolTable.symbols[T->place].offset;
    T->code = genIR(ASSIGNOP, opn1, opn2, result);
    T->width = 8;
}

void int_exp(struct node *T){
    struct opn opn1, opn2, result;
    T->place = temp_add(newTemp(), LEV, T->type, 't', T->offset); //为整常量生成一个临时变量
    T->type = INT;
    opn1.kind = INT;
    opn1.const_int = T->type_int;
    result.kind = ID;
    strcpy(result.id, symbolTable.symbols[T->place].alias);
    result.offset = symbolTable.symbols[T->place].offset;
    T->code = genIR(ASSIGNOP, opn1, opn2, result);
    T->width = 8;
}

void char_exp(struct node *T){
    struct opn opn1, opn2, result;
    T->place = temp_add(newTemp(), LEV, T->type, 't', T->offset); 
    T->type = CHAR;
    T->width = 1;
}

int array_exp(struct node *T,struct node **Tr,int *sum,int key){
    struct node *T0;
    char arr[16];
    struct Array *head;
    struct Array *temp;
    int i=0,num,use_struct=0;
    
    T0 = T->ptr[key];
    head = (struct Array *)malloc(sizeof(struct Array));
    if(head==NULL){
        puts("malloc error");
        exit(-1);
    }
    while (T0->kind == EXP_ARRAY) /* Exp,ArrayList */
    {
        Exp(T0->ptr[1]->ptr[0]);
        if(T0->ptr[1]->ptr[0]->kind == INT){
            arr[i] = T0->ptr[1]->ptr[0]->type_int;
            i++;
        }
        else if(T0->ptr[1]->ptr[0]->kind == ID){
            temp = (struct Array *)malloc(sizeof(struct Array));
            temp->index=i+1;
            strcpy(temp->value.type_id,symbolTable.symbols[T0->ptr[1]->ptr[0]->place].alias);
            strcpy(temp->name,symbolTable.symbols[T0->ptr[1]->ptr[0]->place].name);
            temp->next = head->next;
            head->next = temp;
            arr[i]=0;
            i++;
        }
        T0 = T0->ptr[0];
    }
    T0->offset = T->offset;
    T0->type_array = head->next;
    *Tr = T0;

    Exp(T0);
    if(T0->kind == EXP_ELE){
        T0 = T0->ptr[1];
        use_struct = 1;
    }
    if(i!=symbolTable.symbols[T0->place].paramnum-1){
        semantic_error(T->pos, "", "多级数组的阶数不匹配");
    }
    T->type = T0->type;
    T->width = T0->width;
    T->code = T0->code;
    temp = head;
    while (temp){
        temp->kind = T0->place;
        temp = temp->next;
    }
    
    *sum = 0;
    for(int x=0;x<i;x++){
        num = arr[x];
        for(int y=0;y<x;y++){
            num *= symbolTable.symbols[T0->place].array[symbolTable.symbols[T0->place].paramnum-y-1];
        }
        *sum += num;
    }   
    
    T->code = merge(2, T->code, T->ptr[1-key]->code);

    return use_struct;
}

void struct_exp(struct node *T){
    int rtn;

    Exp(T->ptr[0]);

    sprintf(global_buf,"%s.%s",T->ptr[0]->struct_name,T->ptr[1]->type_id);
    rtn = searchSymbolTable(global_buf);
    if (rtn == -1){
        semantic_error(T->ptr[0]->pos, T->ptr[0]->struct_name, "结构体没有目标条目，语义错误");
        return;
    }
    
    T->ptr[1]->place = rtn;
}

void array_exp_tmp(struct node *T,struct node *T0){
    struct opn opn1, opn2, result;
    struct Array * temp = T0->type_array;
    int rtn;

    memset(&result,0,sizeof(result));
    result.type_array = temp;

    while (temp){
        rtn = searchSymbolTable(temp->name);
        if (rtn == -1){
            semantic_error(T->ptr[0]->pos, temp->name, "未定义，语义错误");
            return;
        }
        temp->offset = symbolTable.symbols[rtn].offset-8;
        temp = temp->next;
    }
    T->code = merge(2, T->code, genIR(NOT, opn1, opn2, result));
}

void array_exp_left(struct node *T,struct opn *result){
    struct node *T0;
    int len=1;
    int sum=-1;

    if(array_exp(T,&T0,&sum,0)){        
        (*result).kind = ARRAY;
        sprintf((*result).id,"%s(%s[%d])<+%d>",symbolTable.symbols[T0->ptr[0]->place].alias,
            symbolTable.symbols[T0->ptr[1]->place].name,sum,
            symbolTable.symbols[T0->ptr[1]->place].offset+sum*get_typelen(symbolTable.symbols[T0->ptr[1]->place].type));
        (*result).offset = symbolTable.symbols[T0->place].offset;
        (*result).type_array = T0->type_array;
    }
    else{
        (*result).kind = ARRAY;
        sprintf((*result).id,"%s",symbolTable.symbols[T0->place].alias);
        (*result).offset = symbolTable.symbols[T0->place].offset;

        for(int i=0;i<symbolTable.symbols[T0->place].paramnum-1;i++){
            len *= symbolTable.symbols[T0->place].array[i+1];
        }
        (*result).offset -= len*get_typelen(symbolTable.symbols[T0->place].type);
        (*result).offset += sum*get_typelen(symbolTable.symbols[T0->place].type);
        (*result).type_array = T0->type_array;
    }
    array_exp_tmp(T,T0);
}

void array_exp_right(struct node *T,struct opn *opn1){
    struct node *T0;
    int len=1;
    int sum=-1;

    if(array_exp(T,&T0,&sum,1)){
        (*opn1).kind = ARRAY;
        sprintf((*opn1).id,"%s(%s[%d])<+%d>",symbolTable.symbols[T0->ptr[0]->place].alias,symbolTable.symbols[T0->ptr[1]->place].name,
            sum,symbolTable.symbols[T0->ptr[1]->place].offset+sum*get_typelen(symbolTable.symbols[T0->ptr[1]->place].type));
        (*opn1).offset = symbolTable.symbols[T0->place].offset;
        (*opn1).type_array = T0->type_array;
    }
    else{
        (*opn1).kind = ARRAY;
        sprintf((*opn1).id,"%s",symbolTable.symbols[T0->place].alias);
        (*opn1).offset = symbolTable.symbols[T0->place].offset;

        for(int i=0;i<symbolTable.symbols[T0->place].paramnum-1;i++){
            len *= symbolTable.symbols[T0->place].array[i+1];
        }
        (*opn1).offset -= len*get_typelen(symbolTable.symbols[T0->place].type);
        (*opn1).offset += sum*get_typelen(symbolTable.symbols[T0->place].type);
        (*opn1).type_array = T0->type_array;
    }
    array_exp_tmp(T,T0);
}

void struct_exp_left(struct node *T,struct opn *result){
    Exp(T->ptr[0]);
    Exp(T->ptr[1]);

    (*result).kind = ID;
    sprintf((*result).id,"%s(%s)<+%d>",symbolTable.symbols[T->ptr[0]->ptr[0]->place].alias,
        symbolTable.symbols[T->ptr[0]->ptr[1]->place].name,symbolTable.symbols[T->ptr[0]->ptr[1]->place].offset);
    (*result).offset = symbolTable.symbols[T->ptr[0]->ptr[0]->place].offset;
}

void struct_exp_right(struct node *T,struct opn *opn1){
    Exp(T->ptr[1]);
    Exp(T->ptr[0]);

    (*opn1).kind = ID;
    sprintf((*opn1).id,"%s(%s)<+%d>",symbolTable.symbols[T->ptr[1]->ptr[0]->place].alias,
        symbolTable.symbols[T->ptr[1]->ptr[1]->place].name,symbolTable.symbols[T->ptr[1]->ptr[1]->place].offset);
    (*opn1).offset = symbolTable.symbols[T->ptr[1]->ptr[0]->place].offset;
}

void id_exp_left(struct node *T,struct opn *result){
    Exp(T->ptr[0]);
    (*result).kind = ID;
    strcpy((*result).id, symbolTable.symbols[T->ptr[0]->place].alias);
    (*result).offset = symbolTable.symbols[T->ptr[0]->place].offset-8;
}

void id_exp_right(struct node *T,struct opn *opn1){
    Exp(T->ptr[1]);
    (*opn1).kind = ID;
    strcpy((*opn1).id, symbolTable.symbols[T->ptr[1]->place].alias);
    (*opn1).offset = symbolTable.symbols[T->ptr[1]->place].offset-8;  
}

void ids_exp_left(struct node *T,struct opn *result){
    Exp(T->ptr[0]);
    if(symbolTable.symbols[T->ptr[0]->place].flag != 'p' || symbolTable.symbols[T->ptr[0]->place].flag != 'P'){
        semantic_error(T->pos, symbolTable.symbols[T->ptr[1]->place].name, "不是指针");
    }
    (*result).kind = ID;
    sprintf((*result).id, "*(%s)",symbolTable.symbols[T->ptr[0]->place].alias);
    (*result).offset = symbolTable.symbols[T->ptr[0]->place].offset;
}

void ids_exp_right(struct node *T,struct opn *opn1){
    Exp(T->ptr[1]);
    if(symbolTable.symbols[T->ptr[1]->place].flag != 'p' || symbolTable.symbols[T->ptr[1]->place].flag != 'P'){
        semantic_error(T->pos, symbolTable.symbols[T->ptr[1]->place].name, "不是指针");
    }
    (*opn1).kind = ID;
    sprintf((*opn1).id,"*(%s)",symbolTable.symbols[T->ptr[1]->place].alias); 
    (*opn1).offset = symbolTable.symbols[T->ptr[1]->place].offset;
}
            
void ida_exp_right(struct node *T,struct opn *opn1){
    Exp(T->ptr[1]);
    (*opn1).kind = ID;
    sprintf((*opn1).id,"&(%s)",symbolTable.symbols[T->ptr[1]->place].alias); 
    (*opn1).offset = symbolTable.symbols[T->ptr[1]->place].offset;
}

void assignop_exp_left(struct node *T,struct opn *result){
    if(T->ptr[0]->kind == EXP_ARRAY){
        array_exp_left(T,result);
    }
    else if(T->ptr[0]->kind == EXP_ELE){
        struct_exp_left(T,result);
    }
    else if (T->ptr[0]->kind == ID){
        id_exp_left(T,result);
    }
    else if(T->ptr[0]->kind == EXP_IDS){
        ids_exp_left(T,result);
    }
    else{
        semantic_error(T->pos, "", "赋值语句左值错误");
    }
}

void assignop_exp_right(struct node *T,struct opn *opn1){
    if(T->ptr[1]->kind == EXP_ARRAY){
        array_exp_right(T,opn1);
    }
    else if(T->ptr[1]->kind == EXP_ELE){
        struct_exp_right(T,opn1);
    }
    else if(T->ptr[1]->kind == ID){
        id_exp_right(T,opn1);
    }
    else if(T->ptr[1]->kind == EXP_IDA){
        ida_exp_right(T,opn1);
    }
    else if(T->ptr[1]->kind == EXP_IDS){
        ids_exp_right(T,opn1);
    }
    else if(T->ptr[1] == NULL){
        semantic_error(T->pos, "", "赋值语句右值错误");
    }
    else{
        Exp(T->ptr[1]);
        (*opn1).kind = ID;
        strcpy((*opn1).id, symbolTable.symbols[T->ptr[1]->place].alias);
        (*opn1).offset = symbolTable.symbols[T->ptr[1]->place].offset;
        T->code = merge(2, T->code, T->ptr[1]->code);
    }
}

void assignop_exp(struct node *T)
{
    struct opn opn1, opn2, result;
    memset(&opn1,0,sizeof(struct opn));
    memset(&opn2,0,sizeof(struct opn));
    memset(&result,0,sizeof(struct opn));

    assignop_exp_left(T,&result);
    assignop_exp_right(T,&opn1);

    T->code = merge(2, T->code, genIR(ASSIGNOP, opn1, opn2, result));
}

void relop_exp(struct node *T)
{
    T->type = INT;
    T->ptr[0]->offset = T->ptr[1]->offset = T->offset;
    Exp(T->ptr[0]);
    Exp(T->ptr[1]);
}

void args_exp(struct node *T)
{
    T->ptr[0]->offset = T->offset;
    Exp(T->ptr[0]);
    T->width = T->ptr[0]->width;
    T->code = T->ptr[0]->code;
    if (T->ptr[1])
    {
        T->ptr[1]->offset = T->offset + T->ptr[0]->width;
        Exp(T->ptr[1]);
        T->width += T->ptr[1]->width;
        T->code = merge(2, T->code, T->ptr[1]->code);
    }
}

void oop_exp(struct node *T){
    struct opn opn1, opn2, result;
    T->ptr[0]->offset = T->offset;
    Exp(T->ptr[0]);

    T->place = temp_add(newTemp(), LEV, T->type, 't', T->offset);
    opn1.kind = INT;
    opn1.const_int = 1;
    result.kind = ID;
    strcpy(result.id, symbolTable.symbols[T->place].alias);
    result.offset = symbolTable.symbols[T->place].offset;
    T->code = genIR(ASSIGNOP, opn1, opn2, result);

    T->type = INT, T->width = T->ptr[0]->width;
    result.kind = ID;
    strcpy(result.id, symbolTable.symbols[T->ptr[0]->place].alias);
    result.type = T->ptr[0]->type;
    result.offset = symbolTable.symbols[T->ptr[0]->place].offset-8;

    opn1.kind = ID;
    strcpy(opn1.id, symbolTable.symbols[T->ptr[0]->place].alias);
    opn1.type = T->ptr[0]->type;
    opn1.offset = symbolTable.symbols[T->ptr[0]->place].offset-8;

    opn2.kind = ID;
    strcpy(opn2.id, symbolTable.symbols[T->place].alias);

    T->code = merge(3, T->ptr[0]->code, T->code, genIR(T->kind, opn1, opn2, result));
    T->width = T->ptr[0]->width;
}

void op_exp(struct node *T)
{
    struct opn opn1, opn2, result;
    T->ptr[0]->offset = T->offset;
    Exp(T->ptr[0]);
    T->ptr[1]->offset = T->offset + T->ptr[0]->width;
    Exp(T->ptr[1]);

    T->type = INT, T->width = T->ptr[0]->width + T->ptr[1]->width + 2;
    T->place = temp_add(newTemp(), LEV, T->type, 't', T->offset + T->ptr[0]->width + T->ptr[1]->width);
    opn1.kind = ID;
    strcpy(opn1.id, symbolTable.symbols[T->ptr[0]->place].alias);
    opn1.type = T->ptr[0]->type;
    opn1.offset = symbolTable.symbols[T->ptr[0]->place].offset-8;
    opn2.kind = ID;
    strcpy(opn2.id, symbolTable.symbols[T->ptr[1]->place].alias);
    opn2.type = T->ptr[1]->type;
    opn2.offset = symbolTable.symbols[T->ptr[1]->place].offset-8;
    result.kind = ID;
    strcpy(result.id, symbolTable.symbols[T->place].alias);
    result.type = T->type;
    result.offset = symbolTable.symbols[T->place].offset-8;
    T->code = merge(3, T->ptr[0]->code, T->ptr[1]->code, genIR(T->kind, opn1, opn2, result));
    T->width = T->ptr[0]->width + T->ptr[1]->width + 8;
}

void func_call_exp(struct node *T)
{
    int rtn,width;
    struct node *T0;
    struct opn opn1, opn2, result;
    rtn = searchSymbolTable(T->type_id);
    if (rtn == -1)
    {
        semantic_error(T->pos, T->type_id, "函数未定义，语义错误");
        return;
    }
    if (symbolTable.symbols[rtn].flag != 'F')
    {
        semantic_error(T->pos, T->type_id, "不是函数名，不能进行函数调用，语义错误");
        return;
    }
    T->type = symbolTable.symbols[rtn].type;
    width = 8;
    if (T->ptr[0])
    {
        T->ptr[0]->offset = T->offset;
        Exp(T->ptr[0]);                      
        T->width = T->ptr[0]->width + width; 
        T->code = T->ptr[0]->code;
    }
    else
    {
        T->width = width;
        T->code = NULL;
    }
    match_param(rtn, T->ptr[0]);
    T0 = T->ptr[0]; 
    while (T0)
    {
        result.kind = ID;
        strcpy(result.id, symbolTable.symbols[T0->ptr[0]->place].alias);
        result.offset = symbolTable.symbols[T0->ptr[0]->place].offset-8;
        T->code = merge(2, T->code, genIR(ARG, opn1, opn2, result));
        T0 = T0->ptr[1];
    }
    T->place = temp_add(newTemp(), LEV, T->type, 't', T->offset + T->width - width);
    opn1.kind = ID;
    strcpy(opn1.id, T->type_id); //保存函数名
    opn1.offset = rtn;           //这里offset用以保存函数定义入口,在目标代码生成时，能获取相应信息
    result.kind = ID;
    strcpy(result.id, symbolTable.symbols[T->place].alias);
    result.offset = symbolTable.symbols[T->place].offset;
    T->code = merge(2, T->code, genIR(CALL, opn1, opn2, result)); //生成函数调用中间代码
}

void not_exp(struct node *T)
{
    T->type = INT;
    T->ptr[0]->offset = T->offset;
    Exp(T->ptr[0]);
}

void ext_var_list(struct node *T,int is_arr) /* ID,EXT_DEC_LIST | ARRAY_DEC,EXT_DEC_LIST */
{
    int rtn, num = 1;
    switch (T->kind)
    {
    case EXT_DEC_LIST:
        T->ptr[0]->type = T->type;                
        T->ptr[0]->offset = T->offset;            
        T->ptr[1]->type = T->type;                
        T->ptr[1]->offset = T->offset + T->width; 
        T->ptr[1]->width = T->width;
        ext_var_list(T->ptr[0],0);
        ext_var_list(T->ptr[1],0);
        T->num = T->ptr[1]->num + 1;
        break;
    case ID:
        if(is_arr==1){
            rtn = fillSymbolTable(T->type_id, newAliasEx(), LEV, T->type, 'a', T->offset); 
        }
        else{
            if(T->type == STRUCT){
                rtn = fillSymbolTable(T->type_id, newAliasEx(), LEV, T->type, 's', T->offset); 
                strcpy(symbolTable.symbols[rtn].stname,T->struct_name);
            }
            else{
                rtn = fillSymbolTable(T->type_id, newAliasEx(), LEV, T->type, 'v', T->offset); 
            }
        }
        if (rtn == -1)
            semantic_error(T->pos, T->type_id, "变量重复定义，语义错误");
        else
            T->place = rtn;
        T->num = 1;
        symbolTable.symbols[T->place].paramnum = 1;
        break;
    case IDS:
        rtn = fillSymbolTable(T->type_id, newAliasEx(), LEV, T->type, 'p', T->offset); 
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
        T->place = T->ptr[0]->place;
        symbolTable.symbols[T->ptr[0]->place].paramnum += 1;
        symbolTable.symbols[T->ptr[0]->place].array[symbolTable.symbols[T->ptr[0]->place].paramnum-1] = T->ptr[1]->ptr[0]->type_int;
        break;
    default:
        break;
    }
}

void ext_def_list(struct node *T)
{
    if (!T->ptr[0])
        return;

    T->ptr[0]->offset = T->offset;
    semantic_Analysis(T->ptr[0]); 
    T->code = T->ptr[0]->code;

    if (T->ptr[1])
    {
        T->ptr[1]->offset = T->ptr[0]->offset + T->ptr[0]->width;
        semantic_Analysis(T->ptr[1]); //访问该外部定义列表中的其它外部定义
        T->code = merge(2, T->code, T->ptr[1]->code);
    }
}

void ext_var_def(struct node *T) /* TYPE,EXT_DEF_LIST | STRUCT_DEC,EXT_DEF_LIST */
{
    int rtn;
    if (!strcmp(T->ptr[0]->type_id, "int"))
    {
        T->type = T->ptr[1]->type = INT;
        T->ptr[1]->width = 8;
    }
    if (!strcmp(T->ptr[0]->type_id, "float"))
    {
        T->type = T->ptr[1]->type = FLOAT;
        T->ptr[1]->width = 8;
    }
    if (!strcmp(T->ptr[0]->type_id, "char"))
    {
        T->type = T->ptr[1]->type = CHAR;
        T->ptr[1]->width = 1;
    }

    if(T->ptr[0]->kind == STRUCT_DEC){
        T->type = T->ptr[1]->type = STRUCT;
        rtn = searchSymbolTable(T->ptr[0]->ptr[0]->struct_name);
        if (rtn == -1)
            semantic_error(T->pos, T->type_id, "结构体未定义，语义错误");
        if (symbolTable.symbols[rtn].flag == 'F'){
            semantic_error(T->pos, T->type_id, "是函数名，不是结构体，语义错误");
        }
        else if (symbolTable.symbols[rtn].flag == 'v'){
            semantic_error(T->pos, T->type_id, "是变量，不是结构体，语义错误");
        }
        strcpy(T->ptr[1]->struct_name,T->ptr[0]->ptr[0]->struct_name);
    }

    T->ptr[1]->offset = T->offset; 
    ext_var_list(T->ptr[1],0);                        
    T->width = (T->ptr[1]->width) * T->ptr[1]->num; 
    T->code = NULL;                                 
}

void func_def(struct node *T) /* TYPE,FUNC_DEC,COMP_STM */
{
    struct node *T0;
    if (!strcmp(T->ptr[0]->type_id, "int")){
        T->ptr[1]->type = INT;
    }
    if (!strcmp(T->ptr[0]->type_id, "float")){
        T->ptr[1]->type = FLOAT;
    }
    if (!strcmp(T->ptr[0]->type_id, "char")){
        T->ptr[1]->type = CHAR;
    }

    T->width = 0;                 
    T->offset = 0;                
    semantic_Analysis(T->ptr[1]);  
    stack_base = symbolTable.symbols[T->ptr[1]->place].paramnum*8;

    T->offset += T->ptr[1]->width; 
    T->ptr[2]->offset = T->offset;
    sprintf(T->ptr[2]->Snext,"%s%s","Flabel",symbolTable.symbols[T->ptr[1]->place].name);
    semantic_Analysis(T->ptr[2]);

    if(T->ptr[2]->ptr[0]!=NULL){
        T0 = T->ptr[2]->ptr[0];
        while(T0->ptr[1]!=NULL){
            T0=T0->ptr[1];
        }
        symbolTable.symbols[T->ptr[1]->place].offset = T0->offset + T0->width + stack_base;
    }else{
        symbolTable.symbols[T->ptr[1]->place].offset = 0 + stack_base;
    }

    stack_base = 0;

    T->code = merge(3, T->ptr[1]->code, T->ptr[2]->code, genLabel(T->ptr[2]->Snext)); 
}

void func_dec(struct node *T){
    int rtn;
    struct opn opn1, opn2, result;
    rtn = fillSymbolTable(T->type_id, newAlias(), LEV, T->type, 'F', 0); 
    if (rtn == -1){
        semantic_error(T->pos, T->type_id, "函数名重复使用，可能是函数重复定义，语义错误");
        return;
    }
    else{
        T->place = rtn;
    }
    result.kind = ID;
    strcpy(result.id, T->type_id);
    result.offset = rtn;
    T->code = genIR(FUNCTION, opn1, opn2, result); 
    T->offset = 8;                                
    if (T->ptr[0]) 
    { 
        T->ptr[0]->offset = T->offset;
        semantic_Analysis(T->ptr[0]); 
        T->width = T->ptr[0]->width;
        symbolTable.symbols[rtn].paramnum = T->ptr[0]->num;
        T->code = merge(2, T->code, T->ptr[0]->code); 
    }
    else{
        symbolTable.symbols[rtn].paramnum = 0;
        T->width = 0;
    }
}

void param_list(struct node *T){
    T->ptr[0]->offset = T->offset;
    semantic_Analysis(T->ptr[0]);
    if (T->ptr[1]){
        T->ptr[1]->offset = T->offset + T->ptr[0]->width;
        semantic_Analysis(T->ptr[1]);
        T->num = T->ptr[0]->num + T->ptr[1]->num;            
        T->width = T->ptr[0]->width + T->ptr[1]->width;       
        T->code = merge(2, T->ptr[0]->code, T->ptr[1]->code);
    }
    else{
        T->num = T->ptr[0]->num;
        T->width = T->ptr[0]->width;
        T->code = T->ptr[0]->code;
    }
}

void param_dec(struct node *T)
{
    int rtn;
    struct opn opn1, opn2, result;
    rtn = fillSymbolTable(T->ptr[1]->type_id, newAlias(), 1, T->ptr[0]->type, 'l', T->offset);
    if (rtn == -1)
        semantic_error(T->ptr[1]->pos, T->ptr[1]->type_id, "参数名重复定义,语义错误");
    else
        T->ptr[1]->place = rtn;
    T->num = 1;                                
    T->width = 8; 
    result.kind = ID;
    result.type = T->ptr[0]->type;
    strcpy(result.id, symbolTable.symbols[rtn].alias);
    result.offset = symbolTable.symbols[rtn].offset-8;
    T->code = genIR(PARAM, opn1, opn2, result); //生成:PARAM 形参名
}

void struct_def(struct node *T){ /* STRUCT_TAG,DEF_LIST(VAR_DEF,DEF_LIST) */
    struct node *T0,*T1;
    char name[64];
    int rtn,num=0,offset=0;

    T->code = NULL;
    T->width = 0;
    T0 = T->ptr[1];
    while (T0 != NULL)
    {
        T1 = T0->ptr[0]->ptr[1]->ptr[0];
        if(T1->kind == ARRAY_DEC){
            while (T1->kind == ARRAY_DEC){
                T1 = T1->ptr[0];
            } 
        }
        sprintf(name,"%s.%s",T->ptr[0]->struct_name,T1->type_id);
        strcpy(T1->type_id,name);
        var_def(T0->ptr[0]);
        symbolTable.symbols[T1->place].flag -= 0x20;
        T->width += T0->ptr[0]->width;
        T0 = T0->ptr[1];
        num ++;
    }

    rtn = fillSymbolTable(T->ptr[0]->struct_name, newAliasEx(), 0, STRUCT, 'S', T->offset + T->width);
    if (rtn == -1){
        semantic_error(T->ptr[0]->pos, T->ptr[0]->type_id, "变量重复定义");
    }
    else {
        symbolTable.symbols[rtn].paramnum = num;
        for(int i=rtn-num;i<rtn;i++){
            offset += symbolTable.symbols[i].offset;
            symbolTable.symbols[i].offset = offset - symbolTable.symbols[i].offset;
        }
    }
}

void comp_stm(struct node *T)
{
    LEV++;
    symbol_scope_TX.TX[symbol_scope_TX.top++] = symbolTable.index;
    T->width = 0;
    T->code = NULL;
    if (T->ptr[0])
    {
        T->offset = 0;
        T->ptr[0]->offset = T->offset;
        semantic_Analysis(T->ptr[0]); //处理该层的局部变量DEF_LIST
        T->width += T->ptr[0]->width;
        T->code = T->ptr[0]->code;
    }
    ShowSymbolTable(T);
    if (T->ptr[1])
    {
        T->ptr[1]->offset = T->offset + T->width;
        strcpy(T->ptr[1]->Snext, T->Snext); 
        semantic_Analysis(T->ptr[1]);      
        T->width += T->ptr[1]->width;
        T->code = merge(2, T->code, T->ptr[1]->code);
    }
    LEV--;                                                       
    symbolTable.index = symbol_scope_TX.TX[--symbol_scope_TX.top]; 
}

void def_list(struct node *T) /* VAR_DEF,DEF_LIST || NULL */
{
    T->code = NULL;
    if (T->ptr[0])
    {
        T->ptr[0]->offset = T->offset;
        semantic_Analysis(T->ptr[0]); //处理一个局部变量定义
        T->code = T->ptr[0]->code;
        T->width = T->ptr[0]->width;
    }
    if (T->ptr[1])
    {
        T->ptr[1]->offset = T->offset + T->ptr[0]->width;
        semantic_Analysis(T->ptr[1]); //处理剩下的局部变量定义
        T->code = merge(2, T->code, T->ptr[1]->code);
        T->width += T->ptr[1]->width;
    }
}

void var_def(struct node *T) /* TYPE,DEC_LIST */
{
    int rtn, num, width;
    struct node *T0,*T1;
    struct opn opn1, opn2, result;
    T->code = NULL;
    int use_struct;
    char arr[32];
    char type[8];

    if (!strcmp(T->ptr[0]->type_id, "int"))
    {
        T->ptr[1]->type = INT;
        width = 8;
    }
    if (!strcmp(T->ptr[0]->type_id, "float"))
    {
        T->ptr[1]->type = FLOAT;
        width = 8;
    }
    if (!strcmp(T->ptr[0]->type_id, "char"))
    {
        T->ptr[1]->type = CHAR;
        width = 1;
    }
    if (!strcmp(T->ptr[0]->type_id, "string"))
    {
        T->ptr[1]->type = STRING;
    }

    use_struct = 0;
    if(T->ptr[0]->kind == STRUCT_DEC){
        T->ptr[1]->type = STRUCT;
        rtn = searchSymbolTable(T->ptr[0]->ptr[0]->struct_name);
        if (rtn == -1){
            semantic_error(T->pos, T->type_id, "结构体未定义，语义错误");
        }
        if (symbolTable.symbols[rtn].flag == 'F'){
            semantic_error(T->pos, T->type_id, "是函数名，不是结构体，语义错误");
        }
        else if (symbolTable.symbols[rtn].flag == 'v'){
            semantic_error(T->pos, T->type_id, "是变量，不是结构体，语义错误");
        }
        use_struct = 1;
        strcpy(T->ptr[1]->struct_name,T->ptr[0]->ptr[0]->struct_name);
    }

    T->width = 0;
    T0 = T->ptr[1]; 
    num = 0;
    while (T0) /* 处理DEC_LIST结点(多个变量共享同一个类型) */
    { 
        num++;
        T0->ptr[0]->type = T0->type; //类型属性向下传递

        if (T0->ptr[0]->kind == ID || T0->ptr[0]->kind == IDS){
            if(T0->ptr[0]->kind == ID){
                rtn = fillSymbolTable(T0->ptr[0]->type_id, newAlias(), LEV, T0->ptr[0]->type, 'v', 0); 
            }
            else if(T0->ptr[0]->kind == IDS){
                rtn = fillSymbolTable(T0->ptr[0]->type_id, newAlias(), LEV, T0->ptr[0]->type, 'p', 0); 
                width = 8;
            }
            
            if (rtn == -1)
                semantic_error(T0->ptr[0]->pos, T0->ptr[0]->type_id, "变量重复定义");
            else
                T0->ptr[0]->place = rtn;

            symbolTable.symbols[rtn].paramnum = 1;
            strcpy(symbolTable.symbols[rtn].stname,T0->struct_name);

            T->width += ALIGN(width,8);
            symbolTable.symbols[rtn].offset = T->offset + T->width;
            symbolTable.symbols[rtn].offset += stack_base;

            if(use_struct){
                result.kind = ID;
                if(T0->ptr[0]->kind == ID){
                    sprintf(result.id,"%s(%s)",symbolTable.symbols[rtn].alias, T->ptr[0]->ptr[0]->struct_name);
                }
                else if(T0->ptr[0]->kind == IDS){
                    sprintf(result.id,"%s(*%s)",symbolTable.symbols[rtn].alias, T->ptr[0]->ptr[0]->struct_name);
                }
                
                result.offset = symbolTable.symbols[rtn].offset-8;
                T->code = merge(2, T->code, genIR(STRUCT, opn1, opn2, result));
            }
            else if(T0->ptr[0]->kind == IDS){
                result.kind = IDS;
                sprintf(result.id,"%s(*%s)",symbolTable.symbols[rtn].alias, check_type(symbolTable.symbols[rtn].type,&type));
                result.offset = T->offset;
                T->code = merge(2, T->code, genIR(IDS, opn1, opn2, result));
            }
        }
        else if (T0->ptr[0]->kind == ARRAY_DEC){ /* ARRAY_DEC,ARRAY_LIST */
            T1 = T0->ptr[0];
            while (T1->kind == ARRAY_DEC){
                T1 = T1->ptr[0];
            }
            T1->type = T0->type;

            rtn = fillSymbolTable(T1->type_id, newAlias(), LEV, T1->type, 'a', 0); 
            if (rtn == -1)
                semantic_error(T1->pos, T1->type_id, "变量重复定义");
            else
                T1->place = rtn;
            
            symbolTable.symbols[rtn].paramnum = 1;
            T1 = T0->ptr[0];
            while (T1->kind == ARRAY_DEC){
                arr[symbolTable.symbols[rtn].paramnum++] = T1->ptr[1]->ptr[0]->type_int;
                T1 = T1->ptr[0];
            }
            arr[0] = 0;
            for(int i=0;i<symbolTable.symbols[rtn].paramnum;i++){
                symbolTable.symbols[rtn].array[i+1] = arr[symbolTable.symbols[rtn].paramnum-i-1];
                if(i==symbolTable.symbols[rtn].paramnum-1) 
                    continue;
                width *= arr[i+1];
            }

            T->width += ALIGN(width,8);
            symbolTable.symbols[rtn].offset = T->offset + T->width;
            symbolTable.symbols[rtn].offset += stack_base;

            result.kind = ID;
            sprintf(result.id,"%s(%s)[%d]",symbolTable.symbols[rtn].alias, check_type(symbolTable.symbols[rtn].type,&type),width/get_typelen(symbolTable.symbols[rtn].type));
            result.offset = symbolTable.symbols[rtn].offset - T->width;
            T->code = merge(2, T->code, genIR(ARRAY, opn1, opn2, result));
        }
        else if (T0->ptr[0]->kind == ASSIGNOP)
        {
            rtn = fillSymbolTable(T0->ptr[0]->ptr[0]->type_id, newAlias(), LEV, T0->ptr[0]->type, 'v', 0); 
            if (rtn == -1)
                semantic_error(T0->ptr[0]->ptr[0]->pos, T0->ptr[0]->ptr[0]->type_id, "变量重复定义");
            else
            {
                T0->ptr[0]->place = rtn;
                T->width += ALIGN(width,8);
                symbolTable.symbols[rtn].offset = T->offset + T->width;
                symbolTable.symbols[rtn].offset += stack_base;
                Exp(T0->ptr[0]->ptr[1]);
                
                opn1.kind = ID;
                strcpy(opn1.id, symbolTable.symbols[T0->ptr[0]->ptr[1]->place].alias);
                result.kind = ID;

                result.offset = symbolTable.symbols[rtn].offset-8;
                strcpy(result.id, symbolTable.symbols[T0->ptr[0]->place].alias);
                T->code = merge(3, T->code, T0->ptr[0]->ptr[1]->code, genIR(ASSIGNOP, opn1, opn2, result));
            }
        }

        T0 = T0->ptr[1];
    }
}

void stmt_list(struct node *T)
{
    if (!T->ptr[0])
    {
        T->code = NULL;
        T->width = 0;
        return;
    }              
    if (T->ptr[1]) 
        strcpy(T->ptr[0]->Snext, newLabel());
    else 
        strcpy(T->ptr[0]->Snext, T->Snext);
    T->ptr[0]->offset = T->offset;
    semantic_Analysis(T->ptr[0]);
    T->code = T->ptr[0]->code;
    T->width = T->ptr[0]->width;
    if (T->ptr[1])
    { 
        strcpy(T->ptr[1]->Snext, T->Snext);
        T->ptr[1]->offset = T->offset;
        semantic_Analysis(T->ptr[1]);
       
        if (T->ptr[0]->kind == RETURN || T->ptr[0]->kind == EXP_STMT || T->ptr[0]->kind == COMP_STM)
            T->code = merge(2, T->code, T->ptr[1]->code);
        else
            T->code = merge(3, T->code, genLabel(T->ptr[0]->Snext), T->ptr[1]->code);
        if (T->ptr[1]->width > T->width)
            T->width = T->ptr[1]->width; 
    }
}

void if_then(struct node *T)
{
    strcpy(T->ptr[0]->Etrue, newLabel()); 
    strcpy(T->ptr[0]->Efalse, T->Snext);
    T->ptr[0]->offset = T->ptr[1]->offset = T->offset;
    boolExp(T->ptr[0]);
    T->width = T->ptr[0]->width;
    strcpy(T->ptr[1]->Snext, T->Snext);
    semantic_Analysis(T->ptr[1]); 
    if (T->width < T->ptr[1]->width)
        T->width = T->ptr[1]->width;
    T->code = merge(3, T->ptr[0]->code, genLabel(T->ptr[0]->Etrue), T->ptr[1]->code);
}

void if_then_else(struct node *T)
{
    strcpy(T->ptr[0]->Etrue, newLabel()); 
    strcpy(T->ptr[0]->Efalse, newLabel());
    T->ptr[0]->offset = T->ptr[1]->offset = T->ptr[2]->offset = T->offset;
    boolExp(T->ptr[0]); 
    T->width = T->ptr[0]->width;
    strcpy(T->ptr[1]->Snext, T->Snext);
    semantic_Analysis(T->ptr[1]); //if子句
    if (T->width < T->ptr[1]->width)
        T->width = T->ptr[1]->width;
    strcpy(T->ptr[2]->Snext, T->Snext);
    semantic_Analysis(T->ptr[2]); //else子句
    if (T->width < T->ptr[2]->width)
        T->width = T->ptr[2]->width;
    T->code = merge(6, T->ptr[0]->code, genLabel(T->ptr[0]->Etrue), T->ptr[1]->code,
                    genGoto(T->Snext), genLabel(T->ptr[0]->Efalse), T->ptr[2]->code);
}

void while_dec(struct node *T)
{
    strcpy(T->ptr[0]->Etrue, newLabel()); //子结点继承属性的计算
    strcpy(T->ptr[0]->Efalse, T->Snext);
    T->ptr[0]->offset = T->ptr[1]->offset = T->offset;
    boolExp(T->ptr[0]); //循环条件，要单独按短路代码处理
    T->width = T->ptr[0]->width;
    strcpy(T->ptr[1]->Snext, newLabel());
    semantic_Analysis(T->ptr[1]); //循环体
    if (T->width < T->ptr[1]->width)
        T->width = T->ptr[1]->width;
    T->code = merge(5, genLabel(T->ptr[1]->Snext), T->ptr[0]->code,
                    genLabel(T->ptr[0]->Etrue), T->ptr[1]->code, genGoto(T->ptr[1]->Snext));
}

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


void exp_stmt(struct node *T){
    T->ptr[0]->offset = T->offset;
    semantic_Analysis(T->ptr[0]);
    T->code = T->ptr[0]->code;
    T->width = T->ptr[0]->width;
}

void return_dec(struct node *T){
    int num;
    struct opn opn1, opn2, result;
    if (T->ptr[0]){
        T->ptr[0]->offset = T->offset;
        Exp(T->ptr[0]);
        num = symbolTable.index;
        do
            num--;
        while (symbolTable.symbols[num].flag != 'F');
        if (T->ptr[0]->type != symbolTable.symbols[num].type){
            semantic_error(T->pos, "返回值类型错误，语义错误", "");
            T->width = 0;
            T->code = NULL;
            return;
        }
        T->width = T->ptr[0]->width;
        result.kind = ID;

        strcpy(result.id, symbolTable.symbols[T->ptr[0]->place].alias);
        result.offset = symbolTable.symbols[T->ptr[0]->place].offset-8;
        T->code = merge(2, T->ptr[0]->code, genIR(RETURN, opn1, opn2, result));
    }
    else{
        T->width = 0;
        result.kind = 0;
        T->code = genIR(RETURN, opn1, opn2, result);
    }
}