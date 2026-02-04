#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <hiredis/hiredis.h>
#include <chrono>
#include <ctime>

int main() {
    // Create a basic TCP server on port 5000
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(5000);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 10);

    char hostname[1024];
    gethostname(hostname, 1024);

    // Connect to Redis , retry until it’s up
    redisContext *c = nullptr;
    while (true) {
        c = redisConnect("redis", 6379);
        if (c != nullptr && !c->err) break;
        if (c) redisFree(c);
        sleep(1);
    }

    long long current_count = 0;

    // Main request loop
    while (true) {
        int new_socket = accept(server_fd, NULL, NULL);
        char buffer[1024] = {0};
        read(new_socket, buffer, 1024);
        std::string request(buffer);

        // If someone wipes redis state, clear the counter and bounce back to /
        if (request.find("GET /reset") != std::string::npos) {
            redisCommand(c, "SET visitor_count 0");
            std::string redirect = 
                "HTTP/1.1 302 Found\r\n"
                "Location: /\r\n"
                "Connection: close\r\n\r\n";
            send(new_socket, redirect.c_str(), redirect.length(), 0);
            close(new_socket);
            continue; 
        } 
        
        // Normal page load: increment the counter in Redis
        else if (request.find("GET / ") != std::string::npos) {
            redisReply *reply = (redisReply *)redisCommand(c, "INCR visitor_count");
            if (reply) {
                current_count = reply->integer;
                freeReplyObject(reply);
            }
        }

        // Current timestamp for the page
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::string ts = std::ctime(&now);

        // HTML response
        std::string html_content = 
        "<html><head><style>"
        "@keyframes scan { 0% { background-position: 0 -100vh; } 100% { background-position: 0 100vh; } }"
        "@keyframes pulse { 0% { box-shadow: 0 0 5px #00ff41; } 50% { box-shadow: 0 0 20px #00ff41; } 100% { box-shadow: 0 0 5px #00ff41; } }"
        "body {"
        "  background: #020202; color: #00ff41; font-family: 'Courier New', monospace;"
        "  background-image: linear-gradient(0deg, transparent 24%, rgba(0, 255, 65, .05) 25%, rgba(0, 255, 65, .05) 26%, transparent 27%, transparent 74%, rgba(0, 255, 65, .05) 75%, rgba(0, 255, 65, .05) 76%, transparent 77%, transparent);"
        "  background-size: 50px 50px; animation: scan 10s linear infinite; padding: 50px;"
        "}"
        ".glitch-container { border: 2px solid #00ff41; padding: 30px; background: rgba(0, 20, 0, 0.9); animation: pulse 4s infinite; max-width: 800px; margin: auto; }"
        ".stat-row { display: flex; justify-content: space-between; border-bottom: 1px solid #004411; padding: 10px 0; }"
        ".label { color: #008822; font-weight: bold; text-transform: uppercase; letter-spacing: 2px; }"
        ".value { color: #fff; text-shadow: 0 0 8px #00ff41; }"
        ".btn-group { margin-top: 30px; display: flex; gap: 20px; }"
        "a { border: 1px solid #00ff41; padding: 10px 20px; color: #00ff41; text-decoration: none; transition: 0.3s; font-weight: bold; }"
        "a:hover { background: #00ff41; color: #000; box-shadow: 0 0 20px #00ff41; }"
        ".log-window { margin-top: 30px; background: #000; border: 1px solid #004411; padding: 15px; font-size: 0.75em; color: #008822; }"
        "</style></head><body>"
        "<div class='glitch-container'>"
        "<h1>[ NEXUS_LINK_v1.0 ]</h1>"
        "<div class='stat-row'><span class='label'>Active Node:</span><span class='value'>" + std::string(hostname) + "</span></div>"
        "<div class='stat-row'><span class='label'>Service Sync:</span><span class='value'>Redis::Shared_State</span></div>"
        "<div class='stat-row'><span class='label'>Load Balancer:</span><span class='value'>Nginx_Reverse_Proxy</span></div>"
        "<div class='stat-row'><span class='label'>Timestamp:</span><span class='value'>" + ts + "</span></div>"
        "<div style='text-align: center; margin-top: 40px;'>"
        "<div style='font-size: 0.9em; color: #008822;'>GLOBAL_VISITOR_INDEX</div>"
        "<div style='font-size: 5em; font-weight: 900;'>" + std::to_string(current_count) + "</div>"
        "</div>"
        "<div class='btn-group'>"
        "<a href='/'>REFRESH</a>"
        "<a href='/reset' style='border-color: #ff0000; color: #ff0000;'>WIPE REDIS STATE</a>"
        "</div>"
        "<div class='log-window'>"
        "  <div>[ SYSTEM ] Connection_Established: " + std::string(hostname) + "</div>"
        "  <div>[ KERNEL ] Syncing_Redis_State... SUCCESS</div>"
        "  <div style='color: #00ff41; border-top: 1px solid #004411; margin-top: 5px; padding-top: 5px;'>[ READY ] Awaiting inbound packet from NGINX_PROXY</div>"
        "</div>"
        "</div>"
        "</body></html>";

        std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n"
                               "Content-Length: " + std::to_string(html_content.length()) + "\r\n"
                               "Connection: close\r\n\r\n" + html_content;
        
        send(new_socket, response.c_str(), response.length(), 0);
        close(new_socket);
    }
    return 0;
}