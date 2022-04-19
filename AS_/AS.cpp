/*
 * Group: 28
 *     92433 - Carolina Pereira
 *     92442 - Daniela Castanho
 *     92470 - Guilherme Fontes
 */

#include "AS.hpp"

/* --------------------------- Global variables ----------------------------- */

Semaphore wait_last_user;
Semaphore smph_ok_to_rrq_reply;

bool verbose = false;
bool ok_to_exit = false;
bool time_to_panic = false;
bool waiting_for_pd = false;

vector<int> client_sockets;
int *udp_socket_ref;
int *tcp_server_socket_ref;
AS_Data *as_data_ptr;

/* ----------------------------- Miscellaneous ------------------------------ */

// Formats a vector of strings onto a string (separates each string inside the
// vector by a space)
string format_to_str(vector<string> strings) {
    string fmt_str;
    for (auto str : strings) {
        fmt_str.append(str);
        fmt_str.push_back(' ');
    }
    fmt_str.erase(fmt_str.end() - 1); // Delete trailing space char
    return fmt_str;
}

// Trim string (remove trailing newline)
string trim(const string &base) {
    string str = base;
    size_t found;
    found = str.find_last_not_of(string("\n"));
    if (found != string::npos) {
        str.erase(found + 1);
    } else {
        return base;
    }

    return str;
}

// Get first word before given token
string filter_1st_with(const string &str, char token) {
    size_t start;
    size_t end = 0;
    if ((start = str.find_first_not_of(token, end)) != string::npos) {
        end = str.find(token, start);
        return str.substr(start, end - start);
    }

    return NULL;
}

// Splitting function for strings
vector<string> split_with(const string &str, char token) {
    vector<string> strs_vec;
    size_t start;
    size_t end = 0;
    while ((start = str.find_first_not_of(token, end)) != string::npos) {
        end = str.find(token, start);
        strs_vec.push_back(str.substr(start, end - start));
    }
    return strs_vec;
}

// Introduces a panic message when something went wrong
void panic(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    while (*fmt != '\0') {
        if (*fmt == 'd') {
            int i = va_arg(args, int);
            std::cout << i;
        } else if (*fmt == 'c') {
            // note automatic conversion to integral type
            int c = va_arg(args, int);
            std::cout << static_cast<char>(c);
        } else if (*fmt == 'f') {
            double d = va_arg(args, double);
            std::cout << d;
        } else if (*fmt == 's') {
            string s = string(va_arg(args, char *));
            cout << s;
        }
        ++fmt;
    }
    va_end(args);
    exit(EXIT_FAILURE);
}

void print_verbose(const char *fmt...) {
    if (verbose) {
        va_list args;
        va_start(args, fmt);

        while (*fmt != '\0') {
            if (*fmt == 'd') {
                int i = va_arg(args, int);
                cout << i;
            } else if (*fmt == 'c') {
                // note automatic conversion to integral type
                int c = va_arg(args, int);
                cout << static_cast<char>(c);
            } else if (*fmt == 'f') {
                double d = va_arg(args, double);
                cout << d;
            } else if (*fmt == 's') {
                string s = va_arg(args, char *);
                cout << s;
            }
            ++fmt;
        }
        va_end(args);
        printf("\n");
    }
}

pair<string, int> get_ip_port(int sockfd) {
    char user_ip[16];
    unsigned int user_port;

    bzero(user_ip, sizeof(char) * 16);
    struct sockaddr_in client_addr;
    bzero(&client_addr, sizeof(client_addr));
    socklen_t len = sizeof(client_addr);
    getsockname(sockfd, (struct sockaddr *)&client_addr, &len);
    inet_ntop(AF_INET, &client_addr.sin_addr, user_ip, sizeof(user_ip));
    user_port = ntohs(client_addr.sin_port);

    return pair<string, int>(string(user_ip), user_port);
}

