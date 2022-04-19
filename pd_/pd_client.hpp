#ifndef PD_CLIENT
#define PD_CLIENT

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <queue>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

#define GROUP_NUM 28

#define ASPORT_DEFAULT 58000
#define PDPORT_DEFAULT 57000

#define UID_LEN 5
#define TID_LEN 4
#define RID_LEN 4
#define VC_LEN 4
#define PASS_LEN 8

#define MAX_BUF 1024
#define EQUAL 0
#define NONE -1
#define LOOP 1

using namespace std;

struct InvalidCommand : public exception {
  private:
    string error_msg;

  public:
    InvalidCommand(string error_msg_ = string("Command wasn't valid"))
        : error_msg(error_msg_) {}

    const char *what() const throw() { // Error message
        return error_msg.c_str();
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
    string pdip, asip, pass;
    int asport, pdport, uid;

  public:
    Data(int asport_ = ASPORT_DEFAULT + GROUP_NUM,
         int pdport_ = PDPORT_DEFAULT + GROUP_NUM,
         string asip_ = string("127.0.0.1"), int uid_ = NONE,
         string pass_ = string())
        : asip(asip_), pass(pass_), asport(asport_), pdport(pdport_),
          uid(uid_) {}

    int get_asport() { return this->asport; }
    void set_asport(int asport) { this->asport = asport; }
    int get_pdport() { return this->pdport; }
    void set_pdport(int pdport) { this->pdport = pdport; }
    string get_pdip() { return this->pdip; }
    void set_pdip(string pdip) { this->pdip.assign(pdip); }
    string get_asip() { return this->asip; }
    void set_asip(string asip) { this->asip.assign(asip); }

    string get_pass() { return this->pass; }
    void set_pass(string pass) { this->pass.assign(pass); }
    int get_uid() { return this->uid; }
    void set_uid(int uid) { this->uid = uid; }

    void debug_print() {
        printf(
            "PDIP: %s\nASIP: %s\nPDPORT: %d\nASPORT: %d\nUID: %d\nPASS: %s\n",
            this->pdip.c_str(), this->asip.c_str(), this->pdport, this->asport,
            this->uid, this->pass.c_str());
    }

    virtual ~Data() {}
};

#endif
