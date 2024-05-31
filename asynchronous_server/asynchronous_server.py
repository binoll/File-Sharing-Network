# Copyright 2024 binoll
import asyncio
import socket
from socket import socket, AF_INET, SOCK_STREAM
from typing import Tuple, List, Optional
from collections import defaultdict
import logging

BUFFER_SIZE = 1000
BACKLOG = 100
commands = ['list', 'get', 'error', 'exist']

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger('Logger')


class FileInfo:
    def __init__(self, size: int, hash_value: str, filename: str, is_filename_changed: bool = False,
                 is_filename_modify: bool = False) -> None:
        self.size = size
        self.hash = hash_value
        self.current_filename = filename
        self.old_filename = filename
        self.is_filename_changed = is_filename_changed
        self.is_filename_modify = is_filename_modify


class Connection:
    def __init__(self) -> None:
        self.storage: defaultdict[Tuple[socket.socket, socket.socket], List[FileInfo]] = defaultdict(list)
        self.addr_listen: Tuple[str, int] = ('127.0.0.1', 0)
        self.addr_communicate: Tuple[str, int] = ('127.0.0.1', 0)
        self.loop = asyncio.get_event_loop()

    async def wait_connection(self) -> None:
        try:
            socket_communicate = socket(AF_INET, SOCK_STREAM)
            socket_communicate.bind(self.addr_communicate)
            socket_communicate.listen(BACKLOG)
            socket_communicate.setblocking(False)

            socket_listen = socket(AF_INET, SOCK_STREAM)
            socket_listen.bind(self.addr_listen)
            socket_listen.listen(BACKLOG)
            socket_listen.setblocking(False)

            logger.info(f'Server is listening on port for listening: {socket_listen.getsockname()[1]}')
            logger.info(f'Server is listening on port for communicate: {socket_communicate.getsockname()[1]}')

            await asyncio.create_task(self.accept_connections(socket_listen, socket_communicate))
        except OSError as e:
            logger.error(e)

    async def accept_connections(self, socket_listen: socket, socket_communicate: socket) -> None:
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

                    self.loop.create_task(self.processing_clients(client_socket_listen, client_socket_communicate))
                else:
                    logger.info(
                        'The client cannot connect. '
                        'The storage could not be synchronized'
                    )

                    self.close_client_connections(client_socket_listen, client_socket_communicate)
        except OSError as e:
            logger.error(e)

    @staticmethod
    def close_client_connections(client_socket_listen: socket, client_socket_communicate: socket) -> None:
        try:
            client_socket_listen.close()
            client_socket_communicate.close()
        except OSError as e:
            logger.error(e)

    async def processing_clients(self, client_socket_listen: socket,
                                 client_socket_communicate: socket) -> None:
        command_list = commands[0]
        command_get = commands[1]
        command_error = commands[2]
        command_exist = commands[3]

        try:
            while True:
                command = await self.receive_message(client_socket_listen, BUFFER_SIZE)

                if command == command_list:
                    result = await self.send_list(client_socket_listen)

                    if result < 0:
                        await self.send_message(client_socket_listen, command_error)
                elif command.startswith(command_get):
                    filename = command.split(':')[1]
                    result = await self.send_file(client_socket_listen, filename)

                    if result == -1:
                        await self.send_message(client_socket_listen, command_error)
                    elif result == -2:
                        await self.send_message(client_socket_listen, command_exist)
                elif len(command) == 0:
                    break

            logger.info(f'Client disconnected: {client_socket_listen.getpeername()},'
                        f' {client_socket_communicate.getpeername()}')
            self.remove_clients((client_socket_listen, client_socket_communicate))
            self.close_client_connections(client_socket_listen, client_socket_communicate)
        except OSError as e:
            logger.error(e)

    async def synchronization(self, client_socket_listen: socket,
                              client_socket_communicate: socket) -> bool:
        command_error = commands[2]

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

                        logger.info(f'Stored the file: {filename}')
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

            self.index_files()

            return True
        except OSError as e:
            logger.error(e)
            return False

    async def send_message(self, sock: socket, message: str) -> int:
        try:
            buffer = message.encode()
            bytes_sent = await self.send_bytes(sock, buffer)
            return bytes_sent
        except Exception as e:
            logger.error(e)
            return -1

    async def receive_message(self, sock: socket, size: int) -> Optional[str]:
        try:
            buffer = await self.receive_bytes(sock, size)
            if buffer is None:
                return None
            return buffer.decode('utf-8')
        except UnicodeDecodeError:
            return None
        except Exception as e:
            logger.error(e)
            return None

    async def send_bytes(self, sock: socket, buffer: bytes) -> int:
        try:
            await self.loop.sock_sendall(sock, buffer)
            return len(buffer)
        except Exception as e:
            logger.error(e)
            return -1

    async def receive_bytes(self, sock: socket, size: int) -> Optional[bytes]:
        try:
            buffer = await self.loop.sock_recv(sock, size)
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

    async def send_list(self, sock: socket) -> int | None:
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

    async def send_file(self, sock: socket, filename: str) -> int:
        try:
            message_size = self.get_size(filename)
            command_error = commands[2]

            if self.is_file_exist_on_client(sock, filename):
                return -2

            sockets = self.find_socket(filename)
            if not sockets:
                return -1

            sockets = [(pair[0], pair[1]) for pair in sockets if
                       pair[0].fileno() != sock.fileno() and pair[1].fileno() != sock.fileno()]

            message = f'{message_size}:'
            bytes_sent = await self.send_message(sock, message)
            if bytes_sent < 0:
                return -1

            total_bytes = 0
            for i, (_, client_socket_communicate) in enumerate(sockets):
                for offset in range(0, message_size, BUFFER_SIZE):
                    client_socket_communicate = sockets[i % len(sockets)][1]
                    chunk_size = min(message_size - offset, BUFFER_SIZE)

                    if not self.check_connection(client_socket_communicate):
                        sockets.remove((_, client_socket_communicate))

                    filename = await self.get_old_filename(client_socket_communicate, filename)

                    message = f'{commands[1]}:{offset}:{chunk_size}:{filename}'

                    bytes_sent = await self.send_message(client_socket_communicate, message)
                    if bytes_sent < 0:
                        continue

                    buffer = await self.receive_bytes(client_socket_communicate, chunk_size)
                    if not buffer:
                        continue

                    if len(buffer) == len(command_error) and buffer.decode('utf-8') == command_error:
                        return -2

                    bytes_sent = await self.send_bytes(sock, buffer)
                    if bytes_sent < 0:
                        continue

                    total_bytes += bytes_sent

            return total_bytes
        except Exception as e:
            logger.error(f'Error: {e}')
            return -1

    def find_socket(self, filename: str) -> list[tuple[socket, socket]] | None:
        try:
            return [(key[0], key[1]) for key, values in self.storage.items() for value in values if
                    value.current_filename == filename]
        except Exception as e:
            logger.error(e)
            return None

    def get_list_files(self) -> List[str]:
        try:
            filename_counts = defaultdict(int)
            unique_filenames = []
            duplicate_filenames = []

            for values in self.storage.values():
                for value in values:
                    filename_counts[value.current_filename] += 1

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
                    if file_info.current_filename == filename:
                        return file_info.size
            return -1
        except Exception as e:
            logger.error(e)
            return -1

    def index_files(self) -> None:
        filename_hash_map = defaultdict(set)

        for item in self.storage.values():
            for file_infos in item:
                if file_infos.is_filename_modify:
                    file_infos.current_filename = self.remove_index(file_infos.current_filename)
                    file_infos.is_filename_modify = False

        storage_items = [file_info for files in self.storage.values() for file_info in files]

        for i, first in enumerate(storage_items):
            for second in storage_items[i + 1:]:
                if first.hash == second.hash and first.current_filename != second.current_filename:
                    second.current_filename = first.current_filename
                    second.is_filename_changed = True

        for files in self.storage.values():
            for file_info in files:
                filename_hash_map[file_info.current_filename].add(file_info.hash)

        for filename, hashes in filename_hash_map.items():
            file_hash_size = len(hashes)

            if file_hash_size > 1:
                for hash_value in hashes:
                    for files in self.storage.values():
                        for file_info in files:
                            if file_info.current_filename == filename and file_info.hash == hash_value:
                                file_info.current_filename += f'({file_hash_size})'
                                file_info.is_filename_modify = True
                    file_hash_size -= 1

    def store_files(self, pair: Tuple[socket, socket], filename: str, size: int, hash_val: str) -> None:
        try:
            data = FileInfo(size, hash_val, filename)
            self.storage[pair].append(data)
        except Exception as e:
            logger.error(e)

    def remove_clients(self, pair: Tuple[socket, socket]) -> None:
        try:
            keys_to_remove = []

            for key, values in self.storage.items():
                if key == pair:
                    keys_to_remove.append(key)
                    filename = self.remove_index(values[0].current_filename)

                    for entry_value in self.storage[key]:
                        if entry_value.current_filename.startswith(filename):
                            entry_value.current_filename = self.remove_index(entry_value.current_filename)

            for key in keys_to_remove:
                del self.storage[key]

            self.index_files()
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
                    if value.current_filename == filename:
                        return value.is_filename_modify
            return False
        except Exception as e:
            logger.error(e)
            return False

    def is_filename_change(self, sock: socket, filename: str) -> bool:
        try:
            for key, values in self.storage.items():
                if key[0] == sock or key[1] == sock:
                    for value in values:
                        if value.current_filename == filename:
                            return value.is_filename_changed
            return False
        except Exception as e:
            logger.error(e)
            return False

    @staticmethod
    def check_connection(client_socket_communicate: socket) -> bool:
        try:
            client_socket_communicate.getpeername()
            return True
        except OSError:
            return False
        except Exception as e:
            logger.error(e)
            return False

    def is_file_exist_on_client(self, sock: socket, filename: str) -> bool:
        for (sock1, sock2), files in self.storage.items():
            if any(file_info.current_filename == filename and (sock1 == sock or sock2 == sock) for file_info in files):
                return True
        return False

    async def get_old_filename(self, sock: socket, filename: str) -> str:
        for (sock1, sock2), files in self.storage.items():
            if sock1.fileno() is sock.fileno() or sock2.fileno() is sock.fileno():
                for file_info in files:
                    if file_info.current_filename == filename:
                        return file_info.old_filename
        return filename


async def main():
    connection = Connection()
    await connection.wait_connection()


if __name__ == "__main__":
    loop = asyncio.get_event_loop()
    loop.run_until_complete(main())