void sig_handler(int signal) {
    switch (signal) {
    case SIGINT:
        ok_to_exit = true;
        for (size_t i = 0; i < 101; i++) {
            wait_last_user.release();
        }
        smph_ok_to_rrq_reply.release();

        // Erase File System, shutdown client sockets and "FS <-> AS" UDP socket
        shutdown(*udp_socket_ref, SHUT_RDWR);
        shutdown(*tcp_server_socket_ref, SHUT_RDWR);
        for (auto socket : client_sockets) {
            shutdown(socket, SHUT_RDWR);
        }
        client_sockets.clear();

        delete as_data_ptr;
        exit(EXIT_SUCCESS);
        break;

    case SIGPIPE:
        struct sigaction act;
        memset(&act, 0, sizeof(act));
        act.sa_handler = SIG_IGN;

        if (sigaction(SIGPIPE, &act, NULL) == -1) {
            panic("sigaction failed with error: "
                  "%d\n%s",
                  errno, strerror(errno));
        }
        break;
    }
    getchar();
}

void parse_args(int argc, char *argv[], Data *data) {
    if (argc > 4 || argc < 1) {
        puts("Correct usage is: ./AS [-p ASport] [-v]");
        exit(EXIT_FAILURE);
    } else if (argc <= 4) { // Check for optional arguments and use them
        if (argc == 2) {    //./AS -v
            if (strcmp("-v", argv[1]) == EQUAL) {
                // Check for verbose flag
                verbose = true;
                data->set_asport(ASPORT_DEFAULT + GROUP_NUM);
                print_verbose("s", "Verbose mode is activated");
            } else {
                puts("Invalid arguments were given");
                exit(EXIT_FAILURE);
            }
        } else if (argc == 3) {
            if (strcmp("-p", argv[1]) == EQUAL) {
                // Check for port flag
                data->set_asport(atoi(argv[2]));
            } else {
                puts("Invalid arguments were given");
                exit(EXIT_FAILURE);
            }
        } else if (argc == 4) {
            if (strcmp("-p", argv[1]) == EQUAL &&
                strcmp("-v", argv[3]) == EQUAL) {
                // Check for port and verbose flags
                data->set_asport(atoi(argv[2]));
                verbose = true;
                print_verbose("s", "Verbose mode is activated");
            } else {
                puts("Invalid arguments were given");
                exit(EXIT_FAILURE);
            }
        } else if (argc == 1) {
            data->set_asport(ASPORT_DEFAULT + GROUP_NUM);
        } else {
            puts("Invalid arguments were given");
            exit(EXIT_FAILURE);
        }
    }
}

// Personalized extension of stoi functionality
int stoi_(string str_to_check, string error_msg, int len_check = NONE) {
    size_t idx;
    int num = 0;
    try { // If stoi can't transfrom the string into an int (stoi throws some
          // exceptions)...
        num = stoi(str_to_check, &idx);
    } catch (const invalid_argument) { // ...throw an invalid command exception
                                       // so it can be handled better elsewhere
        throw InvalidCommand(error_msg);
    } catch (const out_of_range) {
        throw InvalidCommand(error_msg);
    }
    // vvv Check if stoi checked the whole string or not (if it did -> string is
    // a number. If not -> string is not a number)
    if (idx != str_to_check.length() ||
        ((len_check == NONE) ? false : (str_to_check.length() != len_check))) {
        throw InvalidCommand(error_msg); // ^^^ Also, check if the given string
                                         // has a valid length, according to the
                                         // specific message protocol
    }
    return num;
}

int generate_vc_tid() { return rand() % 9000 + 1000; }

void set_signals() {
    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        panic("Failed installing a signal handler for the main thread "
              "with error: %d\n %s\n",
              errno, strerror(errno));
    }
    if (signal(SIGABRT, sig_handler) == SIG_ERR) {
        panic("Failed installing a signal handler for the main thread "
              "with error: %d\n %s\n",
              errno, strerror(errno));
    }
    if (signal(SIGPIPE, sig_handler) == SIG_ERR) {
        panic("Failed installing a signal handler for the main thread "
              "with error: %d\n %s\n",
              errno, strerror(errno));
    }
}

void mask_signals() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGPIPE);
    sigaddset(&mask, SIGABRT);
    sigaddset(&mask, SIGINT);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);
}

/* ------------------------ Communication functions ------------------------- */

