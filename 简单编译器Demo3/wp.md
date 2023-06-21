## 基础信息

本代码是在以下项目的基础上进行完善：

+ [GitHub - Billy1900/compiler-course-project: 华中科技大学编译原理实验2019](https://github.com/Billy1900/compiler-course-project)

上一个版本已经完成了多级数组，此版本主要用于实现结构体

## 语义分析

这里先对上个版本的全局数组定义进行修改：

```c
void ext_var_list(struct node *T,int is_arr) /* ID,EXT_DEC_LIST | ARRAY_DEC,EXT_DEC_LIST */
{
    int rtn, num = 1;
    switch (T->kind)
    {
    case EXT_DEC_LIST:
            ......
    case ID:
            ......
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
```

+ 上一个版本没有及时更新 `T->place`，这里补上了

对结构体的处理分为3部分：

+ 结构体定义
+ 结构体声明
+ 结构体使用

**结构体定义**

相当于处理一个只有 `DefList` 的复合语句：

```c
StructDef: STRUCT OptTag LC DefList RC{$$=mknode(STRUCT_DEF,$2,$4,NULL,yylineno);}
	;

OptTag: {$$=NULL;}
       | ID {$$=mknode(STRUCT_TAG,NULL,NULL,NULL,yylineno);strcpy($$->struct_name,$1);}
       ;
```

我们可以使用 `var_def` 函数快速处理 `DefList`，但必须带有一些标记：

+ 标记用于区分结构体变量和普通变量

```c
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

    rtn = fillSymbolTable(T->ptr[0]->struct_name, newAlias(), 0, STRUCT, 'S', T->offset + T->width);
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
```

+ 处理结构体变量的核心思路就是先修改 `T->type_id`，然后执行 `var_def` 生成符号表，最后修改 `symbolTable.symbols[T1->place].flag` 用于标记
+ 最后还要把结构体名称也写入符号表中
+ 填表完成后，会对 `symbolTable.symbols[i].offset` 进行更新，其意义在于记录该条目在结构体中的偏移

**结构体声明（全局）**

当结构体定义好之后，我们就可以用处理 `TYPE` 的方式来处理全局的结构体变量：

```c
    if (!strcmp(T->ptr[0]->type_id, "int"))
    {
        T->type = T->ptr[1]->type = INT;
        T->ptr[1]->width = 4;
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
```

+ 对 `struct_name` 进行传递，方便之后的 `ext_var_def` 获取到正确的 `struct_name`

**结构体声明（局部）**

先用处理 `TYPE` 的方式来处理局部的结构体变量：

```c
    use_struct = 0;
    if(T->ptr[0]->kind == STRUCT_DEC){
        T->ptr[1]->type = STRUCT;
        rtn = searchSymbolTable(T->ptr[0]->ptr[0]->struct_name);
        if (rtn == -1)
            semantic_error(T->pos, T->type_id, "结构体未定义，语义错误");
        if (symbolTable.symbols[rtn].flag == 'F'){
            semantic_error(T->pos, T->type_id, "是函数名，不是结构体，语义错误");
        }
        else if (symbolTable.symbols[rtn].flag == 'v'){
            semantic_error(T->pos, T->type_id, "是变量，不是结构体，语义错误");
        }
        use_struct = 1;
        strcpy(T->ptr[1]->struct_name,T->ptr[0]->ptr[0]->struct_name);
    }
```

+ `use_struct` 用于区分普通变量和结构体变量

对于局部的结构体变量，可以选择在处理 `ID` 时一并处理：

```c
        if (T0->ptr[0]->kind == ID){
            rtn = fillSymbolTable(T0->ptr[0]->type_id, newAlias(), LEV, T0->ptr[0]->type, 'v', T->offset + T->width); //此处偏移量未计算，暂时为0
            if (rtn == -1)
                semantic_error(T0->ptr[0]->pos, T0->ptr[0]->type_id, "变量重复定义");
            else
                T0->ptr[0]->place = rtn;

            symbolTable.symbols[rtn].paramnum = 1;
            T->width += width;
            if(use_struct){
                result.kind = ID;
                sprintf(result.id,"%s(%s)",symbolTable.symbols[rtn].alias, T->ptr[0]->ptr[0]->struct_name );
                result.offset = T->offset;
                T->code = merge(2, T->code, genIR(STRUCT, opn1, opn2, result));
            }
        }
```

+ 由于非数组的局部变量不会生成 IR 中间代码，于是这里要特殊处理一下，使其显式地生成中间代码
+ 另外需要在 `print_IR` 中添加对应的 `switch-case`：

```c
        case STRUCT:
            sprintf(msg,"  STRUCT %s\n", h->result.id);
            print_io(msg,fd);
            break;
```

**结构体使用**

首先结构体需要在赋值语句中使用，这样在赋值语句中就会有3种情况：

+ 赋值语句左边为结构体变量
+ 赋值语句右边为结构体变量
+ 赋值语句左右两边都是结构体变量

```c
      | Exp DOT Exp	{$$=mknode(EXP_ELE,$1,$3,NULL,yylineno);}//$$=mknode(EXP_ELE,$1,$3,NULL,yylineno);
```

但是在上一个版本的编译器中，我们已经对数组进行了特殊处理，那么就必须考虑赋值语句左右两边分别为数组或结构体变量的情况，同时，结构体变量也可能为数组，大大增加了代码的复杂性

我的解决思路是：

+ 把处理结构体变量的代码镶嵌到数组处理的代码中，这样就可以用现成的代码来处理结构体数组变量了
+ 对于普通的结构体变量也要在数组处理的函数中进行处理（当赋值语句的一端为数组时，另一端要么是结构体变量，要么是普通变量）
+ 当结构体变量和数组混合出现时，优先调用数组处理的函数（因为处理结构体的代码镶嵌在其中，在处理数组的同时就可以处理结构体变量）

首先展示扩展后的数组处理函数：

```c
int array_exp(struct node *T,struct node **Tr,int *sum,int key){
    struct node *T0;
    char arr[32];
    char name[32];
    int i=0,num,use_struct=0;
    
    T0 = T->ptr[key];
    while (T0->kind == EXP_ARRAY) /* Exp,ArrayList */
    {
        arr[i++] = T0->ptr[1]->ptr[0]->type_int;
        T0 = T0->ptr[0];
    }
    T0->offset = T->offset;
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

    if(symbolTable.symbols[T->ptr[1-key]->place].flag == 'a'){
        semantic_error(T->pos, T->ptr[1-key]->type_id, "是数组名，不是普通变量，语义错误");
    }

    T->code = merge(2, T->code, T->ptr[1-key]->code);

    return use_struct;
}

void array_exp_right(struct node *T){
    struct opn opn1, opn2, result;
    struct node *T0;
    char name[32];
    char stname[32];
    int sum=-1;

    if(array_exp(T,&T0,&sum,1)){
        opn1.kind = ID;
        sprintf(name,"%s(%s[%d])<+%d>",symbolTable.symbols[T0->ptr[0]->place].alias,symbolTable.symbols[T0->ptr[1]->place].name,
            sum,symbolTable.symbols[T0->ptr[1]->place].offset+sum*get_typelen(symbolTable.symbols[T0->ptr[1]->place].type));
        strcpy(opn1.id, name);
    }
    else{
        opn1.kind = ID;
        sprintf(name,"%s[%d]",symbolTable.symbols[T0->place].alias,sum);
        strcpy(opn1.id, name);
    }

    if(T->ptr[0]->kind == EXP_ELE){
        sprintf(stname,symbolTable.symbols[T->ptr[0]->ptr[1]->place].name);
        sprintf(name,"%s(%s)<+%d>",symbolTable.symbols[T->ptr[0]->ptr[0]->place].alias,stname,symbolTable.symbols[T->ptr[0]->ptr[1]->place].offset);

        result.kind = ID;
        strcpy(result.id, name);
        result.offset = symbolTable.symbols[T->ptr[0]->ptr[0]->place].offset;
    }
    else{
        result.kind = ID;
        strcpy(result.id, symbolTable.symbols[T->ptr[0]->place].alias); 
        result.offset = symbolTable.symbols[T->ptr[0]->place].offset;
    }

    T->code = merge(2, T->code, genIR(ASSIGNOP, opn1, opn2, result));
}

void array_exp_left(struct node *T){
    struct opn opn1, opn2, result;
    struct node *T0;
    char name[32];
    char stname[32];
    int sum=-1;

    if(array_exp(T,&T0,&sum,0)){        
        result.kind = ID;
        sprintf(name,"%s(%s[%d])<+%d>",symbolTable.symbols[T0->ptr[0]->place].alias,symbolTable.symbols[T0->ptr[1]->place].name,
            sum,symbolTable.symbols[T0->ptr[1]->place].offset+sum*get_typelen(symbolTable.symbols[T0->ptr[1]->place].type));
        strcpy(result.id, name);
    }
    else{
        result.kind = ID;
        sprintf(name,"%s[%d]",symbolTable.symbols[T0->place].alias,sum);
        strcpy(result.id, name);
        result.offset = symbolTable.symbols[T0->place].offset;
    }

    if(T->ptr[1]->kind == EXP_ELE){
        sprintf(stname,symbolTable.symbols[T->ptr[1]->ptr[1]->place].name);
        sprintf(name,"%s(%s)<+%d>",symbolTable.symbols[T->ptr[1]->ptr[0]->place].alias,stname,symbolTable.symbols[T->ptr[1]->ptr[1]->place].offset);

        result.kind = ID;
        strcpy(result.id, name);
        result.offset = symbolTable.symbols[T->ptr[0]->ptr[0]->place].offset;
    }
    else{
        opn1.kind = ID;
        strcpy(opn1.id, symbolTable.symbols[T->ptr[1]->place].alias); 
        opn1.offset = symbolTable.symbols[T->ptr[1]->place].offset;
    }

    T->code = merge(2, T->code, genIR(ASSIGNOP, opn1, opn2, result));
}

void array_exp_all(struct node *T){
    struct opn opn1, opn2, result;
    struct node *T0;
    char name[32];
    int sum=-1;

    if(array_exp(T,&T0,&sum,1)){
        opn1.kind = ID;
        sprintf(name,"%s(%s[%d])<+%d>",symbolTable.symbols[T0->ptr[0]->place].alias,symbolTable.symbols[T0->ptr[1]->place].name,
            sum,symbolTable.symbols[T0->ptr[1]->place].offset+sum*get_typelen(symbolTable.symbols[T0->ptr[1]->place].type));
        strcpy(opn1.id, name);
    }else{
        opn1.kind = ID;
        sprintf(name,"%s[%d]",symbolTable.symbols[T0->place].alias,sum);
        strcpy(opn1.id, name);
    }

    if(array_exp(T,&T0,&sum,0)){
        result.kind = ID;
        sprintf(name,"%s(%s[%d])<+%d>",symbolTable.symbols[T0->ptr[0]->place].alias,symbolTable.symbols[T0->ptr[1]->place].name,
        sum,symbolTable.symbols[T0->ptr[1]->place].offset+sum*get_typelen(symbolTable.symbols[T0->ptr[1]->place].type));
        strcpy(result.id, name);
    }
    else{
        result.kind = ID;
        sprintf(name,"%s[%d]",symbolTable.symbols[T0->place].alias,sum);
        strcpy(result.id, name);
    }

    result.offset = symbolTable.symbols[T0->place].offset;
    T->code = merge(2, T->code, genIR(ASSIGNOP, opn1, opn2, result));
}
```

+ 这些函数都被分为了 “结构体处理”（包括 “数组处理” 和 “普通变量处理”）和 “数组处理” 两个互斥的部分，由 `array_exp` 的返回值进行区分

接着是结构体处理函数：

```c
void struct_exp(struct node *T){
    char name[32];
    int rtn;

    Exp(T->ptr[0]);

    sprintf(name,"%s.%s",T->ptr[0]->struct_name,T->ptr[1]->type_id);
    rtn = searchSymbolTable(name);
    if (rtn == -1){
        semantic_error(T->ptr[0]->pos, T->ptr[0]->struct_name, "结构体没有目标条目，语义错误");
        return;
    }
    
    T->ptr[1]->place = rtn;
}

void struct_exp_right(struct node *T){
    char name[32];
    char stname[32];
    struct opn opn1, opn2, result;

    Exp(T->ptr[1]);
    Exp(T->ptr[0]);
    sprintf(stname,symbolTable.symbols[T->ptr[1]->ptr[1]->place].name);
    sprintf(name,"%s(%s)<+%d>",symbolTable.symbols[T->ptr[1]->ptr[0]->place].alias,stname,symbolTable.symbols[T->ptr[1]->ptr[1]->place].offset);

    opn1.kind = ID;
    strcpy(opn1.id, name);
    opn1.offset = symbolTable.symbols[T->ptr[1]->ptr[0]->place].offset;

    result.kind = ID;
    strcpy(result.id, symbolTable.symbols[T->ptr[0]->place].alias); 
    result.offset = symbolTable.symbols[T->ptr[0]->place].offset;

    T->code = merge(2, T->ptr[0]->code, genIR(ASSIGNOP, opn1, opn2, result));
}

void struct_exp_left(struct node *T){
    char name[32];
    char stname[32];
    struct opn opn1, opn2, result;

    Exp(T->ptr[0]);
    Exp(T->ptr[1]);
    
    sprintf(stname,symbolTable.symbols[T->ptr[0]->ptr[1]->place].name);
    sprintf(name,"%s(%s)<+%d>",symbolTable.symbols[T->ptr[0]->ptr[0]->place].alias,stname,symbolTable.symbols[T->ptr[0]->ptr[1]->place].offset);

    opn1.kind = ID;
    strcpy(opn1.id, symbolTable.symbols[T->ptr[1]->place].alias); 
    opn1.offset = symbolTable.symbols[T->ptr[1]->place].offset;

    result.kind = ID;
    strcpy(result.id, name);
    result.offset = symbolTable.symbols[T->ptr[0]->ptr[0]->place].offset;

    T->code = merge(2, T->ptr[1]->code, genIR(ASSIGNOP, opn1, opn2, result));
}

void struct_exp_all(struct node *T){
    char name[32];
    char stname[32];
    struct opn opn1, opn2, result;

    Exp(T->ptr[0]);
    Exp(T->ptr[1]);

    sprintf(stname,symbolTable.symbols[T->ptr[0]->ptr[1]->place].name);
    sprintf(name,"%s(%s)<+%d>",symbolTable.symbols[T->ptr[0]->ptr[0]->place].alias,stname,symbolTable.symbols[T->ptr[0]->ptr[1]->place].offset);

    result.kind = ID;
    strcpy(result.id, name);
    result.offset = symbolTable.symbols[T->ptr[0]->ptr[0]->place].offset;

    sprintf(stname,symbolTable.symbols[T->ptr[1]->ptr[1]->place].name);
    sprintf(name,"%s(%s)<+%d>",symbolTable.symbols[T->ptr[1]->ptr[0]->place].alias,stname,symbolTable.symbols[T->ptr[1]->ptr[1]->place].offset);

    opn1.kind = ID;
    strcpy(opn1.id, name);
    opn1.offset = symbolTable.symbols[T->ptr[1]->ptr[0]->place].offset;

    T->code = merge(2, T->code, genIR(ASSIGNOP, opn1, opn2, result));
}
```

+ 这些函数其实有些多余，以后看看能不能优化吧

最后是最核心的赋值语句处理：

```c
void assignop_exp(struct node *T)
{
    struct opn opn1, opn2, result;
    struct node *T0;

    if (T->ptr[1]->kind == EXP_ARRAY && T->ptr[0]->kind == EXP_ARRAY){ /* 等号两端都为数组 */
        array_exp_all(T);
    }
    else if (T->ptr[1]->kind == EXP_ARRAY){ /* 等号右端为数组 */
        array_exp_right(T);
    }
    else if (T->ptr[0]->kind == EXP_ARRAY){ /* 等号左端为数组 */
        array_exp_left(T);
    }
    else if (T->ptr[1]->kind == EXP_ELE && T->ptr[0]->kind == EXP_ELE){ /* 等号两端都为结构体变量 */
        struct_exp_all(T);
    } 
    else if (T->ptr[1]->kind == EXP_ELE){ /* 等号右端为结构体变量 */
        struct_exp_right(T);
    }
    else if (T->ptr[0]->kind == EXP_ELE){ /* 等号左端为结构体变量 */
        struct_exp_left(T);
    }
    else if (T->ptr[0]->kind != ID){
        semantic_error(T->pos, "", "赋值语句没有左值，语义错误");
    }
    else
    {
        T->ptr[0]->offset = T->offset;
        Exp(T->ptr[0]); 

        if(symbolTable.symbols[T->ptr[0]->place].flag == 'a'){
            semantic_error(T->pos, T->ptr[0]->type_id, "是数组名，不是普通变量，语义错误");
        }

        T->ptr[1]->offset = T->offset;
        Exp(T->ptr[1]);
        if(symbolTable.symbols[T->ptr[1]->place].flag == 'a'){
            semantic_error(T->pos, T->ptr[1]->type_id, "是数组名，不是普通变量，语义错误");
        }

        T->type = T->ptr[0]->type;
        T->width = T->ptr[1]->width;
        T->code = merge(2, T->ptr[0]->code, T->ptr[1]->code);

        opn1.kind = ID;
        strcpy(opn1.id, symbolTable.symbols[T->ptr[1]->place].alias); //右值一定是个变量或临时变量
        opn1.offset = symbolTable.symbols[T->ptr[1]->place].offset;
        result.kind = ID;
        strcpy(result.id, symbolTable.symbols[T->ptr[0]->place].alias);
        result.offset = symbolTable.symbols[T->ptr[0]->place].offset;
        T->code = merge(2, T->code, genIR(ASSIGNOP, opn1, opn2, result));
    }
}
```

+ 这种写法导致整个函数看上去很臃肿，但目前没有想到更好的解决办法

## 生成 IR 中间语言

这一部分主要添加了对结构体的处理

测试案例如下：

```c
struct test{
	int c;
	int p2;
	int arr[5][5];
	int d;
};

struct test2{
	char arr[5][5];
	char arr2[4][4];
};

int fun(int x){
	struct test t1;
	struct test2 t2;
	struct test t3;

	t1.c = 16;
	t2.arr[4][4] = t1.c;
	t1.d = t1.c;
	t3.c = t2.arr[1][4];
}
```

结果如下：

```c
STRUCT test :
  VAR .c(int)<+0>
  VAR .p2(int)<+4>
  ARRAY .arr(int)[25]<+8>
  VAR .d(int)<+108>
STRUCT test2 :
  ARRAY .arr(char)[25]<+0>
  ARRAY .arr2(char)[16]<+25>


FUNCTION fun :
  PARAM var10(int)
  STRUCT var11(test)
  STRUCT var12(test2)
  STRUCT var13(test)
  temp1 := #16
  var11(test.c)<+0> := temp1
  var11(test.c)<+0> := temp1
  var11(test.d)<+108> := var11(test.c)<+0>
  var13(test.c)<+0> := var12(test2.arr[9])<+9>
LABEL label1 :
```

+ 在结构体中可以记录各个条目的偏移
+ 对于数组则可以直接计算到具体的内存地址

