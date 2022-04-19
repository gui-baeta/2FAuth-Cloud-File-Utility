/*
 * Group: 28
 *     92433 - Carolina Pereira
 *     92442 - Daniela Castanho
 *     92470 - Guilherme Fontes
 */

#include "pd_client.hpp"

using namespace std;

/* --------------------------- Global variables ----------------------------- */

Semaphore smph_ch; // Message channel semaphore lock
bool loop = true;
bool time_to_panic = false;
bool exit_ok = false;

/* ----------------------------- Miscellaneous ------------------------------ */

string get_current_weekday() {
    vector<string> weekdays = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                               "Thursday", "Friday", "Saturday"};
    time_t raw_time;
    tm *time_info;
    time(&raw_time);
    time_info = localtime(&raw_time);
    if (time_info->tm_hour >= 22) {
        try {
            return weekdays.at(time_info->tm_wday + 1);
        } catch (const out_of_range) {
            return weekdays.at(0);
        }
    }
    return weekdays.at(time_info->tm_wday);
}

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

// Introduces a panic message when something went wrong and exits
void panic(const char *fmt, ...) {
    va_list ap;
    printf(fmt, ap);
    printf("\n");
    time_to_panic = true;
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
        (len_check == NONE ? false : (str_to_check.length() != len_check))) {
        throw InvalidCommand(error_msg); // ^^^ Also, check if the given string
                                         // has a valid length, according to the
                                         // specific message protocol
    }
    return num;
}

/* ----------------------------- PD app related ----------------------------- */

void parse_args(int argc, char *argv[], Data *data) {
    if (argc > 8 || argc < 2) {
        panic("Correct usage is: ./pd PDIP [-d PDport] [-n ASIP] [-p ASport]");
    } else if (argc == 2) {
        data->set_pdip(argv[1]);
    } else { // Check for optional arguments and use them
        data->set_pdip(argv[1]);
        try {
            for (size_t i = 2; i < argc; i++) {
                if (i + 1 != argc) { // Check if parsing is done
                    if (strcmp("-d", argv[i]) ==
                        EQUAL) { // Check for PD port argument
                        data->set_pdport(stoi_(
                            argv[++i],
                            "Personal Device port is not a valid number!"));
                    } else if (strcmp("-n", argv[i]) ==
                               EQUAL) { // Check for AS ip argument
                        data->set_asip(argv[++i]);
                    } else if (strcmp("-p", argv[i]) ==
                               EQUAL) { // Check for AS port argument
                        data->set_asport(stoi_(argv[++i],
                                               "Authentication Server port is "
                                               "not a valid number!"));
                    } else {
                        puts("Invalid arguments were given");
                        exit(EXIT_FAILURE);
                    }
                }
            }
        } catch (const InvalidCommand &ic) {
            puts(ic.what());
            exit(EXIT_FAILURE);
        }
    }
}

