/*
** http_client.c -- a stream socket client demo
*/

// C header
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

// CPP header
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#define MAXDATASIZE 2048 // max number of bytes we can get at once

class UrlParser
{
private:
	std::string addr ="";
	std::string port ="";
	std::string file_path ="";
public:
	UrlParser(std::string url)
	{
		// url sample http://12.34.56.78:8888/somefile.txt
		
		auto double_slash_pos = url.find_first_of("//");
		if(double_slash_pos == url.npos){
			printf("Invaild argument, please check the input\n");
			exit(1);
		}
		auto url_without_protocal = url.substr(double_slash_pos + 2);
		// url_without_protocal sample 12.34.56.78:8888/somefile.txt

		auto single_slash_pos = url_without_protocal.find_first_of("/");
		if(single_slash_pos == url_without_protocal.npos){
			printf("Invaild argument, please check the input\n");
			exit(1);
		}
		std::string addr_with_port = url_without_protocal.substr(0, single_slash_pos);
		// addr_with_port sample 12.34.56.78:8888

		auto semicolon_pos = addr_with_port.find_first_of(":");
		if(addr_with_port.find(":") != addr_with_port.npos){
			addr = addr_with_port.substr(0, semicolon_pos);
			port = addr_with_port.substr(semicolon_pos + 1);
		}else{
			addr = addr_with_port;
			port = "80";
		}
		file_path = url_without_protocal.substr(single_slash_pos);

	}
	std::string getAddr(){
		return addr;
	}
	std::string getPort(){
		return port;
	}
	std::string getFilePath(){
		return file_path;
	}
};

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in *)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd;
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	if (argc != 2)
	{
		fprintf(stderr, "usage: ./http_client http(s)://hostname:portNum/path2file\n");
		exit(1);
	}

	//input sample "http://12.34.56.78:8888/somefile.txt"

	std::string url = argv[1];
	UrlParser url_parser(url);
	std::string addr = url_parser.getAddr();
	std::string port = url_parser.getPort();
	std::string file_path = url_parser.getFilePath();


	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(addr.c_str(), port.c_str(), &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next)
	{
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
		{
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL)
	{
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	/* sample http header
	GET /test.txt HTTP/1.1
	User-Agent: Wget/1.12 (linux-gnu)
	Host: localhost:3490
	Connection: Keep-Alive

	*/
	auto http_get_msg = "GET " + file_path + " HTTP/1.1" + "\r\n"
						+ "User-Agent: Wget/1.12 (linux-gnu)" + "\r\n"
						+ "Host: " + addr + ":" + port +"\r\n"
						+ "Connection: Keep-Alive" + "\r\n"
						+ "\r\n";
	printf("The http request is\n");
	printf("%s",http_get_msg.c_str());

	send(sockfd,http_get_msg.c_str(),http_get_msg.size(),0);

	auto fd = fopen("output", "wb");

	bool first_time = true;
	// This flag will be used to identify whether the packet is first packet
	// since there exist part such as 200 OK which we should not write to file
	long long total_size = 0;
	ssize_t numbytes = 0;
	while ((numbytes = recv(sockfd, buf, MAXDATASIZE, 0) )> 0)
	{

		//auto 
		
		if (numbytes == -1)
		{
			perror("recv");
			exit(1);
		}else if (numbytes == 0) 
		{
			break;
		}else 
		{
			if(first_time)
			// the first packet is respond result,not file content
			{
				first_time = false;
				std::string content(buf);
				auto first_change_line_pos = content.find_first_of("\r\n\r\n");
				auto first_line = content.substr(0,first_change_line_pos); 
				auto first_space_pos = first_line.find_first_of(" ");
				auto sec_space = first_line.substr(first_space_pos + 1).find_first_of(" ");
				auto result = first_line.substr(first_space_pos + 1, sec_space);

				
				if(numbytes > first_change_line_pos + 4){
					int remain = numbytes - first_change_line_pos - 4;
					void* temp = malloc(sizeof(char) * remain);
					memcpy((char*)temp, buf + first_change_line_pos + 4,remain);
					fwrite(temp, sizeof(char), remain, fd);

					std::cout << "write " << numbytes <<" to file"<< std::endl;
					total_size += numbytes;
				}

				if(result != "200")
				{
					printf("fail");
					break;
				}
			}else
			{
				fwrite(buf, sizeof(char), numbytes, fd);
				std::cout << "write " << numbytes <<" to file"<< std::endl;

				total_size += numbytes;
			}

		}

	}
	std::cout << "total " << total_size <<" of the file"<< std::endl;
	fclose(fd);
	close(sockfd);

	return 0;
}