void send_msg_tcp(int *socket, string msg) {
    ssize_t chars_written =
        write(*socket, msg.c_str(),
              msg.length()); // Write as many chars as possible
    if (chars_written < 0) {
        print_verbose("sdss",
                      "Failed at writing TCP message with error: ", errno, "\n",
                      strerror(errno));
    }
}

string recv_tcp_msg(int *socket) {
    string new_msg;

    char c[2] = {0};
    do {
        ssize_t chars_read = read(*socket, c, 1);

        if (chars_read == 0) {
            return string();
        }
        if (chars_read < 0 && (errno == ECONNRESET || errno == EREMOTEIO)) {
            return string();
        }
        if (c[0] == '\n') {
            break;
        } else {
            new_msg.push_back(c[0]);
        }
        if (chars_read < 0) {
            if (ok_to_exit) {
                return string();
            }
            print_verbose("sdss",
                          "Failed at reading TCP message with error: ", errno,
                          "\n", strerror(errno));

            exit(EXIT_FAILURE);
        }
    } while (LOOP);

    if (verbose) {
        auto ip_port = get_ip_port(*socket);
        printf("Receiving message from user.\n\tIP: %s\n\tPort: %u\n",
               ip_port.first.c_str(), ip_port.second);
    }

    return new_msg;
}

string recv_msg_udp(int *socket, sockaddr_in *addr, socklen_t *len,
                    string panic_fmt_msg) {
    char buffer[MAX_BUF];
    bzero(buffer, sizeof(char) * MAX_BUF);
    ssize_t chars_read;
    do {

        chars_read =
            recvfrom(*socket, buffer, MAX_BUF, 0, (struct sockaddr *)addr, len);
        if (chars_read > 0) {
            break;
        }
        if (errno == EWOULDBLOCK || errno == ETIMEDOUT) {
            if (waiting_for_pd) {
                throw RecvTimeOutFromPD();
            } else {
                continue;
            }
        } else if (chars_read < 0) {
            if (ok_to_exit) {
                return string();
            }
            panic(panic_fmt_msg.c_str(), errno, strerror(errno));
        }
    } while (LOOP);
    if (verbose) {
        char ip[16];
        unsigned int port;

        bzero(ip, sizeof(char) * 16);
        inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));
        port = ntohs(addr->sin_port);

        printf("Receiving UDP message from:\n\tIP: %s\n\tPort: %u\n", ip, port);
    }

    return string(buffer);
}

void send_msg_udp(int *socket, sockaddr_in *addr, socklen_t *len, string msg,
                  string panic_fmt_msg) {
    ssize_t chars_written;
    ssize_t num_chars = msg.length();

    const char *buffer = msg.c_str();

    while (num_chars > 0) {
        chars_written =
            sendto(*socket, buffer, num_chars, 0, (struct sockaddr *)addr,
                   *len); // Write as many chars as possible
        if (chars_written < 0) {
            panic(panic_fmt_msg.c_str(), errno, strerror(errno));
        }
        num_chars -= chars_written; // Change the number of chars left to write
        buffer += chars_written; // Scrub the buffer pointer to the new position
                                 // to continue sending the message
    }
}

/* ----------------------------- AS functions ------------------------------- */

WHICH_UDP_CLIENT which_client(string op) {
    if (op.compare("REG") == EQUAL || op.compare("UNR") == EQUAL) {
        return IS_PD_CLIENT;
    } else if (op.compare("VLD") == EQUAL) {
        return IS_FS_CLIENT;
    } else if (op.compare("RVC") == EQUAL) {
        return IS_PD_SERVER;
    } else {
        return ERROR;
    }
}

string get_user_reply_op(string op) {
    if (op.compare("REQ") == EQUAL) {
        return string("RRQ");
    } else if (op.compare("LOG") == EQUAL) {
        return string("RLO");
    } else if (op.compare("AUT") == EQUAL) {
        return string("RAU");
    } else {
        throw InvalidOperationArg();
    }
}

