#include <iostream>
#include <string>
#include <vector>
#include <cctype>

#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <syslog.h>

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>

using namespace std;
using namespace boost::placeholders;
using boost::asio::ip::tcp;

#define PORT 8080

// Session class for each client connection
// Handles reading commands and sending responses
// Commands supported: "cpu" for CPU load, "mem" for memory usage
class session {
public:
    session(boost::asio::io_service& io_service)
        : socket_(io_service) {
    }

    tcp::socket& socket() {
        return socket_;
    }

    void start() {
        syslog(LOG_INFO, "Client connected");
        start_read();
    }      
    
private:
    // Start reading from the socket
    // asynchronously reads until a newline character
    // this makes it possible for the server to handle multiple clients
    void start_read() {
        boost::asio::async_read_until(socket_, buffer_, "\n",
            boost::bind(&session::handle_read, this, _1, _2));
    }

    // takes the input from the client, parses it, and handles the command
    // if the command is valid, it sends the response back to the client
    // again asynchronously
    void handle_read(const boost::system::error_code& error, size_t bytes_transferred) {
        if(!error) {
            istream input_stream(&buffer_);
            string line;
            getline(input_stream, line);
            string parsed = parseCommand(line);
            string response = handleCommand(parsed);
            response += "\n";
            boost::asio::async_write(socket_, boost::asio::buffer(response),
                    boost::bind(&session::handle_write, this, _1));
        } else {
            syslog(LOG_INFO, "Client disconnected: %s", error.message().c_str());
        }
    }

    // callback for when the write operation is complete
    void handle_write(const boost::system::error_code& error) {
        if (!error) {
            start_read();
        } else {
            syslog(LOG_ERR, "Write error: %s", error.message().c_str());
        }
    }

    // cpu -- Returns the current CPU load.
    double getCpuLoad() {
        double load[1];
        // get the load average for the last minute
        getloadavg(load, 1);
        return load[0];
    }

    // mem -- Returns memory usage in kilobytes.
    long getMemUsageKB() {
        struct sysinfo si;
        sysinfo(&si);
        return (si.totalram - si.freeram) * si.mem_unit / 1024;
    }

    string parseCommand(string input) {
        string clean = input;

        // remove leading whitespace
        while(!clean.empty() && isspace(clean.front())) {
            clean = clean.substr(1);
        }

        // remove trailing whitespace
        while(!clean.empty() && isspace(clean.back())) {
            clean = clean.substr(0, clean.length() - 1);
        }

        // convert to lowercase
        for (char& c : clean) {
            c = tolower(c);
        }

        // validate command
        if(clean != "cpu" && clean != "mem") {
            return "error";
        }

        return clean;
    }

    // simple command handler
    // logs the command and returns the appropriate response
    string handleCommand(string command) {
        if (command == "cpu") {
            syslog(LOG_INFO, "Client requested CPU usage");
            return "CPU Load: " + to_string(getCpuLoad());
        } else if (command == "mem") {
            syslog(LOG_INFO, "Client requested memory usage");
            return "Memory Usage: " + to_string(getMemUsageKB()) + " KB";
        } else {
            syslog(LOG_ERR, "Client submitted unknown command");
            return "Error";
        }
    }

private:
    tcp::socket socket_;
    boost::asio::streambuf buffer_;
};

// Server clas s to accept client connections
// Manages multiple sessions asynchronously
class server {
public:
    server(boost::asio::io_service& io_service, short port)
        : io_service_(io_service),
        acceptor_(io_service, tcp::endpoint(boost::asio::ip::address_v4::any(), port)) {
        start_accept();
    }

    // starts accepting asynchronously new client connections
    // when a new client connects, it creates a new session
    // and starts reading from that session
    void start_accept() {
        session* new_session = new session(io_service_);

        acceptor_.async_accept(new_session->socket(),
            boost::bind(&server::handle_accept, this, new_session, _1));
    }
    
    // if there was no error, starts the session
    // starts accepting new connections
    void handle_accept(session* new_session, const boost::system::error_code& error) {
        if (!error) {
            new_session->start();
            start_accept();
        } else {
            syslog(LOG_ERR, "Accept error: %s", error.message().c_str());
            delete new_session;
        }
    }

private:
    boost::asio::io_service& io_service_;
    tcp::acceptor acceptor_;
};

// Daemonize the process to run in the background
void daemonize(const char* name) {  
    pid_t pid;

    // fork the new process
    // if error, exit
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    
    // for the parent process, exit
    // so that the child becomes the daemon (child = pid 0)
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    // when daemon creates files they get full permissions
    umask(0);
    openlog(name, LOG_PID, LOG_DAEMON);

    // new session, where the child process becomes the session leader
    // and detaches from the terminal
    // this allows the daemon to run in the background
    if (setsid() < 0) {
        syslog(LOG_ERR, "Failed to create new session");
        exit(EXIT_FAILURE);
    }

    // change directory to root
    // so that the daemon does not block unmounting of the filesystem
    // and close standard file descriptors
    if (chdir("/") < 0) {
        syslog(LOG_ERR, "Failed to change working directory");
        exit(EXIT_FAILURE);
    }

    // close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

int main() {
    //make the process a daemon
    daemonize("ubiquiti_server");

    // initiate server and syslog
    try {
        boost::asio::io_service io_service;
        server server_instance(io_service, PORT);

        syslog(LOG_INFO, "Server listening on port: %d", PORT);

        io_service.run();
        
    } catch (const exception& e) {
        syslog(LOG_ERR, "Server error: %s", e.what());
        closelog();
        return 1;
    }

    syslog(LOG_INFO, "Daemon shutting down");
    closelog();
    return 0;
}