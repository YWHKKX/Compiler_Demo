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
    if (line[0] == "GOTO"){
        return "\tj "+line[1];
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
    if (line[0] == "ARG"){
        func_argn++;
        return "\tmovq %"+params[func_argn-1]+","+storage(line.back());
    }
    if (line[0] == "PARAM"){
        func_param++;
        table[line.back()] = params[func_param-1];
    }
    if (line[0] == "CALL"){
        func_argn = 0;
        return string("")+"\taddi %sp,%sp,-60\n\tsw %ra,0(%sp)\n\tsw %t0,4(%sp)\n\tsw %t1,8(%sp)\n\tsw %t2,12(%sp)\n\tsw %t3,16(%sp)\n"+ \
            "\tsw %t4,20(%sp)\n\tsw %t5,24(%sp)\n\tsw %t6,28(%sp)\n\tsw %t7,32(%sp)\n\tsw %t8,36(%sp)\n\tsw %t9,40(%sp)\n"+ \
            "\tsw %a0,44(%sp)\n\tsw %a1,48(%sp)\n\tsw %a3,52(%sp)\n\tsw %a4,56(%sp)\n"+ \
            "\tjal "+line.back()+"\n"+ \
            "\tlw %ra,0(%sp)\n\tlw %t0,4(%sp)\n\tlw %t1,8(%sp)\n\tlw %t2,12(%sp)\n\tlw %t3,16(%sp)\n"+ \
            "\tlw %t4,20(%sp)\n\tlw %t5,24(%sp)\n\tlw %t6,28(%sp)\n\tlw %t7,32(%sp)\n\tlw %t8,36(%sp)\n\tlw %t9,40(%sp)\n"+ \
            "\tlw %a0,44(%sp)\n\tlw %a1,48(%sp)\n\tlw %a3,52(%sp)\n\tlw %a4,56(%sp)\n"+ \
            "\taddi %sp,%sp,60";
    }
    if (line[0] == "IF") {
        if (line[2] == "=="){
            temp_return = "\tbeq ";
            temp_return += storage(line[1])+",";
            temp_return += storage(line[3])+",";
            temp_return += line.back();
            return temp_return;
        }
        if (line[2] == "!="){
            temp_return = "\tbne ";
            temp_return += storage(line[1])+",";
            temp_return += storage(line[3])+",";
            temp_return += line.back();
            return temp_return;
        }
        if (line[2] == ">"){
            temp_return = "\tbgt ";
            temp_return += storage(line[1])+",";
            temp_return += storage(line[3])+",";
            temp_return += line.back();
            return temp_return;
        }
        if (line[2] == "<"){
            temp_return = "\tblt ";
            temp_return += storage(line[1])+",";
            temp_return += storage(line[3])+",";
            temp_return += line.back();
            return temp_return;
        }
        if (line[2] == ">="){
            temp_return = "\tbge ";
            temp_return += storage(line[1])+",";
            temp_return += storage(line[3])+",";
            temp_return += line.back();
            return temp_return;
        }
        if (line[2] == "<="){
            temp_return = "\tble ";
            temp_return += storage(line[1])+",";
            temp_return += storage(line[3])+",";
            temp_return += line.back();
            return temp_return;
        }
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
            else if (line[3] == "<"){
                temp_return = "\tslt ";
                temp_return += storage(line[0])+",";
                temp_return += storage(line[2])+",";
                temp_return += storage(line.back());
                return temp_return;
            }
            else if (line[3] == ">"){
                temp_return = "\tslt ";
                temp_return += storage(line[0])+",";
                temp_return += storage(line.back())+",";
                temp_return += storage(line[2]);
                return temp_return;
            }
        }

        if (line[2] == "CALL")
        {
            func_argn = 0;
            return string("")+"\taddi %sp,%sp,-60\n\tsw %ra,0(%sp)\n\tsw %t0,4(%sp)\n\tsw %t1,8(%sp)\n\tsw %t2,12(%sp)\n\tsw %t3,16(%sp)\n"+ \
                "\tsw %t4,20(%sp)\n\tsw %t5,24(%sp)\n\tsw %t6,28(%sp)\n\tsw %t7,32(%sp)\n\tsw %t8,36(%sp)\n\tsw %t9,40(%sp)\n"+ \
                "\tsw %a0,44(%sp)\n\tsw %a1,48(%sp)\n\tsw %a2,52(%sp)\n\tsw %a3,56(%sp)\n"+ \
                "\tjal "+line.back()+"\n"+ \
                "\tlw %ra,0(%sp)\n\tlw %t0,4(%sp)\n\tlw %t1,8(%sp)\n\tlw %t2,12(%sp)\n\tlw %t3,16(%sp)\n"+ \
                "\tlw %t4,20(%sp)\n\tlw %t5,24(%sp)\n\tlw %t6,28(%sp)\n\tlw %t7,32(%sp)\n\tlw %t8,36(%sp)\n\tlw %t9,40(%sp)\n"+ \
                "\tlw %a0,44(%sp)\n\tlw %a1,48(%sp)\n\tlw %a2,52(%sp)\n\tlw %a3,56(%sp)\n"+ \
                "\taddi %sp,%sp,60\n\tmove "+storage(line[0])+", %v0";
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
