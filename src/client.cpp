#include <iostream>
#include <string>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;
using namespace std;

int main(int argc, char* argv[]) {
    string host = "127.0.0.1";
    int port = 8080;
    
    // you can specify the host and port as command line arguments if needed
    if (argc > 1) host = argv[1];
    if (argc > 2) port = stoi(argv[2]);

    // creates a socket and connects to the server
    // reads commands from standard input and sends them to the server
    try {
        boost::asio::io_service io_service;
        tcp::socket socket(io_service);
        tcp::endpoint endpoint(boost::asio::ip::address::from_string(host), port);
        socket.connect(endpoint);
        
        string command;
        while (getline(cin, command)) {

            // skip empty commands
            if (command.empty()) continue;

            // end the loop if the command is "quit"
            if (command == "quit") break;
            
            try {
                command += "\n";
                boost::asio::write(socket, boost::asio::buffer(command));
                
                boost::asio::streambuf buffer;
                boost::asio::read_until(socket, buffer, "\n");
                
                istream response_stream(&buffer);
                string response;
                getline(response_stream, response);
                cout << response << endl;
                
            } catch (const boost::system::system_error& e) {
                // handle server disconnection gracefully
                if (e.code() == boost::asio::error::eof || 
                    e.code() == boost::asio::error::connection_reset ||
                    e.code() == boost::asio::error::broken_pipe) {
                    cout << "Server disconnected. Exiting..." << endl;
                    break;
                } else {
                    cerr << "Network error: " << e.what() << endl;
                    break;
                }
            }
        }
        
    } catch (const exception& e) {
        cerr << "Connection error: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}