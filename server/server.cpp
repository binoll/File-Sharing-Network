#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

// Структура для хранения данных о клиенте
struct ClientData {
	int socket_fd;
	struct sockaddr_in client_addr;
};

// Функция обработки клиента в отдельном потоке
void *client_handler(void *arg) {
	struct ClientData *client_data = (struct ClientData *)arg;

	char client_ip[INET_ADDRSTRLEN];
	int client_port;

	inet_ntop(AF_INET, &(client_data->client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
	client_port = ntohs(client_data->client_addr.sin_port);
	printf("Установлено соединение с клиентом: %s:%d\n", client_ip, client_port);

	char buffer[1024];
	ssize_t bytes_received;

	while ((bytes_received = recv(client_data->socket_fd, buffer, sizeof(buffer), 0)) > 0) {
		buffer[bytes_received] = '\0';
		printf("%s:%d: %s\n", client_ip, client_port, buffer);

		// Отправляем сообщение обратно клиенту
		send(client_data->socket_fd, buffer, bytes_received, 0);
	}

	if (bytes_received == -1) {
		perror("recv failed");
	}

	if (shutdown(client_data->socket_fd, SHUT_RDWR) == -1) {
		perror("shutdown failed");
	}
	close(client_data->socket_fd);
	free(client_data); // Освобождение памяти, выделенной для структуры данных клиента

	return NULL;
}

int main(void) {
	struct sockaddr_in sa;
	int SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (SocketFD == -1) {
		perror("cannot create socket");
		exit(EXIT_FAILURE);
	}

	memset(&sa, 0, sizeof sa);

	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);

	int port;
	printf("Введите порт: ");
	scanf("%d", &port);
	sa.sin_port = htons(port);

	if (bind(SocketFD, (struct sockaddr *)&sa, sizeof sa) == -1) {
		perror("bind failed");
		close(SocketFD);
		exit(EXIT_FAILURE);
	}

	if (listen(SocketFD, 10) == -1) {
		perror("listen failed");
		close(SocketFD);
		exit(EXIT_FAILURE);
	}

	for (;;) {
		socklen_t client_addr_len = sizeof sa;
		int ConnectFD = accept(SocketFD, (struct sockaddr *)&sa, &client_addr_len);


		if (ConnectFD == -1) {
			perror("accept failed");
			close(SocketFD);
			exit(EXIT_FAILURE);
		}

		// Создаем структуру данных для клиента
		struct ClientData *client_data = malloc(sizeof(struct ClientData));
		if (!client_data) {
			perror("malloc failed");
			close(SocketFD);
			exit(EXIT_FAILURE);
		}
		client_data->socket_fd = ConnectFD;
		memcpy(&(client_data->client_addr), &sa, sizeof sa);

		pthread_t client_thread;
		if (pthread_create(&client_thread, NULL, client_handler, client_data) != 0) {
			perror("pthread_create failed");
			close(ConnectFD);
			free(client_data);
			close(SocketFD);
			exit(EXIT_FAILURE);
		}

		// Освобождение ресурсов для завершенных потоков
		pthread_detach(client_thread);
	}

	close(SocketFD);
	return EXIT_SUCCESS;
}