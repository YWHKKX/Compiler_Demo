## 基础信息

本代码是在以下项目的基础上进行完善：

+ [GitHub - Billy1900/compiler-course-project: 华中科技大学编译原理实验2019](https://github.com/Billy1900/compiler-course-project)

此版本在上一个版本的基础上进行了大修改

## 语义分析

定义了一个函数用于处理数组中的变量：

```c
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
```

+ 处理的过程比较复杂，核心思路就是添加一条伪代码，用于在数组赋值前说明该数组所引用的变量
+ 效果如下：

```c
VAR exvar1(int)
VAR exvar2(int)

FUNCTION main - 312 :
  ARRAY var2(int)[36]<+0>
  temp1 := #2
  var3<+288> := temp1
  temp2 := #1
  var4<+296> := temp2
  temp3 := #1
  var5<+304> := temp3
  temp6 := #6
  var2<+104> := temp6
#!tempa := {#48}*{var3<+288>}+{#8}*{var4<+296>} /* 伪代码 */
  temp7 := #8
  var2<+0><+tempa> := temp7
LABEL Flabelmain :
```

+ 在一个数组中，所有变量对数组所造成的影响都会记录在 `tempa` 中

## 生成 IR 中间语言

在此部分中添加了生成伪代码的操作：

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
```

## 生成汇编代码

此部分进行了大修改：

实现了对函数调用的翻译：（包括参数传入和参数使用）

```c
    if (line[0] == "ARG"){
        return "\tmovq "+storage(line.back())+",%"+params[func_argn++];
    }
    if (line[0] == "PARAM"){
        return "\tmovq %"+params[func_param++]+","+storage(line.back());
    }
    if (line[0] == "CALL"){
        func_argn = 0;
        temp_return = "\tcall "+line.back();
        return temp_return;
    }
```

实现了对数组赋值的处理：（包括对数组中变量的特殊处理）

```c
        if (line[0][0] == '#' && line[0][1] == '!'){
            std::regex d("#[[:digit:]]+");
            std::regex a("var[[:digit:]]+<\\+[[:digit:]]+>");

            temp_return = "\txor %rax,%rax";
            sregex_iterator pos1(line[2].cbegin(), line[2].cend(), d), end;
            sregex_iterator pos2(line[2].cbegin(), line[2].cend(), a);
            for(; pos1!=end; ++pos1,++pos2){
                temp_return += "\n\txor %rbx,%rbx";
                temp_return += "\n\taddq "+storage(pos2->str())+",%rbx";
                temp_return += "\n\timulq $"+pos1->str().substr(1,pos1->str().size())+",%rbx";
                temp_return += "\n\tsubq %rbx,%rax";
            }
            temp_return += "\t\nxor %rax,%rax";
            return temp_return;
        }
        if (line.size() == 3){
            if (line[2][0] == '#'){
                temp_return = "\tmovq $"+line.back().substr(1)+","+Get_R(line[0]);
            }
            else if (!strncmp(line[2].c_str(),"var",3) && !strncmp(line[0].c_str(),"var",3)){
                temp_return =  "\tmovq "+storage(line[2])+",";
                temp_return += Get_R(line[0])+"\n";
                temp_return += "\tmovq "+Get_R(line[0])+",";
                temp_return += storage(line[0]);           
            }
            else{
                temp_return = "\tmovq ";
                temp_return += storage(line[2])+',';
                temp_return += storage(line[0]);
            }

            if(!strncmp(line[0].c_str(),"var",3) && !strncmp(line[2].c_str(),"temp",4)){
                Clean_R();
            }
            return temp_return;
        }
```

实现了条件跳转：

```c
    if (line[0] == "IF") {
        temp_return = "\tcmpq ";
        if (line[2] == "=="){
            temp_return += storage(line[1])+",";
            temp_return += storage(line[3]);
            temp_return += "\n\tje ";
            temp_return += "."+line.back();
            return temp_return;
        }
        if (line[2] == "!="){
            temp_return += storage(line[1])+",";
            temp_return += storage(line[3]);
            temp_return += "\n\tjne ";
            temp_return += "."+line.back();
            return temp_return;
        }
        if (line[2] == ">"){
            temp_return += storage(line[1])+",";
            temp_return += storage(line[3]);
            temp_return += "\n\tja ";
            temp_return += "."+line.back();
            return temp_return;
        }
        if (line[2] == "<"){
            temp_return += storage(line[1])+",";
            temp_return += storage(line[3]);
            temp_return += "\n\tjb ";
            temp_return += "."+line.back();
            return temp_return;
        }
        if (line[2] == ">="){
            temp_return += storage(line[1])+",";
            temp_return += storage(line[3]);
            temp_return += "\n\tjae ";
            temp_return += "."+line.back();
            return temp_return;
        }
        if (line[2] == "<="){
            temp_return += storage(line[1])+",";
            temp_return += storage(line[3]);
            temp_return += "\n\tjbe ";
            temp_return += "."+line.back();
            return temp_return;
        }
    }
```

实现了“加减乘除”4种基础运算：（其中不包括对带变量数组的处理）

```c
        if (line.size() == 5){
            if (line[3] == "+"){
                if(!strcmp(line[0].c_str(),line[2].c_str())){
                    return "\taddq "+storage(line.back())+","+storage(line[0]);
                }

                temp_return = "\txor "+storage(line[0])+","+storage(line[0])+"\n";

                temp_return += "\tmovq ";
                temp_return += storage(line[0])+",";
                temp_return += "%rax";

                temp_return += "\n\taddq ";
                temp_return += storage(line[2]);
                temp_return += ",%rax";

                temp_return += "\n\taddq ";
                temp_return += storage(line.back());
                temp_return += ",%rax";

                temp_return += "\n\tmovq ";
                temp_return += "%rax,";
                temp_return += storage(line[0]);

                return temp_return;
            }
            else if (line[3] == "-"){
                if(!strcmp(line[0].c_str(),line[0].c_str())){
                    return "\tsubq "+storage(line.back())+","+storage(line[0]);
                }

                temp_return = "\txor "+storage(line[0])+","+storage(line[0])+"\n";

                temp_return += "\tmovq ";
                temp_return += storage(line[0])+",";
                temp_return += "%rax";

                temp_return += "\n\taddq ";
                temp_return += storage(line[2]);
                temp_return += ",%rax";

                temp_return += "\n\tsubq ";
                temp_return += storage(line.back());
                temp_return += ",%rax";

                temp_return += "\n\tmovq ";
                temp_return += "%rax,";
                temp_return += storage(line[0]);

                return temp_return;
            }
            else if (line[3] == "*"){
                if(!strcmp(line[0].c_str(),line[0].c_str())){
                    return "\timulq "+storage(line.back())+","+storage(line[0]);
                }

                temp_return = "\txor "+storage(line[0])+","+storage(line[0])+"\n";

                temp_return += "\tmovq ";
                temp_return += storage(line[0])+",";
                temp_return += "%rax";

                temp_return += "\n\timulq ";
                temp_return += storage(line[2]);
                temp_return += ",%rax";

                temp_return += "\n\timulq ";
                temp_return += storage(line.back());
                temp_return += ",%rax";

                temp_return += "\n\tmovq ";
                temp_return += "%rax,";
                temp_return += storage(line[0]);

                return temp_return;
            }
            else if (line[3] == "\\"){
                if(!strcmp(line[0].c_str(),line[0].c_str())){
                    return "\tdivq "+storage(line.back())+","+storage(line[0]);
                }

                temp_return = "\txor "+storage(line[0])+","+storage(line[0])+"\n";

                temp_return += "\tmovq ";
                temp_return += storage(line[2])+",";
                temp_return += "%rax";

                temp_return += "\n\tdivq ";
                temp_return += storage(line.back());

                temp_return += "\n\tmovq ";
                temp_return += "%rax,";
                temp_return += storage(line[0]);

                return temp_return;
            }
        }
```

