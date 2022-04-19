#ifndef FS_SERVER
#define FS_SERVER

#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <cerrno>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <errno.h>
#include <exception>
#include <fstream>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <queue>
#include <signal.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#define GROUP_NUM 28

#define ASPORT_DEFAULT 58000
#define FSPORT_DEFAULT 59000

#define UID_LEN 5
#define TID_LEN 4
#define RID_LEN 4
#define VC_LEN 4
#define PASS_LEN 8
#define FSIZE_LEN 10

#define MAX_FILES 15

#define MAX_BUF 1024
#define EQUAL 0
#define NONE -1
#define LOOP 1

#define PATH_EXISTS 0

using namespace std;

/* --------------------------- Custom Exceptions ---------------------------- */

struct InvalidCommand : public exception {
  private:
    string error_msg;

  public:
    InvalidCommand(string error_msg_ = string("Command isn't valid"))
        : error_msg(error_msg_) {}

    const char *what() const throw() { return error_msg.c_str(); }
};

struct InvalidOperationArg : public exception {
  private:
    string error_msg;

  public:
    InvalidOperationArg(string error_msg_ = string("Command wasn't valid"))
        : error_msg(error_msg_) {}

    const char *what() const throw() { return error_msg.c_str(); }
};

struct MaxFilesReached : public exception {
  public:
    const char *what() const throw() {
        return "Max 15 files for this user reached!";
    }
};

struct NoFiles : public exception {
  public:
    const char *what() const throw() { return "User doesn't have any files!"; }
};

struct FileNotFound : public exception {
  public:
    const char *what() const throw() { return "File not found!"; }
};

struct FileAlreadyExists : public exception {
  public:
    const char *what() const throw() { return "File already exists!"; }
};

struct RequestUnsuccessful : public exception {
  public:
    const char *what() const throw() { return "Request wasn't successful!"; }
};

struct UIDDoesNotExist : public exception {
  public:
    const char *what() const throw() { return "User ID doesn't exist!"; }
};

/* ---------------------------- Semaphore class ----------------------------- */

class Semaphore {
  private:
    mutex mtx;
    condition_variable cv;
    int count;

  public:
    Semaphore(int count_ = 0) : count(count_) {}

    void set_count(int count) { this->count = count; }

    inline void release() {
        unique_lock<mutex> lock(this->mtx);
        this->count++;
        // release the mutex lock the waiting thread
        this->cv.notify_one();
    }
    inline void acquire() {
        unique_lock<mutex> lock(this->mtx);
        while (this->count == 0) {
            // wait on the mutex until notify is called
            this->cv.wait(lock);
        }
        count--;
    }

    virtual ~Semaphore() {}
};

/* ------------------------------- Data class ------------------------------- */

class Data {
  private:
    int fsport, asport;
    string asip;
    bool verbose_mode;

  public:
    Data(int fsport_ = FSPORT_DEFAULT + GROUP_NUM,
         int asport_ = ASPORT_DEFAULT + GROUP_NUM,
         string asip_ = string("127.0.0.1"), bool verbose_mode_ = false)
        : fsport(fsport_), asport(asport_), asip(asip_),
          verbose_mode(verbose_mode_) {}

    int get_fsport() { return this->fsport; }
    void set_fsport(int fsport) { this->fsport = fsport; }
    int get_asport() { return this->asport; }
    void set_asport(int asport) { this->asport = asport; }
    string get_asip() { return this->asip; }
    void set_asip(string asip) { this->asip.assign(asip); }
    bool get_vrbs_mode() { return this->verbose_mode; }
    void set_vrbs_mode(bool verbose_mode) { this->verbose_mode = verbose_mode; }
    void debug_print() {
        printf("FSport: %d\nASip: %s\nASport: %d\nVerbose mode: %s\n",
               this->fsport, this->asip.c_str(), this->asport,
               (this->verbose_mode ? "true" : "false"));
    }

    virtual ~Data() {}
};

/* ------------------------------- File class ------------------------------- */

class File {
  private:
    string name;
    size_t size;
    string path;

  public:
    File(string name_) : name(name_) {}
    File(string name_, size_t size_, string path_)
        : name(name_), size(size_), path(path_) {}

    string get_name() { return this->name; }
    size_t get_size() { return this->size; }
    string get_path() { return this->path; }

    bool operator==(File file) {
        return this->name.compare(file.get_name()) == EQUAL;
    }

    void debug_print() {
        printf("file: %s\n\tsize: %zu\n\tpath: %s\n", this->name.c_str(),
               this->size, this->path.c_str());
    }

    virtual ~File() {}
};

/* ----------------------------- FileSystem class --------------------------- */

class FileSystem {
  private:
    unordered_map<int, vector<File>> file_system;

  public:
    FileSystem() {}

