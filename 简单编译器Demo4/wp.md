## 基础信息

本代码是在以下项目的基础上进行完善：

+ [GitHub - Billy1900/compiler-course-project: 华中科技大学编译原理实验2019](https://github.com/Billy1900/compiler-course-project)

上一个版本已经实现了结构体，此版本主要实现指针

## 语法分析

```c
VarDec: ID      {$$=mknode(ID,NULL,NULL,NULL,yylineno);strcpy($$->type_id,$1);}  
	| STAR ID	 {$$=mknode(IDS,NULL,NULL,NULL,yylineno);strcpy($$->type_id,$2);} 
	| ADDR ID	{$$=mknode(IDA,NULL,NULL,NULL,yylineno);strcpy($$->type_id,$2);} 
	| VarDec ArrayList {$$=mknode(ARRAY_DEC,$1,$2,NULL,yylineno);}
	;
```

+ 把指针 IDP 当成 ID 处理即可

```c
      | STAR ID	{$$=mknode(EXP_IDS,$2,NULL,NULL,yylineno);strcpy($$->type_id,$2);}
      | ADDR ID	{$$=mknode(EXP_IDA,$2,NULL,NULL,yylineno);strcpy($$->type_id,$2);}
```

+ 在 Exp 中添加对应的规则（用于识别 `&a` 和 `*a`）
+ PS：这里使用的是 ID，这就意味着结构体中的指针没法被识别（暂时不做修改）

## 语义分析

**指针声明（全局）**

```c
    case IDS:
        rtn = fillSymbolTable(T->type_id, newAlias(), LEV, T->type, 'p', T->offset); 
        if (rtn == -1)
            semantic_error(T->pos, T->type_id, "变量重复定义，语义错误");
        else
            T->place = rtn;
        T->num = 1;
        symbolTable.symbols[T->place].paramnum = 1;
        break;
```

+ 和 ID 类似的处理方式

**指针声明（局部）**

```c
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

            T->width += width;
            symbolTable.symbols[rtn].offset = width;

            if(use_struct){
                result.kind = ID;
                if(T0->ptr[0]->kind == ID){
                    sprintf(result.id,"%s(%s)",symbolTable.symbols[rtn].alias, T->ptr[0]->ptr[0]->struct_name);
                }
                else if(T0->ptr[0]->kind == IDS){
                    sprintf(result.id,"%s(*%s)",symbolTable.symbols[rtn].alias, T->ptr[0]->ptr[0]->struct_name);
                }
                
                result.offset = T->offset;
                T->code = merge(2, T->code, genIR(STRUCT, opn1, opn2, result));
            }
            else if(T0->ptr[0]->kind == IDS){
                result.kind = IDS;
                sprintf(result.id,"%s(*%s)",symbolTable.symbols[rtn].alias, check_type(symbolTable.symbols[rtn].type,&type));
                result.offset = T->offset;
                T->code = merge(2, T->code, genIR(IDS, opn1, opn2, result));
            }
        }
```

+ 和 ID 一起处理，但遇到结构体指针时需要特殊处理

**指针的使用**

上一个版本的赋值语句处理很乱，此版本进行了统一编排，使程序看上去更简洁了：

```c
void array_exp_left(struct node *T,struct opn *result){
    struct node *T0;
    int sum=-1;

    if(array_exp(T,&T0,&sum,0)){        
        (*result).kind = ID;
        sprintf((*result).id,"%s(%s[%d])<+%d>",symbolTable.symbols[T0->ptr[0]->place].alias,
            symbolTable.symbols[T0->ptr[1]->place].name,sum,
            symbolTable.symbols[T0->ptr[1]->place].offset+sum*get_typelen(symbolTable.symbols[T0->ptr[1]->place].type));
    }
    else{
        (*result).kind = ID;
        sprintf((*result).id,"%s[%d]",symbolTable.symbols[T0->place].alias,sum);
        (*result).offset = symbolTable.symbols[T0->place].offset;
    }
}

void array_exp_right(struct node *T,struct opn *opn1){
    struct node *T0;
    int sum=-1;

    if(array_exp(T,&T0,&sum,1)){
        (*opn1).kind = ID;
        sprintf((*opn1).id,"%s(%s[%d])<+%d>",symbolTable.symbols[T0->ptr[0]->place].alias,symbolTable.symbols[T0->ptr[1]->place].name,
            sum,symbolTable.symbols[T0->ptr[1]->place].offset+sum*get_typelen(symbolTable.symbols[T0->ptr[1]->place].type));
    }
    else{
        (*opn1).kind = ID;
        sprintf((*opn1).id,"%s[%d]",symbolTable.symbols[T0->place].alias,sum);
    }
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
    (*result).offset = symbolTable.symbols[T->ptr[0]->place].offset;
}

void id_exp_right(struct node *T,struct opn *opn1){
    Exp(T->ptr[1]);
    (*opn1).kind = ID;
    strcpy((*opn1).id, symbolTable.symbols[T->ptr[1]->place].alias);
    (*opn1).offset = symbolTable.symbols[T->ptr[1]->place].offset;  
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
    else if(T->ptr[1]->kind == INT){
        Exp(T->ptr[1]);
        T->code = T->ptr[1]->code;
    }
    else if(T->ptr[1]->kind == EXP_IDA){
        ida_exp_right(T,opn1);
    }
    else if(T->ptr[1]->kind == EXP_IDS){
        ids_exp_right(T,opn1);
    }
    else{
        semantic_error(T->pos, "", "赋值语句右值错误");
    }
}

void assignop_exp(struct node *T)
{
    struct opn opn1, opn2, result;

    assignop_exp_left(T,&result);
    assignop_exp_right(T,&opn1);

    T->code = merge(2, T->code, genIR(ASSIGNOP, opn1, opn2, result));
}
```

+ 先处理等号左边，后处理等号右边

另外还改了一些 BUG

## 生成 IR 中间语言

这里只针对于指针做出了小幅修改：

```c
        case IDS:
            sprintf(msg,"  POINTER %s\n", h->result.id);
            print_io(msg,fd);   
            break;
```



