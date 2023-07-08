#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
using namespace std;

extern "C"{void asm_print(const char *, const int);}

#pragma pack(1) // 强制按一字节对齐
struct BPB{                                  // boot扇区
    unsigned short BPB_BytsPerSec; // 每扇区字节数,一般为512
    unsigned char BPB_SecPerClus;  // 每簇占用的扇区数一般为1
    unsigned short BPB_RsvdSecCnt; // Boot占用的扇区数，1
    unsigned char BPB_NumFATs;     // FAT表的记录数，一般为2
    unsigned short BPB_RootEntCnt; // 最大根目录文件数
    unsigned short BPB_TotSec16;   // 逻辑扇区总数
    unsigned char BPB_Media;       // 媒体描述符
    unsigned short BPB_FATSz16;    // 每个FAT占用扇区数
    unsigned short BPB_SecPerTrk;  // 每个磁道占用扇区数
    unsigned short BPB_NumHeads;   // 磁头数
    unsigned int BPB_HiddSec;      // 隐藏扇区数
    unsigned int BPB_TotSec32;     // 如果BPB_TotSec16是0，则在这记录扇区总数
};

struct RootEntry{// 根目录区的一个目录项
    char DIR_Name[11];          // 文件名8字节，扩展名3字节
    unsigned char DIR_Attr;     // 文件属性 只读/写/执行
    char Reserve[10];           // 保留位
    unsigned short DIR_WrtTime; // 最后一次写入时间
    unsigned short DIR_WrtDate; // 最后一次写入日期
    unsigned short DIR_FstClus; // 此条目对应的开始簇数
    unsigned int DIR_FileSize;  // 文件大小
};

#pragma pack()
int BytsPerSec;
int SecPerClus;
int RsvdSecCnt;
int NumFATs;
int RootEntCnt;
int FATSz;
string str_dian = "./";

class Node{ // 文件或目录节点
public:
    string name, path;    
	bool isFile = false, point = false;
    int dirNum = 0, fileNum = 0;
 	unsigned int size;
    vector<Node *> sons;
    char *content = new char[10000]{};
    Node(string n, string p, bool file, bool apoint):name(n), path(p), isFile(file), point(apoint) {}
};

void addPoint(Node *node, Node *son){
    Node *point = new Node(".", son->path + "./", false, true);
    point->sons.push_back(son);
    son->sons.push_back(point);
    Node *doublePoint = new Node("..", son->path + "../", false, true);
    doublePoint->sons.push_back(node);
    son->sons.push_back(doublePoint);
}

void myprint(const char *ptr) { asm_print(ptr, strlen(ptr)); }

void readBPB(FILE *fat12, struct BPB *bpbPtr){
    int ok = fseek(fat12, 11, SEEK_SET); //引导扇区的第12个字节：BPB信息的开始
    ok = fread(bpbPtr, 1, 25, fat12); //读取25个字节
}

int getFATVal(FILE *fat12, int num){ //num 要读取的FAT项的编号
    int base = RsvdSecCnt * BytsPerSec; // FAT1的偏移字节
    int position = base + num * 3 / 2;  // FAT项的偏移字节
    // 先读FAT项所在的两个字节
    unsigned short twoBytes;
    unsigned short *twobytesPtr = &twoBytes;
    int ok = fseek(fat12, position, SEEK_SET);
    ok = fread(twobytesPtr, 1, 2, fat12);
    if (num % 2 == 0){ // 奇偶FAT项处理方式不同，分类处理
        twoBytes = twoBytes << 4;
        return twoBytes >> 4;
    }else return twoBytes >> 4;
}

void getCtx(FILE *fat12, int startClus, Node *node){ // 获取文件内容 startClus 文件开始簇号   node 文件节点
    int base = BytsPerSec * (RsvdSecCnt + FATSz * NumFATs + (RootEntCnt * 32 + BytsPerSec - 1) / BytsPerSec); //数据区的起始字节 保留扇区 + FAT + 根目录区   根目录扇区数可能不足一个整数，相应处理用俩向上取整
    int currentClus = startClus;
    // 用value进行不同簇的读取（超过512字节）
    int value = 0;
    char *p = node->content;
    if (startClus == 0) return;
    while (value < 0xFF8){ // 0xFF8 文件的最后一个簇
        value = getFATVal(fat12, currentClus); // 获取下一个簇
        if (value == 0xFF7){
            myprint("bad cluster!\n");
            break;
        }
        int length = SecPerClus * BytsPerSec;
        char *str = (char *)malloc(length);
        char *content = str;
        int startByte = base + (currentClus - 2) * length; // -2 获得当前簇相对于数据区的相对偏移
        int ok = fseek(fat12, startByte, SEEK_SET);
        ok = fread(content, 1, length, fat12);
        for (int i = 0; i < length; i++, p++) *p = content[i];
        free(str);
        currentClus = value;
    }
}

