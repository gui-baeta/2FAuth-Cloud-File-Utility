/*
 * Group: 28
 *     92433 - Carolina Pereira
 *     92442 - Daniela Castanho
 *     92470 - Guilherme Fontes
 */
#define GROUP_NUM 28

// User Application Prototype

#include <arpa/inet.h>
#include <cerrno>
#include <netdb.h>
#include <cstdio>
#include <stdlib.h>
#include <cstring>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fstream>
using namespace std;

#define GetCurrentDir getcwd
#define DEFAULT_ASPORT "58028"
#define DEFAULT_FSPORT "59028"
#define DEFAULT_HOST "localhost"

//setting everything to "0" instead of "" so it doesn't send empty stuff
string filename = "0", fop = "0";
string uid = "0", password = "0";
string rid = "0", tid = "0";
char input[256];
int flag = 0, isOpenFS = 0, isOpenAS = 0;
int is_logged_in = 0;
int exit_flag = 0;

/*if threads are use make struct to store tids*/

ssize_t n;
struct addrinfo hints, *res;
char buffer[128] = "";
int sockAS, sockFS, errcode = 0;
char addressAS[128], portAS[128], addressFS[128], portFS[128];

//main functions
void login();
void requestFop();
void validateCode(string vc);
void remove();
void uploadFile();
void retrieveFile();
void list();
void deleteFile();
void exit();

//auxiliary functions
string GetWorkingDir(void);
string trim(const string &base);
vector<string> spliString(string &str, int flag);
int hasFilename();
string toFop();
string readFileFromSockFS(int nleft);
string readFromSockFS();
void writeInSockAS(string message);
void writeInSockFS(string message);
void sendFileToFS(string fname);
void processInput();
void connectToAS();
void connectToFS();
void closeAS();
void closeFS();

//returns directory the app is running on
string GetWorkingDir(void){
    char buff[256];
    GetCurrentDir(buff, 256);
    string current_working_dir(buff);
    return current_working_dir;
}

string trim(const string &base) {
    string str = base;
    size_t found;
    found = str.find_last_not_of(string("\n"));
    if (found != string::npos)
        str.erase(found + 1);
    else
        str.clear();

    return str;
}

vector<string> splitString(string &str, int flag){
    vector<string> strs_vec;

    size_t start, end = 0;

    while((start = str.find_first_not_of(" ", end))!=string::npos){
        end = str.find(" ", start);
        strs_vec.push_back(str.substr(start, end-start));
    }
    /*deletes last character from last word ('\n')*/
    if(flag!=0)
        strs_vec.back() = trim(strs_vec.back());
    
    return strs_vec;
}

int hasFilename(){
    if(fop.compare(string("retrieve"))==0|| fop.compare(string("R"))==0 || fop.compare(string("upload"))==0 || fop.compare(string("U"))==0|| fop.compare(string("delete"))==0 || fop.compare(string("D"))==0)
        return 1;
    return 0;
}

//just in case the input isn't a letter in caps
string toFop(string fop){
    if(fop.compare(string("list"))==0 || fop.compare(string("l"))==0)
        return string("L");
    else if(fop.compare(string("retrieve"))==0 || fop.compare(string("r"))==0)
        return string("R");
    else if(fop.compare(string("upload"))==0 || fop.compare(string("u"))==0)
        return string("U");
    else if(fop.compare(string("delete"))==0 || fop.compare(string("d"))==0)
        return string("D");
    else if(fop.compare(string("remove"))==0 || fop.compare(string("x"))==0)
        return string("X");
    else
        return fop;
}

void verifyConnection(int err){
    if(err == EHOSTDOWN || err == ESHUTDOWN || err == ECONNRESET){
        puts("> Oops, a server is down. Terminating program.");
        exit(EXIT_FAILURE);
    }
}

string readFromSockFS(){
    //reads from sockFS until read string has '\n' in it
    //idk if this works
    char buffer[128] = "";
    int nread;
    string msg = "";

	while (msg.find_first_of(string("\n"))==string::npos) {
		nread = read(sockFS, buffer, 128);
        verifyConnection(nread);
		if (nread < 0) {
			puts("> SockFS: Error on read");
            exit(EXIT_FAILURE);
		}
        msg += buffer;
        strcpy(buffer, "");
	}
    return msg;
}