string parse_args_user(OperationData *ops_data, vector<string> *args) {
    try {
        args->at(0);
    } catch (const out_of_range) {
        return string("ERR\n");
    }

    try {
        switch (args->size()) {
        case 5: // REQ
            ops_data->set_op(args->at(0));
            ops_data->set_user_id(stoi_(
                args->at(1), "User ID must be a 5-digit number!", UID_LEN));
            ops_data->set_req_id(stoi_(
                args->at(2), "Request ID must be a 4-digit number!", RID_LEN));
            if (args->at(3).length() != FOP_LEN) {
                throw InvalidCommand("File Operation must be 1 character!");
            }
            ops_data->set_file_op(args->at(3));
            ops_data->set_file_name(args->at(4));
            break;
        case 4: // REQ, AUT
            if (args->at(0).compare("REQ") == EQUAL) {
                ops_data->set_op(args->at(0));
                ops_data->set_user_id(stoi_(
                    args->at(1), "User ID must be a 5-digit number!", UID_LEN));
                ops_data->set_req_id(
                    stoi_(args->at(2), "Request ID must be a 4-digit number!",
                          RID_LEN));
                if (args->at(3).length() != FOP_LEN) {
                    throw InvalidCommand("File Operation must be 1 character!");
                }
                ops_data->set_file_op(args->at(3));
            } else if (args->at(0).compare("AUT") == EQUAL) {
                ops_data->set_op(args->at(0));
                ops_data->set_user_id(stoi_(
                    args->at(1), "User ID must be a 5-digit number!", UID_LEN));
                ops_data->set_req_id(
                    stoi_(args->at(2), "Request ID must be a 4-digit number!",
                          RID_LEN));
                ops_data->set_val_code(
                    stoi_(args->at(3),
                          "Validation Code must be a 4-digit number!", VC_LEN));
            } else {
                throw InvalidCommand("Invalid Operation!");
            }
            break;
        case 3: // LOG
            ops_data->set_op(args->at(0));
            ops_data->set_user_id(stoi_(
                args->at(1), "User ID must be a 5-digit number!", UID_LEN));
            ops_data->set_pass(args->at(2));
            break;
        default:
            string reply = string(args->at(0));
            reply.append(" ERR\n");
            return reply;
        }

    } catch (const out_of_range) {
        string reply = string(get_user_reply_op(args->at(0)));
        reply.append(" ERR\n");
        return reply;
    } catch (const InvalidCommand) {
        string reply = string(get_user_reply_op(args->at(0)));
        reply.append(" ERR\n");
        return string(reply);
    }
    return string();
}

string parse_args_pd(OperationData *ops_data, AS_Data *as_data,
                     vector<string> *args) {
    try {
        args->at(0);
    } catch (const out_of_range) {
        return string("ERR\n");
    }

    try {
        int uid =
            stoi_(args->at(1), "User ID must be a 5-digit number!", UID_LEN);
        switch (args->size()) {
        case 5: // REG
            ops_data->set_pd_ip(args->at(3));
            ops_data->set_pd_port(stoi_(
                args->at(4), "PD Port must be a 5-digit number", PORT_LEN));
        case 3: // UNR
            ops_data->set_op(args->at(0));
            ops_data->set_user_id(uid);
            ops_data->set_pass(args->at(2));

            break;
        default:
            string reply = string(args->at(0));
            reply.append(" ERR\n");
            return reply;
        }

    } catch (const out_of_range) {
        string reply = string(get_user_reply_op(args->at(0)));
        reply.append(" ERR\n");
        return reply;
    } catch (const InvalidCommand) {
        string reply = string(get_user_reply_op(args->at(0)));
        reply.append(" ERR\n");
        return string(reply);
    }
    return string();
}

void handle_pd_server_query(int *as_udp_socket, OperationData *pd_op,
                            Data *data, vector<string> args) {

    pd_op->clear();
    pd_op->set_op(args.at(0));
    pd_op->set_user_id(
        stoi_(args.at(1), "User ID must be a 5-digit number!", UID_LEN));
    pd_op->set_status(args.at(2));

    waiting_for_pd = false;
    smph_ok_to_rrq_reply.release();
}