void getSon(FILE *fat12, int startClus, Node *node){
    int base = BytsPerSec * (RsvdSecCnt + FATSz * NumFATs + (RootEntCnt * 32 + BytsPerSec - 1) / BytsPerSec); // 数据区第一个簇（2号簇）的偏移字节量
    int currentClus = startClus;
    int value = 0;
    while (value < 0xFF8){
        value = getFATVal(fat12, currentClus); // 查FAT表获取下一个簇号
        if (value == 0xFF7){
            myprint("bad cluster!\n");
            break;
        }
        int startByte = base + (currentClus - 2) * SecPerClus * BytsPerSec;
        int ok, count = 0, size = SecPerClus * BytsPerSec; // 每簇的字节数
        while (count < size){
            RootEntry sonEntry;
            RootEntry *sonEntryPtr = &sonEntry;
            ok = fseek(fat12, startByte + count, SEEK_SET); // 读取目录项，将目录项的信息放在
            ok = fread(sonEntryPtr, 1, 32, fat12);
            count += 32;
            if (sonEntryPtr->DIR_Name[0] == '\0') continue; // 跳过空的项
            bool validName = true;
            for (int i = 0; i < 11; i++) // 不是英文字母、数字、空格,跳过不关心的项
                if (!(((sonEntryPtr->DIR_Name[i] >= 'a') && (sonEntryPtr->DIR_Name[i] <= 'z')) || ((sonEntryPtr->DIR_Name[i] >= 'A') && (sonEntryPtr->DIR_Name[i] <= 'Z')) ||
                      ((sonEntryPtr->DIR_Name[i] >= '0') && (sonEntryPtr->DIR_Name[i] <= '9')) || (sonEntryPtr->DIR_Name[i] == ' '))){
                    validName = false;
                    break;
                }
            if (!validName) continue;
            if ((sonEntryPtr->DIR_Attr & 0x10) == 0) { // file
                char temp[12];
                int length = -1;
                for (int i = 0; i < 11; i++)
                    if (sonEntryPtr->DIR_Name[i] != ' ') temp[++length] = sonEntryPtr->DIR_Name[i];
                    else  {
            			temp[++length] = '.';
                        while (i < 10 && sonEntryPtr->DIR_Name[i + 1] == ' ')i++;
                    }
                temp[++length] = '\0';
                Node *son = new Node(temp, node->path + temp + "/", true, false);
                node->sons.push_back(son);
                son->size = sonEntryPtr->DIR_FileSize;
                node->fileNum++;
                getCtx(fat12, sonEntryPtr->DIR_FstClus, son);
            }else{ // director
                char temp[12];
                int length = -1;
                for (int i = 0; i < 11; i++)
                    if (sonEntryPtr->DIR_Name[i] != ' ') temp[++length] = sonEntryPtr->DIR_Name[i];
                    else temp[++length] = '\0';
                Node *son = new Node(temp, node->path + temp + "/", false, false);
                node->sons.push_back(son);
                node->dirNum++;
                addPoint(node, son);
                getSon(fat12, sonEntryPtr->DIR_FstClus, son);
            }
        }
        currentClus = value; // next clus
    }
}