    void check_create_uid(int uid) {
        if (this->file_system.count(uid) == 0) {
            printf("No uid, trying to insert new\n");
            this->file_system.insert(
                pair<int, vector<File>>(uid, vector<File>()));
            printf("did it!\n");
        }
    }

    void add_file(int uid, File file, string data) {
        try {
            try {
                if (this->get_files(uid).size() == MAX_FILES) {
                    throw MaxFilesReached();
                }
            } catch (const NoFiles) {
                // If there are no Files it's alright!
            }
            try {
                this->get_file(uid, file.get_name());
                throw FileAlreadyExists();
            } catch (const FileNotFound) {
                // If file doesn't exist, we can add it...
            }
            file_system.at(uid).push_back(file);
            ofstream new_file;
            new_file.open(file.get_path());
            new_file << data;
            new_file.close();

        } catch (const out_of_range) {
            throw UIDDoesNotExist();
        }
    }
    File get_file(int uid, string file_name) {
        try {
            const File dummy_file = File(file_name);
            auto vec_begin = this->file_system.at(uid)
                                 .begin(); // Begin and end of the vector
            auto vec_end = this->file_system.at(uid).end();

            auto file_itr = find(vec_begin, vec_end, dummy_file);
            if (file_itr == vec_end) {
                throw FileNotFound();
            }
            return this->file_system.at(uid).at(distance(vec_begin, file_itr));
        } catch (const out_of_range) {
            throw UIDDoesNotExist();
        }
    }
    string read_file(int uid, string file_name) {
        File file_info = this->get_file(uid, file_name);
        ifstream file;
        file.open(file_info.get_path());

        string file_data = string();
        string s;
        while (std::getline(file, s)) {
            file_data.append(s);
            if (file.good()) {
                file_data.append("\n");
            }
        }
        file.close();

        return file_data;
    }
    void delete_file(int uid, string file_name) {
        try {
            const File dummy_file = File(file_name);
            auto vec_begin = this->file_system.at(uid).begin();
            auto vec_end = this->file_system.at(uid).end();

            remove(this->get_file(uid, file_name).get_path().c_str());

            remove(vec_begin, vec_end, dummy_file);
        } catch (const out_of_range) {
            throw UIDDoesNotExist();
        }
    }
    vector<File> get_files(int uid) {
        try {
            if (this->file_system.at(uid).size() == 0) {
                throw NoFiles();
            }
            auto files = this->file_system.at(uid);
            return files;
        } catch (const out_of_range) {
            throw UIDDoesNotExist();
        }
    }
    void delete_personal_fs(int uid) {
        try {
            for (auto file : this->file_system.at(uid)) {
                remove(file.get_path().c_str());
            }
            this->file_system.erase(uid);
        } catch (const out_of_range) {
            throw UIDDoesNotExist();
        }
    }

    void erase() {
        for (auto user : this->file_system) {
            this->delete_personal_fs(user.first);
        }
    }

    void debug_print() {
        for (auto &kv : this->file_system) {
            printf("uid: %d\n", kv.first);
            for (auto &file : kv.second) {
                printf("\tname: %s\n\tsize: %zu\n\tpath: %s\n",
                       file.get_name().c_str(), file.get_size(),
                       file.get_path().c_str());
            }
        }
    }

    virtual ~FileSystem() {}
};

/* --------------------------- Operations Data class ------------------------ */

class OperationData {
  private:
    string op;
    string file_op;
    string file_name;
    string file_data;
    int file_size;
    int user_id;
    int trans_id;

  public:
    OperationData() {
        this->op = string();
        this->file_op = string();
        this->file_name = string();
        this->file_data = string();
        this->file_size = NONE;
        this->user_id = NONE;
        this->trans_id = NONE;
    }

    string get_op() { return this->op; }
    void set_op(string op) { this->op.assign(op); }
    string get_file_op() { return this->file_op; }
    void set_file_op(string file_op) { this->file_op.assign(file_op); }
    string get_file_name() { return this->file_name; }
    void set_file_name(string file_name) { this->file_name.assign(file_name); }
    string get_file_data() { return this->file_data; }
    void set_file_data(string file_data) { this->file_data.assign(file_data); }
    int get_file_size() { return this->file_size; }
    void set_file_size(int file_size) { this->file_size = file_size; }
    int get_user_id() { return this->user_id; }
    void set_user_id(int user_id) { this->user_id = user_id; }
    int get_trans_id() { return this->trans_id; }
    void set_trans_id(int trans_id) { this->trans_id = trans_id; }

    void clear() {
        this->op = string();
        this->file_op = string();
        this->file_name = string();
        this->file_data = string();
        this->file_size = NONE;
        this->user_id = NONE;
        this->trans_id = NONE;
    }

    virtual ~OperationData() {}
};

#endif