string execute_user_query(int *as_udp_socket, int user_socket, AS_Data *as_data,
                          OperationData user_op, OperationData *pd_op) {
    string reply_op = get_user_reply_op(user_op.get_op());

    string user_reply = string();

    user_reply.assign(reply_op);
    user_reply.append(" ");
    if (user_op.get_op().compare("LOG") == EQUAL) {
        try {
            auto user_ip_port = get_ip_port(user_socket);
            as_data->login_user(user_op.get_user_id(), user_op.get_pass(),
                                user_ip_port);

            user_reply.append("OK\n");
        } catch (UserNotRegistered) {
            user_reply.assign(reply_op);
            user_reply.append(" ERR\n");
        } catch (UserWrongPassword) {
            user_reply.assign(reply_op);
            user_reply.append(" NOK\n");
        }
    } else if (user_op.get_op().compare("REQ") == EQUAL) {
        int val_code = generate_vc_tid();
        int uid = user_op.get_user_id();

        string pd_query = string("VLC ");

        pd_query.append(to_string(uid));
        pd_query.append(" ");
        pd_query.append(to_string(val_code));
        pd_query.append(" ");
        pd_query.append(user_op.get_file_op());
        if (!user_op.get_file_name().empty()) {
            pd_query.append(" ");
            pd_query.append(user_op.get_file_name());
        }
        pd_query.append("\n");

        struct sockaddr_in pd_client_addr;
        bzero(&pd_client_addr, sizeof(pd_client_addr));
        pd_client_addr.sin_family = AF_INET; // IPv4
        pd_client_addr.sin_port = htons(as_data->get_user(uid)->get_pdport());
        pd_client_addr.sin_addr.s_addr =
            inet_addr(as_data->get_user(uid)->get_pdip().c_str());

        socklen_t len = sizeof(pd_client_addr);
        int socketfd = *as_udp_socket;
        send_msg_udp(&socketfd, &pd_client_addr, &len, pd_query,
                     "Failed at writing to PD UDP client!");

        if (ok_to_exit) {
            return string();
        }

        waiting_for_pd = true;
        smph_ok_to_rrq_reply.acquire();

        user_reply.append(as_data->validate_file_op(pd_op, user_op, val_code));
        user_reply.append("\n");

    } else if (user_op.get_op().compare("AUT") == EQUAL) {
        user_reply.append(as_data->authenticate(user_op.get_user_id(),
                                                user_op.get_req_id(),
                                                user_op.get_val_code()));
        user_reply.append("\n");
    }

    return user_reply;
}

void handle_user(int *as_udp_socket, AS_Data *as_data, OperationData *pd_op,
                 int user_socket) {

    mask_signals();
    do {
        if (ok_to_exit) {
            break;
        }
        wait_last_user.acquire();
        if (ok_to_exit) {
            break;
        }
        string user_msg = recv_tcp_msg(&user_socket);
        if (ok_to_exit) {
            break;
        }
        if (user_msg.empty()) {
            auto user_ip_port = get_ip_port(*as_udp_socket);
            as_data->search_log_out(user_ip_port.first, user_ip_port.second);
            wait_last_user.release();
            remove(client_sockets.begin(), client_sockets.end(), user_socket);
            close(user_socket);
            break;
        }
        auto args = split_with(user_msg, ' ');

        OperationData op_data;
        parse_args_user(&op_data, &args);
        if (ok_to_exit) {
            break;
        }
        string reply_msg = execute_user_query(as_udp_socket, user_socket,
                                              as_data, op_data, pd_op);
        if (ok_to_exit) {
            break;
        }

        send_msg_tcp(&user_socket, reply_msg);
        if (ok_to_exit) {
            break;
        }

        wait_last_user.release();
    } while (LOOP);
}