void readEntry(FILE *fat12, struct RootEntry *rootEntryPtr, Node *node){ //读取目录项信息，组织文件系统在node中
    addPoint(node, node);
    char temp[12];
    int ok = 0, base = (RsvdSecCnt + NumFATs * FATSz) * BytsPerSec; // 计算根目录区首字节偏移字节数
    for (int i = 0; i < RootEntCnt; i++){ //读取根目录区的每个目录项，保存在rootEntryPtr
        ok = fseek(fat12, base, SEEK_SET);
        ok = fread(rootEntryPtr, 1, 32, fat12);
        base += 32;  //下一个目录项，每个目录项32bytes
        if (rootEntryPtr->DIR_Name[0] == '\0') continue;
        bool validName = true;
        for (int j = 0; j < 11; j++) // 不是英文字母、数字、空格
            if (!(((rootEntryPtr->DIR_Name[j] >= 'a') && (rootEntryPtr->DIR_Name[j] <= 'z')) || ((rootEntryPtr->DIR_Name[j] >= 'A') && (rootEntryPtr->DIR_Name[j] <= 'Z')) ||
                  ((rootEntryPtr->DIR_Name[j] >= '0') && (rootEntryPtr->DIR_Name[j] <= '9')) || (rootEntryPtr->DIR_Name[j] == ' '))){
				validName = false;
                break;
            }
        if (!validName) continue;

        if ((rootEntryPtr->DIR_Attr & 0x10) == 0){ // file   DIR_Attr 第五位表示是文件还是目录 如果是文件，为0
		    int length = -1;
            for (int j = 0; j < 11; j++)
                if (rootEntryPtr->DIR_Name[j] != ' ') temp[++length] = rootEntryPtr->DIR_Name[j];
                else{
                    temp[++length] = '.';
                    while (j < 10 && rootEntryPtr->DIR_Name[j + 1] == ' ') j++;
                }
            temp[++length] = '\0';
            Node *son = new Node(temp, node->path + temp + "/", true, false);
            son->size = rootEntryPtr->DIR_FileSize;
            node->sons.push_back(son);
            node->fileNum++;
            getCtx(fat12, rootEntryPtr->DIR_FstClus, son);  // DIR_FstClus 文件的开始簇数
        }else{ // director
            int length = -1;
            for (int j = 0; j < 11; j++)
                if (rootEntryPtr->DIR_Name[j] != ' ') temp[++length] = rootEntryPtr->DIR_Name[j];
                else{
                    temp[++length] = '\0';
                    break;
                }
            Node *son = new Node(temp, node->path + temp + "/", false, false);
            addPoint(node, son);
            node->sons.push_back(son);
            node->dirNum++;
            getSon(fat12, rootEntryPtr->DIR_FstClus, son);
        }
    }
}

void ls(Node *node, bool hasl){
    Node *p = node;
    if (p->isFile) return;
	if (p->path.length() > 2 && p->path.substr(p->path.length() - 2, 2).compare(str_dian) == 0) p = p->sons[0];
	string str_print = hasl ? p->path + " " + to_string(p->dirNum) + " " + to_string(p->fileNum) + ":\n" : p->path + ":\n";
	myprint(str_print.c_str());
	str_print.clear();
	Node *q;
	int length = p->sons.size();
	for (int i = 0; i < length; i++){
		q = p->sons[i];
		if (q->isFile == false){
			if (hasl){
				str_print = q->point ? "\033[31m" + q->name + "\033[0m" + "\n" : "\033[31m" + q->name + "\033[0m" + "  " + to_string(q->dirNum) + " " + to_string(q->fileNum) + "\n";
			}else  str_print = "\033[31m" + q->name + "\033[0m" + "  ";   
		}else str_print = hasl ? q->name + "  " + to_string(q->size) + "\n" : q->name + "  "; 
		myprint(str_print.c_str());
		str_print.clear();
	}
	myprint("\n");
	for (int i = 0; i < length; i++) if (!(p->sons[i]->point)) ls(p->sons[i], hasl);		
}

int ls_p(Node *node, string s, bool hasl){
    if (s.length() < node->path.length()) return 0;
    if (s.compare(node->path) == 0)
        if (node->isFile) return 2;
        else{
            ls(node, hasl);
            return 1;
        }
    string temp = s.substr(0, node->path.length());
    if (temp.compare(node->path) == 0){
        if (node->path.length() > 2 && node->path.substr(node->path.length() - 2, 2).compare(str_dian) == 0) node = node->sons[0];
        s = node->path + s.substr(temp.length(), s.length() - temp.length());
        for (Node *p : node->sons){
            int temp = ls_p(p, s, hasl);
            if (temp > 0) return temp;
        }
    }
    return 0;
}

int cat(Node *node, string s){
    if (s.length() < node->path.length()) return 0;
    if (s.compare(node->path) == 0)
        if (node->isFile){
            if (node->content[0] != 0){
                myprint(node->content);
                myprint("\n");
            }
            return 1;
        }else return 2;
    string temp = s.substr(0, node->path.length());
    if (temp.compare(node->path) == 0){
        if (node->path.length() > 2 && node->path.substr(node->path.length() - 2, 2).compare(str_dian) == 0) node = node->sons[0];
        s = node->path + s.substr(temp.length(), s.length() - temp.length());
        for (Node *p : node->sons){
            int temp = cat(p, s);
            if (temp > 0) return temp;
        }
    }
    return 0;
}

