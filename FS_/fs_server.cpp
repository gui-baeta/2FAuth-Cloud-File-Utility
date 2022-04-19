/*
 * Group: 28
 *     92433 - Carolina Pereira
 *     92442 - Daniela Castanho
 *     92470 - Guilherme Fontes
 */

#include "fs_server.hpp"

/* --------------------------- Global variables ----------------------------- */

Semaphore smph_ch;         // Message channel semaphore lock
Semaphore as_replied_smph; // Wait for AS reply to proceed with the User request
                           // semaphore lock
Semaphore wait_other_client;
bool ok_to_exit = false;
bool time_to_panic = false;

vector<int> client_sockets;
int as_socket_ref;
int server_fd_ref;
FileSystem *fs_ptr;

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

void print_verbose(bool verbose, const char *fmt...) {
    if (verbose) {
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
                string s = va_arg(args, char *);
                cout << s;
            }
            ++fmt;
        }
        va_end(args);
        printf("\n");
    }
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
            string s = va_arg(args, char *);
            cout << s;
        }
        ++fmt;
    }
    va_end(args);
    exit(EXIT_FAILURE);
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
        ((len_check == NONE) || (len_check == FSIZE_LEN)
             ? false
             : (str_to_check.length() != len_check))) {
        throw InvalidCommand(error_msg); // ^^^ Also, check if the given string
                                         // has a valid length, according to the
                                         // specific message protocol
    }
    return num;
}

// Receives a file operation and translates it into the reply operation (e.g.:
// LST -> RLS)
string get_reply_op(string file_op) {
    string reply_op;
    if (file_op.compare("LST") == EQUAL) {
        reply_op.assign("RLS");
    } else if (file_op.compare("RTV") == EQUAL) {
        reply_op.assign("RRT");
    } else if (file_op.compare("UPL") == EQUAL) {
        reply_op.assign("RUP");
    } else if (file_op.compare("DEL") == EQUAL) {
        reply_op.assign("RDL");
    } else if (file_op.compare("REM") == EQUAL) {
        reply_op.assign("RRM");
    } else {
        throw InvalidOperationArg();
    }

    return reply_op;
}

/* -------------------------- File System related --------------------------- */

void parse_args(int argc, char *argv[], Data *data) {
    if (argc > 8) {
        panic("Correct usage is: ./FS [-q FSport] [-n ASIP] [-p ASport] [-v]");
    } else { // Check for optional arguments and use them
        try {
            for (size_t i = 1; i < argc; i++) {
                if (strcmp("-q", argv[i]) == EQUAL) {
                    data->set_fsport(stoi_(
                        argv[++i],
                        "File System Server port is not a valid number!"));
                } else if (strcmp("-n", argv[i]) == EQUAL) {
                    data->set_asip(argv[++i]);
                } else if (strcmp("-p", argv[i]) ==
                           EQUAL) { // Check for FS port argument
                    data->set_asport(stoi_(argv[++i],
                                           "Authentication Server port is "
                                           "not a valid number!"));
                } else if (strcmp("-v", argv[i]) ==
                           EQUAL) { // Check for verbose mode toggle
                                    // argument
                    data->set_vrbs_mode(true);
                } else {
                    panic("s", "Correct usage is: ./FS [-q FSport] [-n ASIP] "
                               "[-p ASport] [-v]");
                }
            }
        } catch (const InvalidCommand &ic) {
            panic(ic.what());
        }
    }
}

void parse_args_as(OperationData *op_data, vector<string> *args) {
    try {
        switch (args->size()) {
        case 5:
            op_data->set_file_name(args->at(4));
        case 4:
            if (args->at(0).compare("CNF") != EQUAL) {
                throw InvalidCommand("Received command from AS is not CNF!");
            }
            op_data->set_op(args->at(0));
            op_data->set_user_id(stoi_(
                args->at(1), "Received command from AS is invalid!", UID_LEN));
            op_data->set_trans_id(stoi_(
                args->at(2), "Received command from AS is invalid!", TID_LEN));
            op_data->set_op(args->at(3));
            break;
        default:
            throw InvalidCommand("Received command from AS is invalid!");
        }
    } catch (const out_of_range) {
        throw InvalidCommand("Received command from AS is invalid!");
    }
}

