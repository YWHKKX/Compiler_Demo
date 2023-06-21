## 基础信息

本代码是在以下项目的基础上进行完善：

+ [GitHub - Billy1900/compiler-course-project: 华中科技大学编译原理实验2019](https://github.com/Billy1900/compiler-course-project)

## 语义分析

先更正一个错误：

```c
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
```

+ 赋值语句右边可能是运算表达式，这种情况在上个版本考虑漏了

另外还改了一些 BUG 并进行了优化

## 生成 IR 中间语言

```c
        switch (h->op)
        {
        case ASSIGNOP:
            if(!strncmp(resultstr,"var",3) && !strncmp(opnstr1,"var",3)){
                sprintf(msg,"  %s<+%d> := %s<+%d>\n", resultstr, h->result.offset, opnstr1, h->opn1.offset);
            }
            else if(!strncmp(resultstr,"var",3)){
                sprintf(msg,"  %s<+%d> := %s\n", resultstr, h->result.offset, opnstr1);
            }
            else if(!strncmp(opnstr1,"var",3)){
                sprintf(msg,"  %s := %s<+%d>\n", resultstr, opnstr1, h->opn1.offset);
            }
            else{
                sprintf(msg,"  %s := %s\n", resultstr, opnstr1);
            }
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
            if(!strncmp(resultstr,"var",3)){
                sprintf(temp_buf[0],"%s<+%d>",resultstr,h->result.offset);
            }
            else{
                sprintf(temp_buf[0],"%s",resultstr);
            }
            if(!strncmp(opnstr1,"var",3)){
                sprintf(temp_buf[1],"%s<+%d>",opnstr1,h->opn1.offset);
            }
            else{
                sprintf(temp_buf[1],"%s",opnstr1);
            }      
            if(!strncmp(opnstr2,"var",3)){
                sprintf(temp_buf[2],"%s<+%d>",opnstr2,h->opn2.offset);
            }
            else{
                sprintf(temp_buf[2],"%s",opnstr2);
            }

            sprintf(msg,"  %s := %s %c %s\n",temp_buf[0],temp_buf[1],h->op == PLUS ? '+' : h->op == MINUS ? '-' : h->op == STAR ? '*' : '\\',temp_buf[2]);
            print_io(msg,fd);
            break;
                
                ......
                
        case FUNCTION:
            rtn = searchSymbolTable(h->result.id);
            sprintf(msg,"\nFUNCTION %s - %d :\n", h->result.id,symbolTable.symbols[rtn].offset);
            print_io(msg,fd);
            break;
         
                ......
                    
        case RETURN:
            if (h->result.kind){
                sprintf(msg,"  RETURN %s<+%d>\n", resultstr,h->result.offset);
                print_io(msg,fd);
            }
            else{
                sprintf(msg,"  RETURN\n");
                print_io(msg,fd);
            } 
            break;
        }
```

+ 对局部变量的 IR 进行了修改，使其可以显示在栈中的偏移
+ 修改了函数标签，使其可以显示该函数使用了多大的栈空间

## 生成汇编代码

参考了之前 “编译原理实验” 的代码，目前实现了对赋值语句和基础运算的翻译：

```cpp
#include <iostream>
#include <cstdlib>
#include <vector>
#include <regex>
#include <fstream>
#include <bitset>

using namespace std;

string regs[] = {"rbx","rcx","rdx","r8","r10","r11","r14","r15"};
string params[] = {"rdi","rsi","rdx","rcx","r8","r9"};
vector<string> variables;
map<string,string> table;
map<string,int> reg_ok;
size_t func_argn;
size_t func_param;
string func_stack;

void printERR(char* str){
    cout << str << endl;
    exit(-1);
}

void Load_Var(string Inter){
    regex temp_re("temp\\d+");
    regex var_re("var\\d+");
    smatch result;
    string temp_line;

    string::const_iterator iter = Inter.begin();
    string::const_iterator iterEnd = Inter.end();
    while (regex_search(iter,iterEnd,result,temp_re)){
        temp_line = result[0];
        variables.emplace_back(temp_line);
        iter = result[0].second;
    }

    iter = Inter.begin();
    iterEnd = Inter.end();
    while (regex_search(iter,iterEnd,result,var_re)){
        temp_line = result[0];
        variables.emplace_back(temp_line);
        iter = result[0].second;
    }    
}

string Load_Inter(const string& filename){
    string lines;
    string temp_line;
    ifstream in(filename);
    if(in){
        while (getline(in,temp_line)){
            if (temp_line == " ")
                continue;
            lines.append(temp_line);
            lines.append("\n");
        }
    }
    in.close();
    return lines;
}

void Clean_R(){
    vector<string> keys;
    for (auto &it:table) /* 遍历table的key(已经分配寄存器) */
        keys.emplace_back(it.first); /* 直接在vector尾部创建这个元素 */
    for (auto &key:keys){ /* 清空之前所有分配的临时变量的映射关系 */
        if (key.find("temp")!=string::npos && find(variables.begin(),variables.end(),key) == variables.end()){
            reg_ok[table[key]] = 1;
            table.erase(key);

        }
    }
}

string Get_R(const string& str){ /* 引用是原变量的一个别名,跟原来的变量实质上是同一个东西 */
    for (auto it = variables.begin();it!=variables.end();++it){ /* it为迭代器 */
        if (*it == str){ /* 去除重复 */
            it = variables.erase(it);
            break;
        }
    } 

    if (table.find(str) != table.end()){
        /* 如果已经存在寄存器分配,那么直接返回寄存器 */
        return "%"+table[str];
    }
    else{
        vector<string> keys;
        for (auto &it:table) /* 遍历table的key(已经分配寄存器) */
            /* 类似于python的"for i in data" */
            keys.emplace_back(it.first); /* 直接在vector尾部创建这个元素 */
        
        for (const auto & reg : regs){ /* 对于所有寄存器 */
            if(reg_ok[reg] == 1){ /* 如果寄存器可用 */
                table[str] = reg; /* 将可用寄存器分配给该变量,映射关系存到table中 */
                reg_ok[reg] = 0; /* 寄存器reg设置为已用 */
                return "%"+reg;
            }
        } 
        
        printERR("临时变量个数过多");
    }
}

string Get_S(const string& str){
    std::regex stack("<+(.*?)>");
    std::smatch m;
    auto ret = std::regex_search(str,m,stack);
    return "-"+m.str(1).erase(0,1)+"(%rbp)";
}

string storage(const string& str){
    if(strncmp(str.c_str(),"temp",4)==0){
        return Get_R(str);
    }
    else if(strncmp(str.c_str(),"var",3)==0){
        return Get_S(str);
    }
}

string translate(string temp_str){ /* 一次只读取一行 */
    /* 将每行string按空格存成数组 */
    vector<string> line;
    string temp_res;
    stringstream input(temp_str); 
    while (input>>temp_res) /* 可以理解为队列,只能从头节点取出值,新数据接在尾节点 */
        line.emplace_back(temp_res); 

    if(line.size()==0){
        return " "; /* 规避'\n'的影响 */
    }

    string temp_return;
    if(line[0] == "LABEL"){
        return "\taddq $"+func_stack+",%rsp\n\tpopq %rbp\n\tret\n\t.cfi_endproc\n"+line[1]+":";
    }
    if (line[0] == "RETURN"){
        func_argn = 0;
        Clean_R();
        return "\tmovq "+storage(line[1])+",%rax";
    }
    if (line[0] == "FUNCTION"){
        for (const auto & reg : regs) 
            reg_ok[reg] = 1;
        table.clear();
        func_param = 0;
        func_stack = line[3];
        Clean_R();
        return line[1]+":"+"\n\t.cfi_startproc\n\tendbr64\n\tpushq %rbp\n\tmov %rsp,%rbp\n\tsubq $"+func_stack+",%rsp";
    }
    if (line[1] == ":=") {
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
        if (line.size() == 5){
            if (line[3] == "+"){
                temp_return  = "\txor "+storage(line[0])+","+storage(line[0])+"\n";

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
                temp_return  = "\txor "+storage(line[0])+","+storage(line[0])+"\n";

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
                temp_return  = "\txor "+storage(line[0])+","+storage(line[0])+"\n";

                temp_return += "\tmovq ";
                temp_return += storage(line[0])+",";
                temp_return += "%rax";

                temp_return += "\n\tmulq ";
                temp_return += storage(line[2]);
                temp_return += ",%rax";

                temp_return += "\n\tmulq ";
                temp_return += storage(line.back());
                temp_return += ",%rax";

                temp_return += "\n\tmovq ";
                temp_return += "%rax,";
                temp_return += storage(line[0]);

                return temp_return;
            }
            else if (line[3] == "\\"){
                temp_return  = "\txor "+storage(line[0])+","+storage(line[0])+"\n";

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
    }
    return " ";
}

void write_to_txt(const vector<string>& obj){
    ofstream out("./demo.s");
    string temp ="\t.globl main\n\t.type main, @function\n";
    out<<temp;
    for (auto & it:obj)
        out<<it<<endl;
    out.close();
}

int main(){
    for (const auto & reg : regs) //初始化，所有寄存器都可用
        reg_ok[reg] = 1;

    string filename = "./inter.txt";
    string Inter = Load_Inter(filename);//读取中间代码

    Load_Var(Inter);//第一遍扫描，记录所有变量
    vector<string> obj;
    ifstream in(filename);
    string temp_line,obj_line;
    if(in) {
        while (getline(in, temp_line)) {
            obj_line = translate(temp_line);
            if (obj_line == " ")
                continue;
            obj.emplace_back(obj_line);
        }
    } else{
        cout<<"file open falied.\n";
    }
    in.close();
    write_to_txt(obj);
    return 0;
}
```

+ 对于局部变量 “var” 需要存放在栈中，对于临时变量 “temp” 需要存放在寄存器中

下个版本打算先实现函数调用