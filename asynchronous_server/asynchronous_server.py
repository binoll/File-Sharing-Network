import asyncio
import socket
from typing import Tuple, List, Optional
from collections import defaultdict
import logging

BUFFER_SIZE = 1024
BACKLOG = 10
commands = ['list', 'get', 'exit', 'error', 'exist']

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger('Logger')


class FileInfo:
    def __init__(self, size: int, hash_value: str, filename: str, is_filename_changed: bool = False,
                 is_filename_modify: bool = False):
        self.size = size
        self.hash = hash_value
        self.filename = filename
        self.is_filename_changed = is_filename_changed
        self.is_filename_modify = is_filename_modify


class Connection:
    def __init__(self):
        self.storage: defaultdict[Tuple[socket.socket, socket.socket], List[FileInfo]] = defaultdict(list)
        self.addr_listen: Tuple[str, int] = ('0.0.0.0', 0)
        self.addr_communicate: Tuple[str, int] = ('0.0.0.0', 0)
        self.loop = asyncio.get_event_loop()

    async def wait_connection(self):
        try:
            socket_communicate = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            socket_communicate.bind(self.addr_communicate)
            socket_communicate.listen(BACKLOG)
            socket_communicate.setblocking(False)

            socket_listen = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            socket_listen.bind(self.addr_listen)
            socket_listen.listen(BACKLOG)
            socket_listen.setblocking(False)

            logger.info(f'Server is listening on port for communicate: {socket_communicate.getsockname()[1]}')
            logger.info(f'Server is listening on port for listening: {socket_listen.getsockname()[1]}')

            await asyncio.create_task(self.accept_connections(socket_listen, socket_communicate))
        except OSError as e:
            logger.error(e)

    async def accept_connections(self, socket_listen, socket_communicate):
        try:
            while True:
                client_socket_communicate, _ = await self.loop.sock_accept(socket_communicate)
                client_socket_listen, _ = await self.loop.sock_accept(socket_listen)
                client_socket_listen.setblocking(False)
                client_socket_communicate.setblocking(False)

                synchronization_result = await self.loop.create_task(
                    self.synchronization(client_socket_listen, client_socket_communicate)
                )

                if synchronization_result:
                    logger.info(
                        f'Client connected: {client_socket_listen.getpeername()},'
                        f' {client_socket_communicate.getpeername()}'
                    )
                    self.loop.create_task(self.handle_client(client_socket_listen, client_socket_communicate))
                else:
                    logger.info(
                        'The client cannot connect. '
                        'The storage could not be synchronized'
                    )
                    self.close_client_connections(client_socket_listen, client_socket_communicate)
        except OSError as e:
            logger.error(e)

    @staticmethod
    def close_client_connections(client_socket_listen, client_socket_communicate):
        try:
            client_socket_listen.close()
            client_socket_communicate.close()
        except OSError as e:
            logger.error(e)

    async def handle_client(self, client_socket_listen, client_socket_communicate):
        command_error = commands[3]
        command_exist = commands[4]

        try:
            while True:
                command = await self.receive_message(client_socket_listen, BUFFER_SIZE)

                if not command:
                    return

                if command == commands[0]:
                    result = await self.send_list(client_socket_listen)

                    if result < 0:
                        await self.send_message(client_socket_listen, command_error)
                elif command.startswith(commands[1]):
                    filename = command.split(':')[1]
                    result = await self.send_file(client_socket_listen, filename)

                    if result == -1:
                        await self.send_message(client_socket_listen, command_exist)
                    elif result == -2:
                        await self.send_message(client_socket_listen, command_error)
                elif command == commands[2]:
                    self.remove_clients((client_socket_listen, client_socket_communicate))
                    self.close_client_connections(client_socket_listen, client_socket_communicate)
                    logger.info(f'Client disconnected: {client_socket_listen.getpeername()},'
                                f' {client_socket_communicate.getpeername()}')
                    break
        except OSError as e:
            logger.error(e)

    async def synchronization(self, client_socket_listen: socket.socket,
                              client_socket_communicate: socket.socket) -> bool:
        command_error = commands[3]

        try:
            message = await self.receive_message(client_socket_communicate, BUFFER_SIZE)
            if not message or message == command_error:
                return False

            message_size, message = self.process_response(message)
            if message_size is None:
                return False

            while message_size > 0:
                try:
                    for file_info in message.split(' '):
                        if not file_info:
                            break

                        filename, file_size_str, file_hash = file_info.split(':')
                        file_size = int(file_size_str)
                        self.store_files(
                            (client_socket_listen, client_socket_communicate),
                            filename, file_size, file_hash
                        )
                except ValueError as e:
                    logger.error(e)
                    return False
                except Exception as e:
                    logger.error(e)
                    return False

                message_size -= len(message)
                if message_size <= 0:
                    break

                message = await self.receive_message(client_socket_communicate, message_size)
                if not message or message == command_error:
                    return False

            self.update_storage()
            return True
        except OSError as e:
            logger.error(e)
            return False

    async def send_message(self, sock: socket.socket, message: str) -> None:
        try:
            buffer = bytes(message.encode())
            await self.send_bytes(sock, buffer)
        except Exception as e:
            logger.error(e)

    async def receive_message(self, sock: socket.socket, size: int) -> Optional[str]:
        try:
            buffer = await self.receive_bytes(sock, size)
            if not buffer:
                return None
            buffer_str = buffer.decode()
            return buffer_str
        except UnicodeDecodeError:
            return None
        except Exception as e:
            logger.error(e)
            return None

    async def send_bytes(self, sock: socket.socket, buffer: bytes) -> None:
        try:
            await self.loop.sock_sendall(sock, buffer)
        except Exception as e:
            logger.error(e)

    async def receive_bytes(self, sock: socket.socket, size: int) -> bytes | None:
        try:
            buffer = await self.loop.sock_recv(sock, size)
            if not buffer:
                return None
            return buffer
        except Exception as e:
            logger.error(e)
            return None

    @staticmethod
    def process_response(message: str) -> tuple[int, str] | None:
        try:
            size, _, rest = message.partition(':')
            return int(size), rest
        except ValueError:
            return None

    async def send_list(self, sock: socket.socket) -> int | None:
        try:
            files = self.get_list_files()
            list_str = ' '.join(files)
            message_size = len(list_str)
            message = f'{message_size}:{list_str}'
            await self.send_message(sock, message)
            return message_size
        except Exception as e:
            logger.error(e)
            return -1

    async def send_file(self, sock: socket.socket, filename: str) -> int | None:
        try:
            command_error = commands[3]
            message_size = self.get_size(filename)
            real_filename = filename if not self.is_filename_modify(filename) else self.remove_index(filename)

            if self.is_filename_change(sock, filename):
                return -2

            message = f'{message_size}:'
            await self.send_message(sock, message)

            sockets = self.find_socket(filename)
            sockets = [(pair[0], pair[1]) for pair in sockets if pair[0] != sock and pair[1] != sock]

            if not sockets:
                return -1

            for i, (_, client_socket_communicate) in enumerate(sockets):
                for offset in range(0, message_size, BUFFER_SIZE):
                    client_socket_communicate = sockets[i % len(sockets)][1]
                    chunk_size = min(message_size - offset, BUFFER_SIZE)

                    message = f'{commands[1]}:{offset}:{chunk_size}:{real_filename}'
                    if not self.check_connection(client_socket_communicate):
                        return -1

                    await self.send_message(client_socket_communicate, message)

                    buffer = await self.receive_bytes(client_socket_communicate, chunk_size)

                    if not buffer:
                        return -1

                    try:
                        buffer_str = str(buffer)
                        if buffer_str == command_error:
                            return -2
                    except ValueError as e:
                        logger.error(e)
                        return -1

                    await self.send_bytes(sock, buffer)
            return 0
        except Exception as e:
            logger.error(f'Error: {e}')
            return -1

    def find_socket(self, filename: str) -> List[Tuple[socket.socket, socket.socket]]:
        try:
            return [(key[0], key[1]) for key, values in self.storage.items() for value in values if
                    value.filename == filename]
        except Exception as e:
            logger.error(e)
            return []

    def get_list_files(self) -> List[str]:
        try:
            filename_counts = defaultdict(int)
            unique_filenames = []
            duplicate_filenames = []

            for values in self.storage.values():
                for value in values:
                    filename_counts[value.filename] += 1

            for filename, count in filename_counts.items():
                if count == 1:
                    unique_filenames.append(filename)
                else:
                    duplicate_filenames.append(filename)

            unique_filenames.extend(duplicate_filenames)

            return unique_filenames
        except Exception as e:
            logger.error(e)
            return []

    def get_size(self, filename: str) -> int:
        try:
            for pair, file_infos in self.storage.items():
                for file_info in file_infos:
                    if file_info.filename == filename:
                        return file_info.size
            return -1
        except Exception as e:
            logger.error(e)
            return -1

    def update_storage(self):
        hash_count = defaultdict(int)

        try:
            for pair, file_infos in self.storage.items():
                for file_info in file_infos:
                    hash_count[file_info.hash] += 1

            for first_pair, first_file_infos in self.storage.items():
                for second_pair, second_file_infos in self.storage.items():
                    if first_pair == second_pair:
                        continue

                    for first_file_info in first_file_infos:
                        for second_file_info in second_file_infos:
                            if (first_file_info.hash != second_file_info.hash and
                                    first_file_info.filename == second_file_info.filename):
                                file_occurrences = hash_count[first_file_info.hash]
                                if file_occurrences > 1:
                                    first_file_info.filename += f'({file_occurrences - 1})'
                                    second_file_info.filename += f'({file_occurrences})'
                                    first_file_info.is_filename_modify = True
                                    second_file_info.is_filename_modify = True
                                    hash_count[first_file_info.hash] -= 1
                                    hash_count[second_file_info.hash] -= 1
                            elif (first_file_info.hash == second_file_info.hash and
                                  first_file_info.filename != second_file_info.filename):
                                second_file_info.filename = first_file_info.filename
                                second_file_info.is_filename_changed = True
        except Exception as e:
            logger.error(e)

    def store_files(self, pair: Tuple[socket.socket, socket.socket], filename: str, size: int, hash_val: str):
        try:
            data = FileInfo(size, hash_val, filename)
            self.storage[pair].append(data)
            logger.info(f'Stored the file: {filename}')
        except Exception as e:
            logger.error(e)

    def remove_clients(self, pair: Tuple[socket.socket, socket.socket]):
        try:
            keys_to_remove = []

            for key, values in self.storage.items():
                if key == pair:
                    keys_to_remove.append(key)
                    filename = self.remove_index(values[0].filename)

                    for entry_value in self.storage[key]:
                        if entry_value.filename.startswith(filename):
                            entry_value.filename = self.remove_index(entry_value.filename)

            for key in keys_to_remove:
                del self.storage[key]

            self.update_storage()
        except Exception as e:
            logger.error(e)

    @staticmethod
    def remove_index(filename: str) -> str:
        try:
            pos = filename.rfind('(')
            if pos != -1:
                filename = filename[:pos]
            return filename
        except Exception as e:
            logger.error(e)
            return filename

    def is_filename_modify(self, filename: str) -> bool:
        try:
            for values in self.storage.values():
                for value in values:
                    if value.filename == filename:
                        return value.is_filename_modify
            return False
        except Exception as e:
            logger.error(e)
            return False

    def is_filename_change(self, sock: socket.socket, filename: str) -> bool:
        try:
            for key, values in self.storage.items():
                if key[0] == sock or key[1] == sock:
                    for value in values:
                        if value.filename == filename:
                            return value.is_filename_changed
            return False
        except Exception as e:
            logger.error(e)
            return False

    @staticmethod
    def check_connection(client_socket_communicate: socket.socket) -> bool:
        try:
            client_socket_communicate.getpeername()
            return True
        except OSError:
            return False
        except Exception as e:
            logger.error(e)
            return False


async def main():
    connection = Connection()
    await connection.wait_connection()


if __name__ == "__main__":
    loop = asyncio.get_event_loop()
    loop.run_until_complete(main())
