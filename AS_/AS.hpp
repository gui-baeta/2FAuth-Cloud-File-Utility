#ifndef AS_H
#define AS_H

#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <cerrno>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
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

enum WHICH_UDP_CLIENT { IS_PD_CLIENT, IS_PD_SERVER, IS_FS_CLIENT, ERROR };

#define GROUP_NUM 28
#define ASPORT_DEFAULT 58000
#define PDPORT_DEFAULT 57000
#define FSPORT_DEFAULT 59000

#define PORT_LEN 5
#define UID_LEN 5
#define TID_LEN 4
#define RID_LEN 4
#define VC_LEN 4
#define PASS_LEN 8
#define FOP_LEN 1

#define MAX_BUF 2000
#define EQUAL 0
#define NONE -1
#define LOOP 1
#define PATH_EXISTS 0

#define TID_ERROR 0

using namespace std;

/* ---------------------------- Exceptions ----------------------------- */

struct InvalidCommand : public exception { //
  private:
    string error_msg;

  public:
    InvalidCommand(string error_msg_ = string("Command wasn't valid"))
        : error_msg(error_msg_) {}

    const char *what() const throw() { // Error message
        return error_msg.c_str();
    }
};

struct RVCstatusNOK : public exception {
  public:
    const char *what() const throw() { return "Error on PD!"; }
};

struct MessageNotSent : public exception {
  public:
    const char *what() const throw() { return "Error on sending the message!"; }
};

struct UserAlreadyExists : public exception {
  public:
    const char *what() const throw() { return "User is already registered!"; }
};

struct UIDDoesNotExist : public exception {
  public:
    const char *what() const throw() { return "User ID doesn't exist!"; }
};

struct UserNotRegistered : public exception {
  public:
    const char *what() const throw() { return "User isn't registered yet!"; }
};

struct UserWrongPassword : public exception {
  public:
    const char *what() const throw() { return "User password doesn't match!"; }
};

struct RecvTimeOutFromPD : public exception {
  public:
    const char *what() const throw() {
        return "Receiving message from PD timed out!";
    }
};

struct InvalidOperationArg : public exception {
  public:
    const char *what() const throw() {
        return "Operation Argument is invalid!";
    }
};

/* ---------------------------- Semaphore class ----------------------------- */

class Semaphore {
  private:
    mutex mt_lock;
    condition_variable cv;
    int count;

  public:
    Semaphore(int count_ = 0) : count(count_) {}

    void set_count(int count) { this->count = count; }

    inline void release() {
        unique_lock<mutex> lock(this->mt_lock);
        this->count++;
        // release the mutex lock for the waiting thread
        this->cv.notify_one();
    }
    inline void acquire() {
        unique_lock<mutex> lock(this->mt_lock);
        while (this->count == 0) {
            // wait on the mutex until release is called
            this->cv.wait(lock);
        }
        count--;
    }
};

/* ------------------------------- Data class ------------------------------- */

class Data {
  private:
    string asip, fsip;
    int asport, fsport;

  public:
    Data(int asport_ = ASPORT_DEFAULT + GROUP_NUM, int fsport_ = NONE,
         string asip_ = string("127.0.0.1"), string fsip_ = string())
        : asip(asip_), fsip(fsip_), asport(asport_), fsport(fsport_) {}

    int get_asport() { return this->asport; }
    void set_asport(int asport) { this->asport = asport; }
    int get_fsport() { return this->fsport; }
    void set_fsport(int fsport) { this->fsport = fsport; }

    string get_asip() { return this->asip; }
    void set_asip(string asip) { this->asip.assign(asip); }
    string get_fsip() { return this->fsip; }
    void set_fsip(string fsip) { this->fsip.assign(fsip); }

    void debug_print() {
        printf("-----DATA DEBUG------\nASport: %d\nASip: %s\n"
               "fSport: %d\nFSip: %s\n--------------\n",
               this->asport, this->asip.c_str(), this->fsport,
               this->fsip.c_str());
    }

    virtual ~Data() {}
};

class User {

  private:
    string pdip, pass;
    int pdport, uid;
    bool isLoggedIn;

    int vc, tid;
    string file_op, file_name;
    string user_ip;
    int user_port;