string parse_args_client(OperationData *ops_data, vector<string> *args) {
    try {
        args->at(0);
    } catch (const out_of_range) {
        return string("ERR\n");
    }

    try {
        switch (args->size()) {
        case 5: // UPL
            ops_data->set_file_size(
                stoi_(args->at(4),
                      "File size received on Upload operation is not a number!",
                      FSIZE_LEN));
        case 4: // RTV, DEL
            ops_data->set_file_name(args->at(3));
        case 3: // LST, REM
            ops_data->set_op(args->at(0));
            ops_data->set_user_id(stoi_(
                args->at(1), "User ID must be a 5-digit number!", UID_LEN));
            ops_data->set_trans_id(
                stoi_(args->at(2), "Transaction ID must be a 4-digit number!",
                      TID_LEN));
            break;
        default:
            string reply = string(args->at(0));
            reply.append(" ERR\n");
            return reply;
        }

    } catch (const out_of_range) {
        try {
            string reply = string(get_reply_op(args->at(0)));
            reply.append(" ERR\n");
            return reply;
        } catch (const InvalidOperationArg) {
            return string("ERR\n");
        }
    } catch (const InvalidCommand) {
        string reply = string(get_reply_op(args->at(0)));
        reply.append(" ERR\n");
        return string(reply);
    }
    return string();
}

void sig_handler(int signal) {
    switch (signal) {
    case SIGINT:
        ok_to_exit = true;
        for (size_t i = 0; i < 101; i++) {
            wait_other_client.release();
        }
        smph_ch.release();
        as_replied_smph.release();

        // Erase File System, shutdown client sockets and "FS <-> AS" UDP socket
        fs_ptr->erase();
        shutdown(as_socket_ref, SHUT_RDWR);
        shutdown(server_fd_ref, SHUT_RDWR);
        for (auto socket : client_sockets) {
            shutdown(socket, SHUT_RDWR);
        }
        client_sockets.clear();

        delete fs_ptr;
        exit(EXIT_SUCCESS);
        break;

    case SIGPIPE:
        /* Ignore */
        break;
    }
    getchar();
}

string get_client_query_ans(FileSystem *fs, OperationData op_data,
                            OperationData *as_op_data) {

    string reply_op = get_reply_op(op_data.get_op());

    string query_ans = string(reply_op);
    query_ans.append(" ");

    if (strcmp(as_op_data->get_op().c_str(), "E") == EQUAL) {
        query_ans.append("INV");
        return query_ans;
    }

    fs->check_create_uid(op_data.get_user_id());

    // Reply to the List command
    if (reply_op.compare("RLS") == EQUAL) {
        try {
            query_ans.append(
                to_string(fs->get_files(op_data.get_user_id()).size()));
            for (auto &files : fs->get_files(op_data.get_user_id())) {
                query_ans.append(" ");
                query_ans.append(files.get_name());
                query_ans.append(" ");
                query_ans.append(to_string(files.get_size()));
            }
        } catch (const UIDDoesNotExist) {
            query_ans.assign(reply_op);
            query_ans.append(" NOK");
        } catch (const NoFiles) {
            query_ans.assign(reply_op);
            query_ans.append(" EOF");
        }

        // Reply to the Retrieve command
    } else if (reply_op.compare("RRT") == EQUAL) {
        try {
            query_ans.append("OK ");
            query_ans.append(to_string(
                fs->get_file(op_data.get_user_id(), op_data.get_file_name())
                    .get_size()));
            query_ans.append(" ");
            query_ans.append(
                fs->read_file(op_data.get_user_id(), op_data.get_file_name()));
        } catch (const FileNotFound) {
            query_ans.assign(reply_op);
            query_ans.append(" EOF");
        } catch (const UIDDoesNotExist) {
            query_ans.assign(reply_op);
            query_ans.append(" NOK");
        }

        // Reply to the Upload command
    } else if (reply_op.compare("RUP") == EQUAL) {
        try {
            query_ans.append("OK");
            string file_path;
            file_path.append("FS_files/");
            file_path.append(to_string(op_data.get_user_id()));
            file_path.append("_");
            file_path.append(op_data.get_file_name());
            File file = File(op_data.get_file_name(), op_data.get_file_size(),
                             file_path);
            fs->add_file(op_data.get_user_id(), file, op_data.get_file_data());
        } catch (const UIDDoesNotExist) {
            query_ans.assign(reply_op);
            query_ans.append(" NOK");
        } catch (const FileAlreadyExists) {
            query_ans.assign(reply_op);
            query_ans.append(" DUP");
        } catch (const MaxFilesReached) {
            query_ans.assign(reply_op);
            query_ans.append(" FULL");
        }

        // Reply to the File Deletion command
    } else if (reply_op.compare("RDL") == EQUAL) {
        try {
            fs->get_file(op_data.get_user_id(), op_data.get_file_name());
            fs->delete_file(op_data.get_user_id(), op_data.get_file_name());
            query_ans.append("OK");
        } catch (const FileNotFound) {
            query_ans.append("EOF");
        } catch (const UIDDoesNotExist) {
            query_ans.append("NOK");
        }

        // Reply to the User Deletion command
    } else if (reply_op.compare("RRM") == EQUAL) {
        try {
            fs->delete_personal_fs(op_data.get_user_id());
            query_ans.append("OK");
        } catch (const UIDDoesNotExist) {
            query_ans.append("NOK");
        }
    }

    return query_ans;
}

