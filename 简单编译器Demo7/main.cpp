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
map<string,string> exvariables;
vector<string> func_name;
map<string,string> table;
map<string,int> reg_ok;
size_t func_argn;
size_t func_param;
string func_stack;

void printERR(char* str){
    cout << str << endl;
    exit(-1);
}

void Load_Sym(string Inter){
    regex temp_re("temp\\d+");
    regex var_re("var\\d+");
    regex exvar_re("[VAR,ARRAY] exvar\\d+(.*?)<+(.*?)>");
    regex fun_re("FUNCTION (.*?) -");
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

    iter = Inter.begin();
    iterEnd = Inter.end();
    while (regex_search(iter,iterEnd,result,exvar_re)){
        string num=result[2];
        temp_line = result[0];
        strtok((char*)temp_line.c_str()," ");
        temp_line = strtok(NULL," ");
        temp_line = strtok((char*)temp_line.c_str(),"(");
        exvariables[temp_line] = num.substr(1);
        iter = result[0].second;
    }   

    iter = Inter.begin();
    iterEnd = Inter.end();
    while (regex_search(iter,iterEnd,result,fun_re)){
        temp_line = result[1];
        func_name.emplace_back(temp_line);
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
    for (auto &it:table) 
        keys.emplace_back(it.first); 
    for (auto &key:keys){ 
        if (key.find("temp")!=string::npos && find(variables.begin(),variables.end(),key) == variables.end()){
            reg_ok[table[key]] = 1;
            table.erase(key);
        }
    }
}

bool Check_arrVarList(const string& str){
	regex r("<+[[:digit:]]*(var[[:alpha:]])>");
	if (regex_search(str, r)) {
		return true;
	}else{
		return false;
	}
}

vector<string> Get_arrVarList(const string& str){
    auto res = vector<string>();
    std::regex r("<+(.*?)>");

    sregex_iterator pos(str.cbegin(), str.cend(), r), end;
    for(; pos!=end; ++pos){
        res.push_back(pos->str(1));
        cout << pos->str(1) << endl;
    }

    return res;
}

string Get_R(const string& str){ 
    for (auto it = variables.begin();it!=variables.end();++it){ 
        if (*it == str){ 
            it = variables.erase(it);
            break;
        }
    } 

    if (table.find(str) != table.end()){
        return "%"+table[str];
    }
    else{
        vector<string> keys;
        for (auto &it:table) 
            keys.emplace_back(it.first); 
        
        for (const auto & reg : regs){ 
            if(reg_ok[reg] == 1){ 
                table[str] = reg; 
                reg_ok[reg] = 0; 
                return "%"+reg;
            }
        } 
        
        printERR("临时变量个数过多");
    }
}

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

string storage(const string& str){
    if(!strncmp(str.c_str(),"temp",4)){
        return Get_R(str);
    }
    else if(!strncmp(str.c_str(),"var",3)){
        return Get_S(str);
    }
    else if(!strncmp(str.c_str(),"exvar",5)){
        return Get_D(str);
    }
}

string translate(string temp_str){ 
    vector<string> line;
    string temp_res;
    stringstream input(temp_str); 
    while (input>>temp_res) 
        line.emplace_back(temp_res); 

    if(line.size()==0){
        return " "; 
    }

    string temp_return;
    if(line[0] == "LABEL"){
        if(!strncmp(line[1].c_str(),"label",5)){
            return "."+line[1]+":";
        }
        else{
            return "\taddq $"+func_stack+",%rsp\n\tpopq %rbp\n\tret\n\t.cfi_endproc\n"+"."+line[1]+":\n";
        }
    }
    if (line[0] == "GOTO"){
        return "\tjmp ."+line[1];
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
    if (line[1] == ":=") {
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
            return temp_return;
        }
        if (line.size() == 3){
            if (line[2][0] == '#'){
                temp_return = "\tmovq $"+line.back().substr(1)+","+Get_R(line[0]);
            }
            else if(line[2][0] == '&' || line[2][0] == '*' || line[0][0] == '*'){
                if(line[2][0] == '&'){
                    temp_return =  "\tleaq "+storage(line[2].substr(1))+",";
                    temp_return += Get_R(line[0])+"\n";
                    temp_return += "\tmovq "+Get_R(line[0])+",";
                    temp_return += storage(line[0]);  
                }
                else if(line[2][0] == '*' && line[0][0] == '*'){
                    temp_return = "\txor %rax,%rax";
                    temp_return += "\n\tmovq "+storage(line[0].substr(1))+","+Get_R(line[0].substr(1));
                    temp_return += "\n\tmovq "+storage(line[2].substr(1))+","+Get_R(line[2].substr(1));
                    temp_return += "\n\tmovq ("+Get_R(line[2].substr(1))+"),"+Get_R(line[2].substr(1));
                    temp_return += "\n\tmovq "+Get_R(line[2].substr(1))+",("+Get_R(line[0].substr(1))+")";
                }
                else if(line[2][0] == '*' && !strncmp(line[0].c_str(),"var",3)){
                    temp_return = "\txor %rax,%rax";
                    temp_return += "\n\tmovq "+storage(line[2].substr(1))+",%rax";
                    temp_return += "\n\tmovq (%rax),%rax";
                    temp_return += "\n\tmovq %rax,"+storage(line[0]);
                }  
                else if(line[0][0] == '*' && !strncmp(line[2].c_str(),"var",3)){
                    temp_return = "\txor %rax,%rax";
                    temp_return += "\n\tmovq "+storage(line[0].substr(1))+",%rax";
                    temp_return += "\n\tmovq "+storage(line[2])+","+Get_R(line[2]);
                    temp_return += "\n\tmovq "+Get_R(line[2])+",(%rax)";
                }
                else if(line[0][0] == '*' && !strncmp(line[2].c_str(),"temp",4)){
                    temp_return = "\txor %rax,%rax";
                    temp_return += "\n\tmovq "+storage(line[0].substr(1))+",%rax";
                    temp_return += "\n\tmovq "+storage(line[2])+",(%rax)";
                } 
            }    
            else if((!strncmp(line[2].c_str(),"var",3) || !strncmp(line[2].c_str(),"exvar",5)) && 
                (!strncmp(line[0].c_str(),"var",3)) || (!strncmp(line[0].c_str(),"exvar",5))){
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

        if (line[2] == "CALL")
        {
            func_argn = 0;
            temp_return =  "\tcall "+line.back();
            temp_return += "\n\tmovq %rax,"+storage(line[0]);
            return temp_return;
        }
    }
    return " ";
}

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

int main(){
    for (const auto & reg : regs) //初始化，所有寄存器都可用
        reg_ok[reg] = 1;

    string filename = "./inter.txt";
    string Inter = Load_Inter(filename);//读取中间代码

    Load_Sym(Inter);//第一遍扫描，记录所有符号
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
