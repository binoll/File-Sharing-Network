#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

int main() {
	struct sockaddr_in sa{};
	int SocketFD;

	char server_ip[INET_ADDRSTRLEN];
	int server_port;

	printf("Введите IP адрес сервера: ");
	scanf("%s", server_ip);
	printf("Введите порт сервера: ");
	scanf("%d", &server_port);

	SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	if(SocketFD == -1) {
		perror("cannot create socket");
		exit(EXIT_FAILURE);
	}

	memset(&sa, 0, sizeof sa);

	sa.sin_family = AF_INET;
	sa.sin_port = htons(server_port);

	if(connect(SocketFD, (struct sockaddr*) &sa, sizeof sa) == -1) {
		perror("connect failed");
		close(SocketFD);
		exit(EXIT_FAILURE);
	}

	char buffer[1024];
	ssize_t bytes_received;

	printf("Установлено соединение с сервером %s:%d\n", server_ip, server_port);

	while(true) {
		printf("Введите сообщение для отправки (или 'exit' для выхода): ");
		scanf(" %[^\n]", buffer);

		if(strcmp(buffer, "exit") == 0) {
			break;
		}

		send(SocketFD, buffer, strlen(buffer), 0);

		bytes_received = recv(SocketFD, buffer, sizeof(buffer), 0);
		if(bytes_received <= 0) {
			perror("recv failed");
			break;
		}

		buffer[bytes_received] = '\0';
		printf("От сервера получено: %s\n", buffer);
	}

	close(SocketFD);
	return EXIT_SUCCESS;
}