string recv_n_chars_msg_tcp(int *socket, ssize_t num_chars, bool verbose) {
    char c[2] = {0};

    string new_msg;
    ssize_t chars_read;
    while (num_chars > 0) {
        if (num_chars == 0) {
            read(*socket, c, 1);
            break;
        }
        chars_read = read(*socket, c, 1);
        new_msg.push_back(c[0]);
        if (chars_read < 0) {
            if (ok_to_exit) {
                return string();
            }
            print_verbose(verbose, "sdss",
                          "Failed at reading TCP message with error: ", errno,
                          "\n", strerror(errno));
        }
        if (ok_to_exit) {
            return string();
        }
        num_chars -= 1;
    }
    if (verbose) {
        char ip[30];
        bzero(ip, sizeof(char) * 30);
        struct sockaddr_in client_addr;
        bzero(&client_addr, sizeof(client_addr));
        socklen_t len = sizeof(client_addr);
        getsockname(*socket, (struct sockaddr *)&client_addr, &len);
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
        unsigned int port = ntohs(client_addr.sin_port);
        printf("Receiving message from user.\n\tIP: %s\n\tPort: %u\n", ip,
               port);
    }

    return new_msg;
}

string recv_tcp_msg(int *socket, int verbose) {
    string new_msg;

    char c[2] = {0};
    size_t space_count = 0;
    do {
        ssize_t chars_read = read(*socket, c, 1);

        if (c[0] == ' ') {
            space_count++;
        }
        // If 5 spaces where counted (aka, read Fsize) or
        if (chars_read == 0) {
            return string();
        }
        if (space_count == 5 || c[0] == '\n') {
            break;
        } else {
            new_msg.push_back(c[0]);
        }
        if (chars_read < 0) {
            if (ok_to_exit) {
                return string();
            }
            print_verbose(verbose, "sdss",
                          "Failed at reading TCP message with error: ", errno,
                          "\n", strerror(errno));

            exit(EXIT_FAILURE);
        }
    } while (LOOP);
    if (verbose) {
        char ip[16];
        bzero(ip, sizeof(char) * 16);
        struct sockaddr_in client_addr;
        bzero(&client_addr, sizeof(client_addr));
        socklen_t len = sizeof(client_addr);
        getsockname(*socket, (struct sockaddr *)&client_addr, &len);
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
        unsigned int port = ntohs(client_addr.sin_port);
        printf("Receiving message from user.\n\tIP: %s\n\tPort: %u\n", ip,
               port);
    }

    return new_msg;
}

void send_msg_tcp(int *socket, string msg, bool verbose) {
    ssize_t chars_written =
        write(*socket, msg.c_str(),
              msg.length()); // Write as many chars as possible
    if (chars_written < 0) {
        print_verbose(verbose, "sdss",
                      "Failed at writing TCP message with error: ", errno, "\n",
                      strerror(errno));
    }
}