string readFileFromSockFS(int nleft){
    string msg = "";
    char buffer[1];
    bzero(buffer, sizeof(buffer));
    int nread = 0;

    int scale = nleft/5;
    int j = nleft;

    printf("> Retrieving file... [#");

    while(nleft>0){
        nread = read(sockFS, buffer, 1);
        verifyConnection(nleft);
		if (nread < 0) {
			puts("> SockFS: Error on read");
            exit(EXIT_FAILURE);
        }
        if(nleft == j){
            printf("#");
            j -= scale;
        }
        nleft -= nread;
        msg.push_back(buffer[0]);
        bzero(buffer, sizeof(buffer));
    }

    puts("##]");

    nread = read(sockFS, buffer, 1);
        verifyConnection(nread);
		if (nread < 0) {
			puts("> SockFS: Error on read");
            exit(EXIT_FAILURE);
        }
    return msg; 
}

string read_n_from_sockFS(int n){
    char buffer[n] = "";
    string msg = "";
    int err;
    if((err = read(sockFS, buffer, n))<0){
        puts("> SockFS: Error on read");
        exit(EXIT_FAILURE);
    }
    verifyConnection(err);
    msg += buffer;
    return msg;
}

int readFileSize(){
    char buffer[1] = "";
    string msg = "";
    int err;
    while(msg.find_first_of(" ")==string::npos){
        if((err = read(sockFS, buffer, 1))<0){
            puts("> SockFS: Error on read");
            exit(EXIT_FAILURE);
        }
        verifyConnection(err);
        msg += buffer;
        bzero(buffer, sizeof(char));
    }
    msg = msg.substr(0, msg.size()-1);
    return atoi(msg.c_str());
}

void writeInSockAS(string message){
    char buffer[128] = "";
    strcpy(buffer, message.c_str());
    int err;
    if((err = write(sockAS, buffer, strlen(buffer)))<0){
        puts("> SockAS: error on write");
        exit(EXIT_FAILURE);
    }
    verifyConnection(err);
}

void writeInSockFS(string message){
    char buffer[1024] = "", *ptr;
    int n, nleft, nwritten;
    ptr = strcpy(buffer, message.c_str());
	n = strlen(buffer);

	nleft = n;

	while (nleft > 0) {
		nwritten = write(sockFS, ptr, nleft);
        verifyConnection(nwritten);
		if (nwritten < 0) {
			puts("> SockFS: Error on write");
            exit(EXIT_FAILURE);
		}
		nleft -= nwritten;
		ptr += nwritten;
	}
}

string readFileToUpload(string fname){
    int nwritten;
    char buff[1];
    bzero(buff, sizeof(buff));
    string data = "";

    ifstream file(fname);

    while(file.get(buff[0])){
        data.push_back(buff[0]);
    }

    file.close();
    return data;
}

void processInput(){
    string tok[3];
    fgets(input, 256, stdin);

    string buffer = input;

    while(buffer.compare(string("exit\n"))!=0){

        if(exit_flag==1)
            return;

        vector<string> vec = splitString(buffer, 1);

        for(int i = 0; i<vec.size(); i++){
            tok[i].clear();
            tok[i].assign(vec.at(i));
        }

        //login
        if(tok[0].compare("login")==0){
            fop = tok[0];

            int size_username = tok[1].size();
            int size_password = tok[2].size();
            if(size_username==5 && size_password==8){
                uid.assign(tok[1]);
                password.assign(tok[2]);
                login();
            }
            else
                puts("> error: invalid arguments on login");
        }

        //requestFop
        else if(tok[0].compare("req")==0){
            fop.assign(toFop(tok[1]));
            if(hasFilename()==1)
                filename.assign(tok[2]);
            requestFop();
        }

        else if(tok[0].compare("val")==0){
            validateCode(tok[1]);
        }

        else if(tok[0].compare("list")==0 || tok[0].compare("l")==0 || tok[0].compare("L")==0){
            list();
        }

        else if(tok[0].compare("retrieve")==0 || tok[0].compare("r")==0 || tok[0].compare("R")==0){
            fop.assign(tok[0]);
            filename.assign(tok[1]);
            retrieveFile();
        }

        else if(tok[0].compare("upload")==0 || tok[0].compare("u")==0 || tok[0].compare("U")==0){
            fop.assign(tok[0]);
            filename.assign(tok[1]);
            uploadFile();
        }

        else if(tok[0].compare("delete")==0 || tok[0].compare("d")==0 || tok[0].compare("D")==0){
            fop.assign(tok[0]);
            filename.assign(tok[1]);
            deleteFile();
        }

        else if(tok[0].compare("remove")==0 || tok[0].compare("x")==0 || tok[0].compare("X")==0){
            remove();
        }

        else
            puts("> error: invalid arguments were given.");
        
        fgets(input, 256, stdin);
        buffer = input;
        
    }
    flag = 1;
}