string recv_small_msg_udp(int *socket, sockaddr_in *addr, socklen_t *len,
                          string panic_fmt_msg) {
    char buffer[MAX_BUF];
    bzero(buffer, sizeof(char) * MAX_BUF);

    ssize_t chars_read =
        recvfrom(*socket, buffer, MAX_BUF, 0, (struct sockaddr *)addr, len);
    if (chars_read < 0) {
        panic(panic_fmt_msg.c_str(), errno, strerror(errno));
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

bool validate(Data *data, int uid, int val_code, string file_op,
              string file_name) {
    if (data->get_uid() == uid) {
        printf(
            "User request made:\n\tUsed ID: %d\n\tValidation Code: %d\n\tFile "
            "operation: %s\n",
            uid, val_code, file_op.c_str());
        if (!file_name.empty()) {
            printf("\tFile name: %s\n", file_name.c_str());
        }
        return true;
    } else {
        return false;
    }
}

/* -------------------------- PD <=> AS connection -------------------------- */

void send_as_requests(Data *data, queue<vector<string>> *msg_ch,
                      int *as_socket) {
    struct sockaddr_in serv_addr; // AS server info
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(data->get_asport());
    serv_addr.sin_addr.s_addr = inet_addr(data->get_asip().c_str());

    if ((*as_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        panic("PD UDP client socket creation failed with error: %d\n%s", errno,
              strerror(errno));
    }

    socklen_t len = sizeof(serv_addr);
    while (loop) {
        smph_ch.acquire();
        auto args = msg_ch->front(); // Retrieve input from user from...
        if (args.at(0).compare("REG") == EQUAL) { // ...the threads channel...
            args.push_back(data->get_pdip()); // ...and check which message...
            args.push_back(to_string(data->get_pdport())); // ...to send
        } else if (args.at(0).compare("UNR") == EQUAL) {
            args.push_back(to_string(data->get_uid()));
            args.push_back(data->get_pass());
        } else {
            panic("PD app option\"%s\" not available", args.at(0).c_str());
        }

        string command = format_to_str(args);
        command.append("\n");
        send_msg_udp(
            as_socket, &serv_addr, &len, command,
            "Failed at sending message to the AS server with error: %d\n%s");
        msg_ch->pop();

        string msg = recv_small_msg_udp(
            as_socket, &serv_addr, &len,
            "Failed at receiving the message from the AS server with "
            "error: %d\n%s");

        if (msg.compare("RUN OK\n") == EQUAL) {
            exit_ok = true;
            printf("Unregistration successful!\nHave a great %s!\n",
                   get_current_weekday().c_str());
            break;
        } else if (msg.compare("RUN NOK\n") == EQUAL) {
            puts("The User ID is not valid or you didn't register yet!");
        } else if (msg.compare("RRG OK\n") == EQUAL) {
            puts("Registration successful!\nYou can now enjoy our FileSystem "
                 "service!\n*Remember that your quota is only of 15 files*");
        } else if (msg.compare("RRG NOK") == EQUAL) {
            puts("Registration unsuccessful!\nThat User ID is already taken "
                 ":'(\nTry some other User ID :D!!");
        } else {
            panic("Received an invalid return message from AS:\n\t%s",
                  msg.c_str());
        }
    }
}

void accept_as_requests(Data *data, int *pd_socket) {
    struct sockaddr_in serv_addr; // PD server and AS client info
    struct sockaddr_in as_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    bzero(&as_addr, sizeof(as_addr));
    serv_addr.sin_family = AF_INET; // IPv4
    serv_addr.sin_port = htons(data->get_pdport());
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if ((*pd_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        panic("PD UDP server socket creation failed with error: %d\n%s", errno,
              strerror(errno));
    }

    int opt = 1;
    if (setsockopt(*pd_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt)) < 0) {
        time_to_panic = true;
        panic("Failed at setting FS server socket options with error: %d\n%s",
              errno, strerror(errno));
    }

    if (bind(*pd_socket, (const struct sockaddr *)&serv_addr,
             sizeof(serv_addr)) < 0) {
        panic("PD UDP server binding failed with error: %d\n%s", errno,
              strerror(errno));
    }

    socklen_t len = sizeof(as_addr);
    do {
        if (exit_ok) {
            return;
        }
        string msg = recv_small_msg_udp(
            pd_socket, &as_addr, &len,
            "Failed at receiving an AS request with error: %d\n%s");

        if (exit_ok) {
            return;
        }
        auto args = split_with(msg, ' ');

        try {
            if (args.size() > 5) {
                throw InvalidCommand("Received more than 5 (4 mandatory + 1 "
                                     "optional) arguments!");
            }
            if (args.at(0).compare("VLC") == EQUAL) {
                int val_code =
                    stoi_(args.at(2),
                          "Validation Code received is not valid!\nValidation "
                          "Code should be a 4-digit number code!",
                          VC_LEN);
                int uid = stoi_(args.at(1),
                                "User ID received is not valid!\nUser ID "
                                "should be a 5-digit number!",
                                UID_LEN);
                string file_op = args.at(3);
                string file_name = string();
                if (args.size() == 5) {
                    file_name.assign(args.at(4));
                }
                if (exit_ok) {
                    return;
                }
                string reply = string("RVC ");
                reply.append(to_string(uid));
                if (validate(data, uid, val_code, file_op, file_name)) {
                    send_msg_udp(pd_socket, &as_addr, &len,
                                 reply.append(" OK\n"),
                                 "Failed at sending validation reply ok "
                                 "\" OK\" with error: %d\n%s");
                } else {
                    send_msg_udp(pd_socket, &as_addr, &len,
                                 reply.append(" NOK\n"),
                                 "Failed at sending validation reply not ok "
                                 "\"RVC NOK\" with error: %d\n%s");
                }
                if (exit_ok) {
                    return;
                }
            } else {
                throw InvalidCommand(
                    "Command received from AS not valid!It should be VLC!");
            }
        } catch (const InvalidCommand &ic) {
            if (exit_ok) {
                return;
            }
            puts(ic.what());
            send_msg_udp(pd_socket, &as_addr, &len, string("ERR\n"),
                         "Failed at sending error msg \"ERR\" to AS with "
                         "error: %d\n%s");
            continue;
        }
    } while (LOOP);
}

void pd_app(Data *data, queue<vector<string>> *msg_ch) {
    while (LOOP) {
        string command;
        getline(std::cin, command);
        vector<string> args = split_with(command, ' ');
        try {
            if (args.size() > 3) {
                throw InvalidCommand(
                    "Command given was not valid!The valid commands are:\n\t> "
                    "reg UID pass\n\t> exit");
            }
            if (args.at(0).compare("reg") == EQUAL) {
                args.at(0).assign("REG");
                data->set_uid(stoi_(args.at(1),
                                    "User ID received is not valid!\nUser ID "
                                    "should be a 5-digit number!",
                                    UID_LEN));
                if (args.at(2).length() != PASS_LEN) {
                    throw InvalidCommand(
                        "User password received is not valid!\nPassword should "
                        "be a 8-character string!");
                }
                data->set_pass(args.at(2));
                msg_ch->push(args);
                smph_ch.release();
                data->debug_print();

            } else if (args.at(0).compare("exit") == EQUAL) {
                if (data->get_uid() == NONE || data->get_pass().empty()) {
                    printf("This device is not registered to the AS yet!\n");
                } else {
                    args.at(0).assign("UNR");
                    msg_ch->push(args);
                    smph_ch.release();
                    break;
                }
            } else {
                throw InvalidCommand(
                    "Command given was not valid!The valid commands are:\n\t> "
                    "reg UID pass\n\t> exit");
            }
        } catch (const out_of_range) {
            puts("Command given was not valid!The valid commands are:\n\t> reg "
                 "UID pass\n\t> exit");
        } catch (const InvalidCommand &ic) {
            puts(ic.what());
        }
    }
}

/* --------------------------------- Main ----------------------------------- */

int main(int argc, char *argv[]) {
    errno = 0;
    Data data = Data();
    parse_args(argc, argv, &data);

    queue<vector<string>> msg_ch; // Message channel between threads

    int as_socket = 0, pd_socket = 0;
    thread t1(send_as_requests, &data, &msg_ch, &as_socket);
    thread t2(accept_as_requests, &data, &pd_socket);
    thread t3(pd_app, &data, &msg_ch);

    do {
        if (time_to_panic) {
            t1.detach();
            t2.detach();
            t3.detach();
            exit(EXIT_FAILURE);
        }
        if (exit_ok) {
            shutdown(as_socket, SHUT_RDWR);
            shutdown(pd_socket, SHUT_RDWR);
            break;
        }
        this_thread::sleep_for(chrono::milliseconds(500));
    } while (LOOP);

    t1.join();
    t2.join();
    t3.join();
}