string recv_small_msg_udp(int *socket, sockaddr_in *addr, socklen_t *len,
                          string panic_fmt_msg) {
    char buffer[MAX_BUF];
    bzero(buffer, sizeof(char) * MAX_BUF);

    ssize_t chars_read =
        recvfrom(*socket, buffer, MAX_BUF, 0, (struct sockaddr *)addr, len);
    if (chars_read < 0) {
        if (ok_to_exit) {
            return string();
        }
        panic(panic_fmt_msg.c_str(), errno, strerror(errno));
    }

    return string(buffer);
}

string recv_n_chars_msg_udp(int *socket, ssize_t num_chars, sockaddr_in *addr,
                            socklen_t *len, string panic_fmt_msg) {
    char *buffer = (char *)malloc(sizeof(char) * num_chars);
    bzero(buffer, sizeof(char) * num_chars);

    ssize_t chars_read;
    while (num_chars > 0) {
        chars_read =
            recvfrom(*socket, buffer, MAX_BUF, 0, (struct sockaddr *)addr, len);
        if (chars_read < 0) {
            if (ok_to_exit) {
                return string();
            }
            panic(panic_fmt_msg.c_str(), errno, strerror(errno));
        }
        num_chars -= chars_read;
        buffer += chars_read;
    }

    string new_msg = string(buffer);
    free(buffer);
    return new_msg;
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

void talk_to_as_server(Data *data, vector<string> *msg,
                       OperationData *as_op_data) {
    int as_socket;
    struct sockaddr_in as_addr;
    bzero(&as_addr, sizeof(as_addr));
    as_addr.sin_family = AF_INET;
    as_addr.sin_port = htons(data->get_asport());
    as_addr.sin_addr.s_addr = inet_addr(data->get_asip().c_str());
    socklen_t as_addr_len = sizeof(as_addr);

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGPIPE);
    sigaddset(&mask, SIGABRT);
    sigaddset(&mask, SIGINT);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    if ((as_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        panic("Failed at creating AS server socket with error: %d\n%s", errno,
              strerror(errno));
    }

    as_socket_ref = as_socket;

    int opt = 1;
    if (setsockopt(as_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt)) < 0) {
        if (ok_to_exit) {
            return;
        }
        panic("Failed at setting FS server socket options with error: %d\n%s",
              errno, strerror(errno));
    }
    if (ok_to_exit) {
        return;
    }

    do {
        smph_ch.acquire(); // Wait for FS to get a client request and its info
                           // ready to be sent to the AS

        if (ok_to_exit) {
            break;
        }

        string command = format_to_str(*msg);

        command.append("\n");
        send_msg_udp(&as_socket, &as_addr, &as_addr_len, command,
                     "Failed at sending UDP message to AS with error: %d\n%s");
        if (ok_to_exit) {
            break;
        }

        string as_reply = recv_small_msg_udp(
            &as_socket, &as_addr, &as_addr_len,
            "Failed at receiving message from AS with error: %d\n%s");
        if (as_reply.empty()) {
            break;
        }

        as_reply = trim(as_reply);

        if (ok_to_exit) {
            break;
        }

        print_verbose(
            data->get_vrbs_mode(), "sssd",
            "Receiving message from AS.\n\tIP: ", data->get_asip().c_str(),
            "\n\tPort: ", data->get_asport());

        auto as_ans = split_with(as_reply, ' ');
        try {
            parse_args_as(as_op_data, &as_ans);
        } catch (const InvalidCommand &ic) {
            string err_msg = string("ERR\n");
            send_msg_udp(&as_socket, &as_addr, &as_addr_len, err_msg.c_str(),
                         "Failed at sending \"ERR\" message to AS server with "
                         "error: %d\n%s");
        }
        as_replied_smph.release(); // Signal that the FS can proceed with the
                                   // fulfillment of the User request
        if (ok_to_exit) {
            break;
        }
    } while (LOOP);
}

