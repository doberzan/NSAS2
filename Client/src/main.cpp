#include <iostream>
#include <chrono>
#include <thread>
#include <climits>

// Windows API headers
#include <winsock2.h>
#include <windows.h>
#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib, "winmm.lib")

// g++ .\main.cpp -l "ws2_32" -l "winmm"

#define MAX_BUFF 1024
#define TRUE 1
#define HEARTBEAT 100

int RUNNING = 1;
char* AUDIO_BUFFER;
using namespace std::chrono_literals;
struct sockaddr_in srv_addr, cli_addr;
const char *msg = "RESP";
const char *init_msg = "INIT";
const char *discon_msg = "DISCONNECT";
SOCKET sock_fd;
WSADATA wsa;

int setup_client(char* address, int port)
{
    // Initialize WSA
    if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
	{
		printf("Failed WSAStartup. Error Code : %d",WSAGetLastError());
		exit(EXIT_FAILURE);
	}

    // Create UDP socket
    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Socket creation failed.");
        exit(EXIT_FAILURE);
    }

    DWORD dw = (1 * 1000) + ((1 + 999) / 1000);
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO,(char *)&dw,sizeof(dw)) < 0) {
        perror("Error");
    }

    // Populate address structs
    memset(&srv_addr, 0, sizeof(srv_addr));
    memset(&cli_addr, 0, sizeof(cli_addr));

    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = inet_addr(address);
    srv_addr.sin_port = htons(port);
    return 0;
}

int playAudio()
{
    PlaySound((LPCSTR)AUDIO_BUFFER, NULL, SND_MEMORY | SND_ASYNC);
    return 0;
}

int download_part()
{
    delete AUDIO_BUFFER;
    unsigned int alloc_size = 0;
    char alloc_size_buff[4];
    char tmp_buff;
    SOCKET dl_sock_fd;
    std::cout << "DOWNLOAD CALL" << std::endl;
    if ((dl_sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Download socket creation failed.");
        exit(EXIT_FAILURE);
    }
    for (int trys = 0; trys <=4; trys++)
    {
        if ((connect(dl_sock_fd, (struct sockaddr*)&srv_addr,sizeof(srv_addr))) < 0) {
            std::cout << "Download connection failed: Try " << trys << std::endl;
        }else
        {
            int data_len = recv(dl_sock_fd, alloc_size_buff, 4, 0);
            alloc_size = alloc_size & 0x00000000;
            alloc_size = (alloc_size + (alloc_size_buff[3] & 0x000000FF)); 
            alloc_size = ((alloc_size << 8) + (alloc_size_buff[2] & 0x000000FF)); 
            alloc_size = ((alloc_size << 8) + (alloc_size_buff[1] & 0x000000FF)); 
            alloc_size = ((alloc_size << 8) + (alloc_size_buff[0] & 0x000000FF)); 
            std::cout << "Audio Part Size: " << alloc_size << std::endl; 
            AUDIO_BUFFER = new char[alloc_size];
            int ptr = 0;
            data_len = 204800;
            for(int ptr=0; data_len > 0; ptr += data_len){
                data_len = recv(dl_sock_fd, AUDIO_BUFFER+ptr, 204800, 0);
                std::cout << "data_len: " << data_len << " PTR: " << ptr << std::endl;
            }
            std::cout << "Download Complete. DATA: " << AUDIO_BUFFER << std::endl;
            FILE* pFile;
            pFile = fopen("file.wav", "wb");
            fwrite(AUDIO_BUFFER, sizeof(char), alloc_size, pFile);
            char* test = new char[10000];
            memset(test, 0x40, 10000);
            PlaySound(NULL, NULL, SND_ASYNC);
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
    std::cout << "Download Failed" << std::endl;
}

int wait_conn(){
    int rcv_num;
    int sock_len = sizeof(srv_addr);
    int hb_count = 0;
    auto last = std::chrono::high_resolution_clock::now();
    char buffer[MAX_BUFF];
    int delta;

    int beats[10] = {0};

    while (RUNNING)
    {
        std::cout << "Sending initial packets" <<std::endl;
        last = std::chrono::high_resolution_clock::now();
        // Send connection packets
        memset(buffer, 0, sizeof(buffer));
        sendto(sock_fd, (const char *)init_msg, strlen(init_msg), 0x0, (const struct sockaddr *) &srv_addr, sock_len);
        sock_len = sizeof(srv_addr);
        rcv_num = recvfrom(sock_fd, (char *)buffer, MAX_BUFF, 0x0, (struct sockaddr*) &srv_addr, &sock_len);
        buffer[rcv_num] = '\0';
        std::cout << "DEBUG: " << buffer << std::endl;
        // If server responds then enter heartbeat loop
        if(strcmp(buffer, "OK") == 0)
        {
            hb_count = 0;
            std::cout << "Connected" << std::endl;
            while(RUNNING)
            {
                // Clear previous message buffer
                memset(buffer, 0, sizeof(buffer));

                // Receive from socket
                sock_len = sizeof(srv_addr);
                rcv_num = recvfrom(sock_fd, (char *)buffer, MAX_BUFF, 0x0, (struct sockaddr*) &srv_addr, &sock_len);
                buffer[rcv_num] = '\0';
                if (strcmp(buffer, "PING") == 0)
                {
                    delta = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()-last).count();
                    std::cout << "Server Response: " << delta-HEARTBEAT << std::endl;
                    // std::this_thread::sleep_for(std::chrono::milliseconds(100-(delta%100)));

                    sendto(sock_fd, (const char *)msg, strlen(msg), 0x0, (const struct sockaddr *) &srv_addr, sock_len);
                    last = std::chrono::high_resolution_clock::now();
                    std::cout << "Heart Beat: " << hb_count << std::endl;
                    
                    // Inc & check heart beat
                    hb_count ++;
                    if (hb_count == INT_MAX)
                    {
                        hb_count = 0;
                    }
                    continue;
                }
                if(strcmp(buffer, "OK") == 0)
                {
                    continue;
                }
                if(strcmp(buffer, "DOWNLOAD") == 0)
                {
                    std::cout << "Downloading part..." << std::endl;
                    std::thread download_thread(download_part);
                    download_thread.join();
                    std::cout << "DEBUG TEST MSG" << std::endl;
                    continue;
                }
                if(strcmp(buffer, "EXECUTE") == 0)
                {
                    std::cout << "Executing..." << std::endl;
                    playAudio();
                    continue;
                }
                if(strcmp(buffer, "KILL") == 0)
                {
                    RUNNING = 0;
                    std::cout << "Exiting..." << std::endl;
                    break;
                }
                std::cout << "SERVER: " << buffer << std::endl;
                break;
            }
        }
        // Init connection loop
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }

    closesocket(sock_fd);
	WSACleanup();

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc == 3)
    {
        std::cout << "Arg count: " << argc << std::endl;
        std::cout << "Args.....: " << argv[1] << " " << argv[2] << std::endl;
    }else
    {
        std::cout << "Invalid arguments provided!" << std::endl;
        std::cout << "Syntax: ./nsas.exe <ip> <port>" << std::endl;
        exit(1);
    }

    // Socket setup
    setup_client(argv[1], std::stoi(argv[2]));
    
    // Run loop
    return wait_conn();
}