  public:
    User(int pdport_ = PDPORT_DEFAULT + GROUP_NUM, string pdip_ = string(),
         int uid_ = NONE, string pass_ = string(), string fname_ = string(),
         string fop_ = string(), bool login = false, int vc_ = NONE,
         int tid_ = TID_ERROR, string file_op_ = string(),
         string file_name_ = string(), string user_ip_ = string(),
         int user_port_ = NONE)
        : pdip(pdip_), pass(pass_), pdport(pdport_), uid(uid_),
          isLoggedIn(login), vc(vc_), tid(tid_), file_op(file_op_),
          file_name(file_name_), user_ip(user_ip_), user_port(user_port_) {}

    int get_pdport() { return this->pdport; }
    void set_pdport(int pdport) { this->pdport = pdport; }
    string get_pdip() { return this->pdip; }
    void set_pdip(string pdip) { this->pdip.assign(pdip); }
    bool is_logged_in() { return this->isLoggedIn; }
    void set_logged_in(bool loggedIn) { this->isLoggedIn = loggedIn; }
    string get_pass() { return this->pass; }
    void set_pass(string pass) { this->pass.assign(pass); }
    int get_uid() { return this->uid; }
    void set_uid(int uid) { this->uid = uid; }
    int get_vc() { return this->vc; }
    void set_vc(int vc) { this->vc = vc; }
    int get_tid() { return this->tid; }
    void set_tid(int tid) { this->tid = tid; }
    string get_file_op() { return this->file_op; }
    void set_file_op(string file_op) { this->file_op.assign(file_op); }
    string get_file_name() { return this->file_name; }
    void set_file_name(string file_name) { this->file_name.assign(file_name); }
    string get_user_ip() { return this->user_ip; }
    void set_user_ip(string user_ip) { this->user_ip.assign(user_ip); }
    int get_user_port() { return this->user_port; }
    void set_user_port(int user_port) { this->user_port = user_port; }

    virtual ~User() {}
};

/* --------------------------- Operations Data class ------------------------ */

class OperationData {
  private:
    string op;
    string file_op;
    string file_name;
    string pass;
    string status;
    string pd_ip;
    int user_id;
    int req_id;
    int val_code;
    int trans_id;
    int pd_port;

  public:
    OperationData() {
        this->op = string();
        this->file_op = string();
        this->file_name = string();
        this->pass = string();
        this->status = string();
        this->pd_ip = string();
        this->user_id = NONE;
        this->req_id = NONE;
        this->val_code = NONE;
        this->trans_id = TID_ERROR;
        this->pd_port = NONE;
    }

    string get_op() { return this->op; }
    void set_op(string op) { this->op.assign(op); }
    string get_file_op() { return this->file_op; }
    void set_file_op(string file_op) { this->file_op.assign(file_op); }
    string get_file_name() { return this->file_name; }
    void set_file_name(string file_name) { this->file_name.assign(file_name); }
    string get_pass() { return this->pass; }
    void set_pass(string pass) { this->pass.assign(pass); }
    string get_status() { return this->status; }
    void set_status(string status) { this->status.assign(status); }
    string get_pd_ip() { return this->pd_ip; }
    void set_pd_ip(string pd_ip) { this->pd_ip.assign(pd_ip); }
    int get_user_id() { return this->user_id; }
    void set_user_id(int user_id) { this->user_id = user_id; }
    int get_req_id() { return this->req_id; }
    void set_req_id(int req_id) { this->req_id = req_id; }
    int get_val_code() { return this->val_code; }
    void set_val_code(int val_code) { this->val_code = val_code; }
    int get_trans_id() { return this->trans_id; }
    void set_trans_id(int trans_id) { this->trans_id = trans_id; }
    int get_pd_port() { return this->pd_port; }
    void set_pd_port(int pd_port) { this->pd_port = pd_port; }

    void clear() {
        this->op = string();
        this->file_op = string();
        this->file_name = string();
        this->pass = string();
        this->status = string();
        this->pd_ip = string();
        this->user_id = NONE;
        this->req_id = NONE;
        this->val_code = NONE;
        this->trans_id = TID_ERROR;
        this->pd_port = NONE;
    }

    virtual ~OperationData() {}
};