void execute_pd_query(int *as_udp_socket, struct sockaddr_in cli_udp_addr,
                      socklen_t len, vector<string> args, Data *data,
                      AS_Data *as_data) {
    OperationData pd_client_op;
    parse_args_pd(&pd_client_op, as_data, &args);

    string reply_msg;
    if (args.at(0).compare("REG") == EQUAL) {
        int uid = pd_client_op.get_user_id();
        User *new_user =
            new User(pd_client_op.get_pd_port(), pd_client_op.get_pd_ip(), uid,
                     pd_client_op.get_pass());
        try {
            as_data->add_user(uid, new_user);
            reply_msg.assign("RRG OK\n");
        } catch (UserAlreadyExists) {
            reply_msg.assign("RRG NOK\n");
        }
    } else if (args.at(0).compare("UNR") == EQUAL) {
        try {
            as_data->unregister_user(pd_client_op);
            reply_msg.assign("RUN OK\n");
        } catch (UserNotRegistered) {
            reply_msg.assign("RUN NOK\n");
        } catch (UserWrongPassword) {
            reply_msg.assign("RUN NOK\n");
        }
    } else {
        panic("Something really wrong happened! Invalid operation received "
              "from PD client!");
    }

    if (ok_to_exit) {
        return;
    }
    int sockfd = *as_udp_socket;
    send_msg_udp(&sockfd, &cli_udp_addr, &len, reply_msg,
                 "Failed at sending message to UDP PD client");
}

void execute_fs_query(int as_udp_socket, vector<string> args, Data *data,
                      AS_Data *as_data, struct sockaddr_in fs_cli_addr) {
    string reply_msg;
    OperationData fs_op;
    try {
        fs_op.set_op(args.at(0));
        fs_op.set_user_id(
            stoi_(args.at(1), "User ID must be a 5-digit number!", UID_LEN));
        fs_op.set_trans_id(stoi_(
            args.at(2), "Transaction ID must be a 4-digit number!", TID_LEN));
        auto file_op_name = as_data->validate_fs_op(fs_op);
        reply_msg.assign("CNF ");
        reply_msg.append(to_string(fs_op.get_user_id()));
        reply_msg.append(" ");
        reply_msg.append(to_string(fs_op.get_trans_id()));
        reply_msg.append(" ");
        reply_msg.append(file_op_name.first);
        if (file_op_name.second.empty()) {
            reply_msg.append(" ");
            reply_msg.append(file_op_name.second);
        }
        reply_msg.append("\n");
    } catch (out_of_range) {
        reply_msg.assign("ERR\n");
    } catch (InvalidCommand) {
        reply_msg.assign("ERR\n");
    }
    if (ok_to_exit) {
        return;
    }

    socklen_t len = sizeof(fs_cli_addr);
    send_msg_udp(&as_udp_socket, &fs_cli_addr, &len, reply_msg,
                 "Failed at sending reply message to the FS UDP client");
}

void await_pds_fs(int *as_udp_socket, OperationData *pd_op, Data *data,
                  AS_Data *as_data) {
    struct sockaddr_in serv_udp_addr;
    struct sockaddr_in cli_udp_addr;
    bzero(&serv_udp_addr, sizeof(serv_udp_addr));
    bzero(&cli_udp_addr, sizeof(cli_udp_addr));
    serv_udp_addr.sin_family = AF_INET; // IPv4
    serv_udp_addr.sin_port = htons(data->get_asport());
    serv_udp_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    mask_signals();

    if ((*as_udp_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        time_to_panic = true;
        panic("AS UDP server socket creation failed with error: %d\n%s", errno,
              strerror(errno));
    }

    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    if (setsockopt(*as_udp_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                   sizeof(timeout)) < 0) {
        time_to_panic = true;
        panic("Failed at setting AS server socket options with error: %d\n%s",
              errno, strerror(errno));
    }
    int opt = 1;
    if (setsockopt(*as_udp_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt)) < 0) {
        time_to_panic = true;
        panic("Failed at setting AS server socket options with error: %d\n%s",
              errno, strerror(errno));
    }

    if (bind(*as_udp_socket, (const struct sockaddr *)&serv_udp_addr,
             sizeof(serv_udp_addr)) < 0) {
        time_to_panic = true;
        panic("AS UDP server binding failed with error: %d\n%s", errno,
              strerror(errno));
    }

    print_verbose("sd", "Created UDP AS socket with fd: ", *as_udp_socket);

    socklen_t len = sizeof(cli_udp_addr);
    do {
        if (ok_to_exit) {
            break;
        }
        string msg;
        try {
            msg = recv_msg_udp(as_udp_socket, &cli_udp_addr, &len,
                               "Failed at receiving message at AS UDP server");
        } catch (RecvTimeOutFromPD) {
            print_verbose("s", "PD client timed out.");
            pd_op->clear();
            pd_op->set_status("EPD");
            smph_ok_to_rrq_reply.release();
            waiting_for_pd = false;
            continue;
        }
        if (ok_to_exit) {
            break;
        }

        auto args = split_with(trim(msg), ' ');

        switch (which_client(args.at(0))) {
        case IS_PD_CLIENT:
            execute_pd_query(as_udp_socket, cli_udp_addr, len, args, data,
                             as_data);
            break;
        case IS_FS_CLIENT:
            execute_fs_query(*as_udp_socket, args, data, as_data, cli_udp_addr);
            break;
        case IS_PD_SERVER:
            handle_pd_server_query(as_udp_socket, pd_op, data, args);
            break;
        default:
            panic(
                "Something really wrong happened!\nCouldn't detect which type "
                "of UDP client was (FS or PD)");
            break;
        }

    } while (LOOP);
}

