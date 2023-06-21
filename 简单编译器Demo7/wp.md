## 基础信息

本代码是在以下项目的基础上进行完善：

+ [GitHub - Billy1900/compiler-course-project: 华中科技大学编译原理实验2019](https://github.com/Billy1900/compiler-course-project)

此版本已经完成了所有功能：全局变量处理，结构体处理，指针处理

## 语义分析

全局变量处理：

```c
char *newAliasEx()
{
    static int no = 1;
    char s[10];
    snprintf(s, 10, "%d", no++);
    return str_catch("exvar", s);
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
        symbolTable.symbols[T->place].offset = get_typelen(symbolTable.symbols[T->place].type);
        symbolTable.symbols[T->place].paramnum = 1;
        break;
    case IDS:
        rtn = fillSymbolTable(T->type_id, newAliasEx(), LEV, T->type, 'p', T->offset); 
        if (rtn == -1)
            semantic_error(T->pos, T->type_id, "变量重复定义，语义错误");
        else
            T->place = rtn;
        T->num = 1;
        symbolTable.symbols[T->place].offset = 8;
        symbolTable.symbols[T->place].paramnum = 1;
        break;
    case ARRAY_DEC: /* ARRAY_DEC,ARRAY_LIST */
        T->ptr[0]->type = T->type;                
        T->ptr[0]->offset = T->offset;      
        T->ptr[0]->width = T->width * T->ptr[1]->type_int;  
        symbolTable.symbols[T->ptr[0]->place].offset = get_typelen(symbolTable.symbols[T->ptr[0]->place].type);
        symbolTable.symbols[T->ptr[0]->place].paramnum = 0;
        ext_var_list(T->ptr[0],1);
        T->place = T->ptr[0]->place;
        symbolTable.symbols[T->ptr[0]->place].offset *= T->ptr[1]->ptr[0]->type_int;
        symbolTable.symbols[T->ptr[0]->place].paramnum += 1;
        symbolTable.symbols[T->ptr[0]->place].array[symbolTable.symbols[T->ptr[0]->place].paramnum-1] = T->ptr[1]->ptr[0]->type_int;
        break;
    default:
        break;
    }
}
```

结构体处理：

```c
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

void struct_exp_left(struct node *T,struct opn *result){
    Exp(T->ptr[0]);
    Exp(T->ptr[1]);

    (*result).kind = ID;
    sprintf((*result).id,"%s(@%s)<+%d>",symbolTable.symbols[T->ptr[0]->ptr[0]->place].alias,
        symbolTable.symbols[T->ptr[0]->ptr[1]->place].name,
        symbolTable.symbols[T->ptr[0]->ptr[1]->place].offset);
    (*result).offset = symbolTable.symbols[T->ptr[0]->ptr[0]->place].offset;
}

void struct_exp_right(struct node *T,struct opn *opn1){ /* Exp,Exp */
    Exp(T->ptr[1]);
    Exp(T->ptr[0]);

    (*opn1).kind = ID;
    sprintf((*opn1).id,"%s(@%s)<+%d>",symbolTable.symbols[T->ptr[1]->ptr[0]->place].alias,
        symbolTable.symbols[T->ptr[1]->ptr[1]->place].name,
        symbolTable.symbols[T->ptr[1]->ptr[1]->place].offset);
    (*opn1).offset = symbolTable.symbols[T->ptr[1]->ptr[0]->place].offset;
}
```

## 生成 IR 中间语言

全局变量处理：

```c
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
            is_arrVar[h->result.type] = 1;
        }
    
        if(!strncmp(resultstr,"var",3) || !strncmp(resultstr,"*var",4)
            || !strncmp(resultstr,"exvar",5) || !strncmp(resultstr,"*exvar",6)){
            if(h->result.offset>=0)
                sprintf(resultstr,"%s<+%d>",resultstr,h->result.offset);
            else
                sprintf(resultstr,"%s<-%d>",resultstr,-h->result.offset);
            if(is_arrVar[0] == 1){
                strcat(resultstr,"<+tempa>");
                is_arrVar[0] = 0;
            }
        }

        if(!strncmp(opnstr1,"var",3) || !strncmp(opnstr1,"&var",4) || !strncmp(opnstr1,"*var",4) 
            || !strncmp(opnstr1,"exvar",5) || !strncmp(opnstr1,"*exvar",6)){
            if(h->opn1.offset>=0)
                sprintf(opnstr1,"%s<+%d>",opnstr1,h->opn1.offset);
            else
                sprintf(opnstr1,"%s<-%d>",opnstr1,-h->opn1.offset);
            if(is_arrVar[1] == 1){
                strcat(opnstr1,"<+tempa>");
                is_arrVar[1] = 0;
            }
        }

        if(!strncmp(opnstr2,"var",3) || !strncmp(opnstr2,"*var",4)
            || !strncmp(opnstr2,"exvar",5) || !strncmp(opnstr2,"*exvar",6)){
            if(h->opn2.offset>=0)
                sprintf(opnstr2,"%s<+%d>",opnstr2,h->opn2.offset);
            else
                sprintf(opnstr2,"%s<-%d>",opnstr2,-h->opn2.offset);
        }
```

## 生成汇编代码

结构体处理：

```cpp
string Get_S(const string& str){
    auto res = vector<string>();
    string re;
    std::regex stack("<+(.*?)>");
    std::regex stru("(.*?)@(.*?)");
    cmatch m;

    sregex_iterator pos(str.cbegin(), str.cend(), stack), end;
    for(; pos!=end; ++pos){
        res.push_back(pos->str(1));
    }
    if(res.size()>2 && regex_match(str,stru)){
        int num = 0;
        num += atoi(res[0].erase(0,1).c_str());
        num += atoi(res[1].erase(0,1).c_str());
        re = "-"+to_string(num)+"(%rbp,%rax,1)";
    }
    else if(res.size()>1 && !regex_match(str,stru)){
        re = "-"+res[0].erase(0,1)+"(%rbp,%rax,1)";
    }
    else if(regex_match(str,stru)){
        int num = 0;
        num += atoi(res[0].erase(0,1).c_str());
        num += atoi(res[1].erase(0,1).c_str());
        re = "-"+to_string(num)+"(%rbp)";
    }
    else{
        re = "-"+res[0].erase(0,1)+"(%rbp)";
    }

    return re;
}
```

全局变量处理：

```cpp
string Get_D(const string& str){
    auto res = vector<string>();
    string re;
    std::regex stack("<+(.*?)>");
    std::regex stru("(.*?)@(.*?)");

    sregex_iterator pos(str.cbegin(), str.cend(), stack), end;
    for(; pos!=end; ++pos){
        res.push_back(pos->str(1));
    }
    re = strtok((char *)str.c_str(),"<");
    if(res.size()>1){
        re = re+res[0]+"(%rbp,%rax,1)";
    }
    else{
        re = re+res[0]+"(%rip)";
    }
    return re;
}
```

```cpp
void write_to_txt(const vector<string>& obj){
    ofstream out("./demo.s");
    string temp="";
    for (auto & exvar:exvariables)
        temp +="\t.comm "+exvar.first+","+exvar.second+","+exvar.second+"\n";
    for (auto & fun:func_name)
        temp +="\t.globl "+fun+"\n\t.type "+fun+", @function\n";;
    out<<temp;
    for (auto & it:obj)
        out<<it<<endl;
    out.close();
}
```

基础功能已经全部完成，剩下的就是找 BUG 和改 BUG