void connectToAS(char address[], char port[]){
    //not tested yet

	sockAS = socket(AF_INET, SOCK_STREAM, 0);  //TCP socket
	if (sockAS == -1) {
        printf("> AS: socket error.\n");
		exit(EXIT_FAILURE);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;        
	hints.ai_socktype = SOCK_STREAM;  //TCP socket

	errcode = getaddrinfo(address, port, &hints, &res);
	if (errcode != 0) {
        printf("> AS: getaddrinfo error\n");
		exit(EXIT_FAILURE);
	}

	errcode = connect(sockAS, res->ai_addr, res->ai_addrlen);
	if (errcode == -1) {
        printf("> AS: connection error\n");
		exit(EXIT_FAILURE);
	}

    isOpenAS = 1;
}

void connectToFS(char address[], char port[]){

	sockFS = socket(AF_INET, SOCK_STREAM, 0);  //TCP socket
	if (sockFS == -1) {
        printf("> FS: socket error.\n");
		exit(EXIT_FAILURE);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;        
	hints.ai_socktype = SOCK_STREAM;  //TCP socket

	errcode = getaddrinfo(address, port, &hints, &res);
	if (errcode != 0) {
        printf("> FS: getaddrinfo error\n");
		exit(EXIT_FAILURE);
	}

	errcode = connect(sockFS, res->ai_addr, res->ai_addrlen);
	if (errcode == -1) {
        printf("> FS: connection error\n");
		exit(EXIT_FAILURE);
	}

    isOpenFS = 1;
}

void closeFS(){
    close(sockFS);
    isOpenFS=0;
}

void closeAS(){
    close(sockAS);
    isOpenAS=0;
}

void login() { 
    /*sends uid and pass to AF for verification*/
    char buffer[128] = "";

    if(isOpenAS==0){
        connectToAS(addressAS, portAS);
    }

    string msg = "LOG " + uid + " " + password + "\n";

    writeInSockAS(msg);

    if(read(sockAS, buffer, 128)<0){
        puts("> SockAS: Error on read");
        exit(EXIT_FAILURE);
    }

    if(strcmp(buffer, "RLO NOK\n")==0)
        puts("> Failed to login: password is incorrect");
    
    else if(strcmp(buffer, "RLO ERR\n")==0)
        puts("> Error on login");

    else if(strcmp(buffer, "RLO OK\n")==0){
        puts("> You are now logged in.");
        is_logged_in = 1;
    }

    else
        puts("> An unexpected error has occured.");
}

void requestFop() { 
    /*requests validation code from AF; code is sent to PD*/ 

    int Rid = rand()%9000 + 1000; //generates random request id (4 digit)
    rid.assign(to_string(Rid));
    string msg = "REQ " + uid + " " + rid + " " + fop;
    
    if(is_logged_in == 0){
        puts("> Please login first");
        return;
    }
    
    char buffer[128] = "";

    if(hasFilename()!=0)
        msg += " " + filename;
    
    msg += "\n";

    writeInSockAS(msg);

    msg = "";

    msg = readFromSockFS();
    vector<string> out = splitString(msg, 1);

    if(out[0].compare(string("ERR"))==0)
        puts("> An unexpected error has occured.");

    else if(out[1].compare(string("ELOG"))==0)
        puts("> Error: user is not logged in");
    
    else if(out[1].compare(string("EPD"))==0)
        puts("> Error: failed to reach Personal Device");

    else if(out[1].compare(string("EUSER"))==0)
        puts("> Error: invalid user identification");

    else if(out[1].compare(string("EFOP"))==0)
        puts("> Error: invalid operation");

    else if(out[1].compare(string("ERR"))==0)
        puts("> Invalid format");

    else if(out[1].compare(string("OK"))==0){
        puts("> Please validate the code sent to your personal device.");
    }
    else
        puts("> An unexpected error has occured.");
}

void validateCode(string vc) {
    /*validates code on AF*/
    /*correspondent Fop can now be performed by FS*/

    string msg = "AUT " + uid + " " + rid + " " + vc + "\n";
    char buffer[128] = "";

    if(is_logged_in == 0){
        puts("> Please login first");
        return;
    }

    if(rid.compare(string(""))==0){
        puts("> Please request operation first");
        return;
    }

    if(vc.size() != 4){
        puts("> Validation code must have 4 digits.");
        return;
    }    

    writeInSockAS(msg);

    int err;
    if((err = read(sockAS, buffer, 128))<0){
        verifyConnection(err);
        puts("> SockAS: Error on read");
        exit(EXIT_FAILURE);
    }

    verifyConnection(err);

    if(strcmp(buffer, "RAU 0\n")==0){
        puts("> Error: code validation failed");
        return;
    }
    
    else if(strcmp(buffer, "ERR\n")!=0 && strcmp(buffer, "")!=0){
        string message = buffer;
        vector<string> output = splitString(message, 1);
        tid.assign(output[1]);
        rid.assign("0");
        printf("> Success! TID: %s.\n", output[1].c_str());
    }

    else
        puts("> An unexpected error has occured.");

}

void list() { /*displays numeric list of files from FS*/
    connectToFS(addressFS, portFS);
    
    string msg = "LST " + uid + " " + tid + "\n";

    if(is_logged_in == 0){
        puts("> Please login first");
        return;
    }

    if(tid.compare(string(""))==0){
        puts("> Please request operation first");
        return;
    }

    writeInSockFS(msg);
    msg = readFromSockFS();
    
    vector<string> vec = splitString(msg, 1);

    //form RLS status
    if(vec[0].compare(string("ERR"))==0)
        puts("> Error: unexpected protocol message received");

    else if(vec[1].compare(string("EOF"))==0)
        puts("> No files available.");
    
    else if(vec[1].compare(string("NOK"))==0)
        puts("> Invalid user identification");
    
    else if(vec[1].compare(string("INV"))==0)
        puts("> Authentication Server: validation error");
    
    else if(vec[1].compare(string("ERR"))==0)
        puts("> Request is not correctly formulated");
    
    //form RLS N[ Fname Fsize]
    else{
        int n = std::stoi(vec[1]);
        int iterator = 1;
        int j = iterator+1;

        //print numbered list
        while(iterator <= n){
            printf("%d. %s\n", iterator, vec[j].c_str());
            iterator++;
            j += 2; //skip file sizes
        }
        filename = "";
        tid = "";
    }

    closeFS();
}

void retrieveFile() { /*retrieves selected file from FS*/ 
    connectToFS(addressFS, portFS);
    //RTV UID TID Fname

    string msg = "RTV " + uid + " " + tid + " " + filename + "\n";
    
    if(is_logged_in == 0){
        puts("Please login first");
        return;
    }

    if(tid.compare(string(""))==0){
        puts("Please request operation first");
        return;
    }
    
    writeInSockFS(msg);

    msg = "";

    //msg = readFileFromSockFS(1);
    msg = read_n_from_sockFS(7);
    //puts(msg.c_str());
    vector<string> vec = splitString(msg, 0);

    if(vec[0].compare(string("ERR"))==0)
        puts("Error: unexpected protocol message received");

    else if(vec[1].compare(string("EOF"))==0)
        puts("File is not available");
    
    else if(vec[1].compare(string("NOK"))==0)
        puts("Invalid user identification");
    
    else if(vec[1].compare(string("INV"))==0)
        puts("Authentication Server: validation error");
    
    else if(vec[1].compare(string("ERR"))==0)
        puts("Request is not correctly formulated");
    
    //form RRT status [Fsize data]
    else{
        int nleft = readFileSize();
    
        string path = GetWorkingDir() + "/" + filename;
        filename.assign(path);
        int i;
        string output = "";

        msg = "";
        if(nleft > 0)
            output = readFileFromSockFS(nleft);

        if(output.size() < nleft)
            puts("Warning: couldn't retrieve all data. Partial data written to file.");

        else
            puts("Retrieve operation successful.");
        
        ofstream file(path, std::ios::out | std::ios::binary);
        i = 0;
        while(i < nleft){
            file << output.at(i);
            i++;
        }
        file.close();

        msg = "";
        printf("The file is stored in %s\n", path.c_str());
        filename = "";
        tid = "";
    
    }

    closeFS();
}

void uploadFile() { /*uploades selected file to FS*/ 
    connectToFS(addressFS, portFS);

    //UPL UID TID Fsize data
    string msg = "UPL " + uid + " " + tid + " " + filename + " ";

    if(is_logged_in == 0){
        puts("> Please login first");
        return;
    }

    if(tid.compare(string(""))==0){
        puts("> Please request operation first");
        return;
    }
    
    string data = "";
    data = readFileToUpload(filename);

    msg += to_string(data.size()) + " ";

    writeInSockFS(msg);
    
    printf("> Uploading... [#"); //loading bar

    int i = 0, j = 0;
    int scale = data.size()/5;
    int err;
    while(i < data.size()){
        char buff[1] = "";
        buff[0] = data.at(i++);
        if((err = write(sockFS, buff, 1)) < 0){
            puts("> SockFS: error on write.");
            exit(EXIT_FAILURE);
        }
        verifyConnection(err);
        if(i == j){
            printf("#");
            j +=scale;
        }
    }

    puts("##]");

    //send last "\n"
    if((err = write(sockFS, "\n", sizeof("\n")))<0){
        puts("> SockFS: error on write.");
        exit(EXIT_FAILURE);
    }
    verifyConnection(err);

    msg = readFromSockFS();

    vector<string> vec = splitString(msg, 1);

    if(vec[1].compare(string("NOK"))==0)
        puts("> Invalid user identification.");
    else if(vec[1].compare(string("DUP"))==0)
        puts("> Error: file has already been uploaded.");
    else if(vec[1].compare(string("FULL"))==0)
        puts("> Error: maximum number of uploads reached.");
    else if(vec[1].compare(string("INV"))==0)
        puts("> Authentication Server: validation error..");
    else if(vec[1].compare(string("ERR"))==0)
        puts("> Request is not correctly formulated.");
    else if(vec[0].compare(string("ERR"))==0)
        puts("> An unexpected error has occured.");
    else{
        puts("> Upload sucessful.");
        filename = "";
        tid = "";
    }

    closeFS();
}

void deleteFile() { /*deletes selected file from FS*/ 
    connectToFS(addressFS, portFS);

    //DEL UID TID Fname
    string msg = "DEL " + uid + " " + tid + " " + filename + "\n";

    if(is_logged_in == 0){
        puts("> Please login first");
        return;
    }

    if(tid.compare(string(""))==0){
        puts("> Please request operation first");
        return;
    }

    writeInSockFS(msg);

    msg = "";

    msg = readFromSockFS();

    vector<string> vec = splitString(msg, 1);

    if(vec[0].compare(string("ERR"))==0)
        puts("> An unexpected error has occured.");

    else if(vec[1].compare(string("EOF"))==0)
        puts("> File is not available");
    
    else if(vec[1].compare(string("NOK"))==0)
        puts("> Invalid user identification");
    
    else if(vec[1].compare(string("INV"))==0)
        puts("> Authentication Server: validation error");
    
    else if(vec[1].compare(string("ERR"))==0)
        puts("> Request is not correctly formulated");
    else{
        puts("> Delete operation successful.");
        filename = "";
        tid = "";
    }
}

void remove() {
    /*removes all files and directories of user from FS*/
    connectToFS(addressFS, portFS);

    string msg = "REM " + uid + " " + tid + "\n";

    if(is_logged_in == 0){
        puts("> Please login first");
        return;
    }

    if(tid.compare(string(""))==0){
        puts("> Please request operation first");
        return;
    }

    writeInSockFS(msg);

    msg = readFromSockFS();

    if(msg.compare(string("RRM NOK\n"))==0)
        puts("> Error: invalid user");

    else if(msg.compare(string("RRM INV\n"))==0)
        puts("> Error: invalid TID");

    else if(msg.compare(string("RRM ERR\n"))==0)
        puts("> Error: request is not correctly formulated");

    else if(msg.compare(string("ERR\n"))==0)
        puts("> Error: unexpected protocol message received");
    
    else{
        puts("> Remove operation successful");
        exit_flag = 1;
    }

    closeFS();
}

void exit() {
    /*closes any open connections and terminates the program*/
    if(isOpenAS==1){
        closeAS();
    }
    if(isOpenFS==1){
        closeFS();
    }
    is_logged_in = 0;
    puts("> Exiting...");
}

int main(int argc, char *argv[]){
    /*[-n ASIP] [-p ASport] [-m FSIP] [-q FSport]*/

    string asip = DEFAULT_HOST, asport = DEFAULT_ASPORT;
    string fsip = DEFAULT_HOST, fsport = DEFAULT_FSPORT;

    if(argc > 9) puts("> invalid arguments were given");

    else{
        for(int i = 1; i < argc; i++){
            if(strcmp("-n", argv[i])==0){
                asip.assign(argv[++i]);
                continue;
            }
            else if(strcmp("-p", argv[i])==0){
                asport.assign(argv[++i]);
                continue;
            }
            else if(strcmp("-m", argv[i])==0){
                fsip.assign(argv[++i]);
                continue;
            }
            else if(strcmp("-q", argv[i])==0){
                fsport.assign(argv[++i]);
                continue;
            }
            else{
                puts("> invalid arguments were given.");
            }
        }
    }

    strcpy(addressAS, asip.c_str());
    strcpy(portAS, asport.c_str());
    strcpy(addressFS, fsip.c_str());
    strcpy(portFS, fsport.c_str());
    
    //connectToAS(addressAS, portAS);

    processInput();
    
    exit();
    puts("> Exit successful");

    return 0;
}