void handle_client(Data *data, int client_socket, FileSystem *fs,
                   vector<string> *msg_vec, OperationData *as_op_data) {

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGPIPE);
    sigaddset(&mask, SIGABRT);
    sigaddset(&mask, SIGINT);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    if (ok_to_exit) {
        return;
    }
    wait_other_client.acquire();
    string msg = recv_tcp_msg(&client_socket, data->get_vrbs_mode());

    if (ok_to_exit) {
        return;
    }
    if (!msg.empty()) {
        auto args = split_with(msg, ' ');
        OperationData ops_data;
        parse_args_client(&ops_data, &args); // Parse user message

        if (ops_data.get_file_size() != NONE) {
            // Receive file from user
            string data_rcvd =
                recv_n_chars_msg_tcp(&client_socket, ops_data.get_file_size(),
                                     data->get_vrbs_mode());
            if (data_rcvd.empty()) {
                return;
            }

            ops_data.set_file_data(data_rcvd);
        }
        if (ok_to_exit) {
            return;
        }

        msg_vec->push_back(
            "VLD"); // Build validation enquiry message to send to AS
        msg_vec->push_back(to_string(ops_data.get_user_id()));
        msg_vec->push_back(to_string(ops_data.get_trans_id()));

        if (ok_to_exit) {
            return;
        }

        smph_ch.release(); // Signal that the info is ready to be sent to the AS
        if (ok_to_exit) {
            return;
        }
        as_replied_smph.acquire(); // Wait for AS reply
        string query_ans = get_client_query_ans(fs, ops_data, as_op_data);
        if (data->get_vrbs_mode()) {
            fs->debug_print();
        }
        query_ans.append("\n");
        send_msg_tcp(&client_socket, query_ans, data->get_vrbs_mode());
    }

    msg_vec->clear();
    as_op_data->clear();
    remove(client_sockets.begin(), client_sockets.end(), client_socket);
    close(client_socket);
    wait_other_client.release();
}

void get_user_requests(Data *data, FileSystem *fs, vector<string> *msg,
                       OperationData *as_op_data) {
    int server_fd, opt = 1;
    struct sockaddr_in serv_addr;

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGPIPE);
    sigaddset(&mask, SIGABRT);
    sigaddset(&mask, SIGINT);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        panic("Failed at creating FS server socket with error: %d\n%s", errno,
              strerror(errno));
    }
    server_fd_ref = server_fd;

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt)) < 0) {
        panic("Failed at setting FS server socket options with error: %d\n%s",
              errno, strerror(errno));
    }

    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(data->get_fsport());
    socklen_t addr_len = sizeof(serv_addr);

    if (bind(server_fd, (struct sockaddr *)&serv_addr, addr_len) < 0) {
        panic("Failed at binding FS server socket with error: %d\n%s", errno,
              strerror(errno));
    }

    if (listen(server_fd, 101) < 0) {
        panic("Failed at listening to the FS server socket with error: %d\n%s",
              errno, strerror(errno));
    }

    do {
        if (ok_to_exit) {
            break;
        }
        int client_socket;
        if ((client_socket = accept(server_fd, (struct sockaddr *)&serv_addr,
                                    (socklen_t *)&addr_len)) < 0) {
            if (ok_to_exit) {
                break;
            }
            panic("Failed at accepting FS server client(an user) with error: "
                  "%d\n%s",
                  errno, strerror(errno));
        }
        if (ok_to_exit) {
            break;
        }
        client_sockets.push_back(client_socket);

        if (ok_to_exit) {
            break;
        }

        thread new_client(handle_client, data, client_socket, fs, msg,
                          as_op_data);
        new_client.detach();
    } while (LOOP);
}

int main(int argc, char *argv[]) {
    Data data = Data();
    FileSystem *fs = new FileSystem();
    vector<string> msg;
    OperationData as_op_data; // AS operation data

    fs_ptr = fs;

    parse_args(argc, argv, &data);

    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        print_verbose(data.get_vrbs_mode(),
                      "Failed installing a signal handler for the main thread "
                      "with error: %d\n %s\n",
                      errno, strerror(errno));
    }
    if (signal(SIGABRT, sig_handler) == SIG_ERR) {
        print_verbose(data.get_vrbs_mode(),
                      "Failed installing a signal handler for the main thread "
                      "with error: %d\n %s\n",
                      errno, strerror(errno));
    }

    wait_other_client.set_count(1);

    struct stat st;
    if (stat("FS_files/", &st) != PATH_EXISTS) {
        if (mkdir("FS_files/", 0777) < 0) {
            panic("Couldn't create directory. Error: %d", errno,
                  strerror(errno));
        }
    }

    thread t1(get_user_requests, &data, fs, &msg, &as_op_data);
    thread t2(talk_to_as_server, &data, &msg, &as_op_data);

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
}