class AS_Data {
  private:
    unordered_map<int, User *> as_system;

  public:
    AS_Data() {}

    void add_user(int uid, User *user) {
        try {
            this->get_user(uid);
            throw UserAlreadyExists();
        } catch (const UIDDoesNotExist) {
            // If user doesn't exist, we can add it...
        }
        this->as_system.insert(pair<int, User *>(uid, user));
    }

    void login_user(int uid, string pass, pair<string, int> ip_port) {
        try {
            User *user = this->get_user(uid);
            if (user->get_pass().compare(pass) == EQUAL) {
                // Passwords match! User is ok to log in!!
                user->set_logged_in(true);
                user->set_user_ip(ip_port.first);
                user->set_user_port(ip_port.second);
            } else {
                throw UserWrongPassword();
            }
        } catch (UIDDoesNotExist) {
            throw UserNotRegistered();
        }
    }

    void search_log_out(string user_ip, int user_port) {
        for (auto &[uid, user] : this->as_system) {
            if (user->get_user_ip().compare(user_ip) == EQUAL &&
                user->get_user_port() == user_port) {
                user->set_logged_in(false);
            }
        }
    }

    string authenticate(int uid, int rid, int vc) {
        User *user = this->get_user(uid);
        try {

            if (user->is_logged_in() && vc == user->get_vc()) {
                int tid = this->generate_vc_tid();
                user->set_tid(tid);
                return to_string(tid);
            } else {
                user->set_tid(TID_ERROR);
                // Error has occured, return with TID ERROR (0)...
            }
        } catch (UIDDoesNotExist) {
            // Error has occured, return with TID ERROR (0)...
        }
        return to_string(TID_ERROR);
    }

    // Checks if file operation is valid (L, R, U, D or X)
    bool check_fop(string file_op) {
        return file_op.compare("L") == EQUAL || file_op.compare("R") == EQUAL ||
               file_op.compare("U") == EQUAL || file_op.compare("D") == EQUAL ||
               file_op.compare("X") == EQUAL;
    }

    string validate_file_op(OperationData *pd_op, OperationData user_op,
                            int vc) {
        try {
            User *user = this->get_user(user_op.get_user_id());
            if (user->is_logged_in()) {
                if (pd_op->get_status().compare("EPD") == EQUAL) {
                    return string("EPD");
                }
                if (!this->check_fop(user_op.get_file_op())) {
                    return string("EFOP");
                }
                user->set_file_op(user_op.get_file_op());
                user->set_file_name(user_op.get_file_name());
                user->set_vc(vc);
                return string("OK");
            } else {
                return string("ELOG");
            }
        } catch (UIDDoesNotExist) {
            return string("EUSER");
        }
    }

    pair<string, string> validate_fs_op(OperationData fs_op) {
        User *user = this->get_user(fs_op.get_user_id());
        string file_op = user->get_file_op();
        string file_name = user->get_file_name();
        if (fs_op.get_trans_id() == user->get_tid()) {
            if (file_op.compare("X") == EQUAL) {
                this->delete_user_data(user->get_uid());
            }
            return pair<string, string>(file_op, file_name);
        } else {
            return pair<string, string>(string("E"), file_name);
        }
    }

    void log_out_user(int uid) { this->get_user(uid)->set_logged_in(false); }

    void unregister_user(OperationData pd_client_op) {
        try {
            User *user = this->get_user(pd_client_op.get_user_id());
            if (pd_client_op.get_pass().compare(user->get_pass()) == EQUAL) {
                this->delete_user_data(user->get_uid());
            } else {
                throw UserWrongPassword();
            }
        } catch (UIDDoesNotExist) {
            throw UserNotRegistered();
        }
    }

    User *get_user(int uid) {
        try {
            return this->as_system.at(uid);
        } catch (const out_of_range) {
            throw UIDDoesNotExist();
        }
    }

    void delete_user_data(int uid) {
        try {
            this->as_system.erase(uid);
        } catch (const out_of_range) {
            throw UIDDoesNotExist();
        }
    }

    int generate_vc_tid() { return rand() % 9000 + 1000; }

    void erase_data() { this->as_system.clear(); }

    virtual ~AS_Data() {}
};

#endif