vector<string> split(const string &str, const string &delim){
    vector<string> res;
    char *strs = new char[str.length() + 1];
    strcpy(strs, str.c_str());
    char *d = new char[delim.length() + 1];
    strcpy(d, delim.c_str());
    char *p = strtok(strs, d);
    while (p){
        string s = p;
        if (s.compare(" ") != 0 && s.compare("") != 0 && s.compare("\n") != 0) res.push_back(s);
        p = strtok(NULL, d);
    }
    return res;
}

int main(){
    FILE *fat12 = fopen("a.img", "rb"); // 打开fat12镜像文件
    struct BPB bpb;
    struct BPB *bpbPtr = &bpb;
    readBPB(fat12, bpbPtr); // 读取boot扇区
    BytsPerSec = bpb.BPB_BytsPerSec;
    SecPerClus = bpb.BPB_SecPerClus;
    RsvdSecCnt = bpb.BPB_RsvdSecCnt;
    NumFATs = bpb.BPB_NumFATs;
    RootEntCnt = bpb.BPB_RootEntCnt;
    FATSz = bpb.BPB_FATSz16 != 0 ? bpb.BPB_FATSz16 : bpb.BPB_TotSec32;
    struct RootEntry rootEntry;
    struct RootEntry *rootEntryPtr = &rootEntry;
    Node *root = new Node("", "/", false, false);
    readEntry(fat12, rootEntryPtr, root);
    while (true){
        string input;
        myprint(">");
        getline(cin, input);
        vector<string> inputList = split(input, " ");
        if (inputList[0].compare("exit") == 0) {
            myprint("Exit\n");
            break;
        } else if (inputList[0].compare("ls") == 0) {
            if (inputList.size() == 1){ls(root, false); }
            else {
                bool has_l = false;
                bool hasPath = false;
                bool flag = true;
                string *path = NULL;
                for (int i = 1; i < inputList.size(); ++i)
                    if (inputList[i].length() > 0 && inputList[i][0] == '-'){
                        if (inputList[i].length() >= 2) {
                            for (int j = 1; j < inputList[i].length(); ++j){
                                if (inputList[i][j] != 'l') {
                                    myprint("wrong parameter!\n");
                                    flag = false;
                                    break;
                                }
                            }
                            if (!flag) break;
                            has_l = true;
                        }else{
                            myprint("wrong parameter!\n");
                            flag = false;
                            break;
                        }
                    }else{
                        if (!hasPath) {
                            hasPath = true;
                            if (inputList[i].length() == 0) {
                                myprint("invalid path!\n");
                                flag = false;
                                break;
                            }
                            inputList[i] = inputList[i][0] == '/' ? inputList[i] : "/" + inputList[i];
                            inputList[i] = inputList[i][inputList[i].length() - 1] == '/' ? inputList[i] : inputList[i] + "/";
                            path = &inputList[i];
                        } else{
                            myprint("too many path!\n");
                            flag = false;
                            break;
                        }
                    }
                if (!flag) continue;
                int valid = 0;
                if (has_l && !hasPath) {
                    valid = 1;
                    ls(root, true);
                }else if (hasPath) valid = ls_p(root, *path, has_l);
                else {
                    myprint("invaild command!\n");
                    continue;
                }
                if (valid == 0)  myprint("no such path!\n");
                else if (valid == 2) myprint("can not open!\n");
            }
        }else if (inputList[0].compare("cat") == 0) {
            if (inputList.size() <= 1){myprint("no parameter wrong!\n");}
            else if (inputList.size() == 2 && inputList[1][0] != '-'){ // cat path
                int valid = 0;
                inputList[1] = inputList[1][0] == '/' ? inputList[1] : "/" + inputList[1];
                inputList[1] = inputList[1][inputList[1].length() - 1] == '/' ? inputList[1] : inputList[1] + "/";
                valid = cat(root, inputList[1]);
                if (valid == 0){ myprint("no such path!\n"); }
                if (valid == 2){myprint("can not open!\n");}
            } else if (inputList.size() == 2 && inputList[1][0] == '-') {  myprint("wrong parameter!\n");}
            else{ myprint("too many parameter!\n");}
        }else{ myprint("command error!\n");}
    }
    fclose(fat12);
    return 0;
}