void await_users(int *as_tcp_socket, int *as_udp_socket, AS_Data *as_data,
                 OperationData *pd_op, Data *data) {
    struct sockaddr_in tcp_serv_addr;
    bzero(&tcp_serv_addr, sizeof(tcp_serv_addr));
    tcp_serv_addr.sin_family = AF_INET; // IPv4
    tcp_serv_addr.sin_port = htons(data->get_asport());
    tcp_serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    mask_signals();

    if ((*as_tcp_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        time_to_panic = true;
        panic("AS TCP server socket creation failed with error: %d\n%s", errno,
              strerror(errno));
    }
    int opt = 1;
    if (setsockopt(*as_tcp_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt)) < 0) {
        time_to_panic = true;
        panic("Failed at setting AS server socket options with error: %d\n%s",
              errno, strerror(errno));
    }

    if (bind(*as_tcp_socket, (const struct sockaddr *)&tcp_serv_addr,
             sizeof(tcp_serv_addr)) < 0) {
        time_to_panic = true;
        panic("AS TCP server binding failed with error: %d\n%s", errno,
              strerror(errno));
    }

    print_verbose("sd", "Created TCP AS socket with fd: ", *as_tcp_socket);

    if (listen(*as_tcp_socket, 101) < 0) {
        time_to_panic = true;
        panic("AS TCP server listen failed with error: %d\n%s", errno,
              strerror(errno));
    }

    socklen_t len = sizeof(tcp_serv_addr);
    do {
        int new_user_socket;
        if ((new_user_socket =
                 accept(*as_tcp_socket, (struct sockaddr *)&tcp_serv_addr,
                        (socklen_t *)&len)) < 0) {
            panic("Failed at accepting AS server client (an user) with "
                  "error: "
                  "%d\n%s",
                  errno, strerror(errno));
        }
        client_sockets.push_back(new_user_socket);
        print_verbose("sd",
                      "Accepted TCP User socket with fd: ", new_user_socket);
        thread new_client(handle_user, as_udp_socket, as_data, pd_op,
                          new_user_socket);
        new_client.detach();
    } while (LOOP);
}

/* --------------------------------- Main ----------------------------------- */

int main(int argc, char *argv[]) {
    Data data = Data();
    AS_Data *as_data = new AS_Data();

    as_data_ptr = as_data;

    parse_args(argc, argv, &data);

    int as_tcp_socket = 0, as_udp_socket = 0;

    tcp_server_socket_ref = &as_tcp_socket;
    udp_socket_ref = &as_udp_socket;

    wait_last_user.set_count(1);

    OperationData pd_op;

    set_signals();

    thread t1(await_users, &as_tcp_socket, &as_udp_socket, as_data, &pd_op,
              &data);
    thread t2(await_pds_fs, &as_udp_socket, &pd_op, &data, as_data);

    do {
        if (time_to_panic) {
            t1.detach();
            t2.detach();
            exit(EXIT_FAILURE);
        }
        if (ok_to_exit) {
            t1.join();
            t2.join();
            break;
        }
        this_thread::sleep_for(chrono::milliseconds(500));
    } while (LOOP);

    return 0;
}
