import asyncio
import socket
import struct
import os
from typing import Tuple, List, Dict

BUFFER_SIZE = 1024
BACKLOG = 100
commands = ["list", "get", "exit", "error", "exist"]


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
        self.storage = Dict[Tuple[int, int], FileInfo]
        self.addr_listen = ('0.0.0.0', 0)
        self.addr_communicate = ('0.0.0.0', 0)
        self.loop = asyncio.get_event_loop()

    async def wait_connection(self):
        await asyncio.gather(
            self.listen(self.addr_listen, self.addr_communicate)
        )

    async def listen(self, addr_listen: Tuple[str, int], addr_communicate: Tuple[str, int]):
        socket_communicate = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        socket_communicate.bind(addr_communicate)
        socket_communicate.listen(BACKLOG)

        socket_listen = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        socket_listen.bind(addr_listen)
        socket_listen.listen(BACKLOG)

        print(f'[*] Server is listening on ports: {addr_communicate[1]}, {addr_listen[1]}.')

        while True:
            client_socket_communicate, client_addr_communicate = self.loop.sock_accept(socket_communicate)
            client_socket_listen, client_addr_listen = self.loop.sock_accept(socket_listen)

            print(f'[+] Success: Client connected: {client_addr_listen[0]}:{client_addr_listen[1]},'
                  f' {client_addr_communicate[0]}:{client_addr_communicate[1]}.')

            await asyncio.gather(self.synchronization(client_socket_listen, client_socket_communicate))
            await asyncio.create_task(self.handle_clients(client_socket_listen, client_socket_communicate))

    async def handle_clients(self, client_socket_listen: socket.socket, client_socket_communicate: socket.socket):
        while Connection.is_connect(client_socket_listen, client_socket_communicate):
            command = await self.receive_message(client_socket_listen)

            if not command:
                break

            if command == commands[0]:
                await self.send_list(client_socket_listen)
            elif command.startswith(commands[1]):
                filename = command.split(":")[1]
                await self.send_file(client_socket_listen, filename)
            elif command == commands[2]:
                break

        client_socket_listen.close()
        print('[+] Success: Client disconnected.')

    @staticmethod
    def is_connect(client_socket_listen: socket, client_socket_communicate: socket):
        return (Connection.check_connection(client_socket_listen) and
                Connection.check_connection(client_socket_communicate))

    @staticmethod
    def check_connection(socket: socket) -> bool:
        return socket.fileno() != -1

    async def send_message(self, socket: socket, message: str) -> None:
        await self.send_bytes(socket, bytearray(message.encode()))

    async def receive_message(self, socket: socket) -> str | None:
        buffer = bytearray()
        bytes_received = await self.receive_bytes(socket, buffer)

        if bytes_received > 0:
            return str(buffer.decode())
        else:
            return None

    async def send_bytes(self, socket: socket, buffer: bytearray) -> None:
        await self.loop.sock_sendall(socket, buffer)

    async def receive_bytes(self, socket: socket, buffer: bytearray) -> int:
        bytes_received = await self.loop.sock_recv_into(socket, buffer)
        return bytes_received

    @staticmethod
    async def process_response(message: str) -> int | None:
        try:
            pos = message.find(':')
            if pos == -1:
                return None
            size = int(message[:pos])
            message = message[pos + 1:]
            return size
        except Exception:
            return None

    async def synchronization(self, client_socket_listen: socket, client_socket_communicate: socket) -> bool:
        bytes_received = await self.receive_message(client_socket_listen)
        command_error = commands[3]

        if bytes_received < 0:
            return False

        message = str(bytes_received.decode())
        if message == command_error:
            return False

        message_size = await self.process_response(message)
        if message_size < 0:
            return False

        while message_size > 0:
            bytes_received = await self.receive_message(client_socket_listen)
            if bytes_received < 0:
                return False

            message = str(bytes_received.decode())
            if message == command_error:
                return False

            try:
                for file_info in message.split():
                    filename, file_size_str, file_hash = file_info.split(':')
                    file_size = int(file_size_str)
                    self.store_files((client_socket_listen, client_socket_communicate), filename, file_size,
                                     file_hash)
            except ValueError as err:
                print("[-] Error:", err)
                return False
            except Exception as err:
                print("[-] Error:", err)
                return False

            message_size -= len(message)

        await self.update_storage()
        return True

    async def send_list(self, socket: socket.socket) -> int:
        files = self.get_list_files()
        list_str = ' '.join(files)
        message_size = str(len(list_str)) + ':'

        try:
            await self.send_message(socket, message_size + list_str)
            return len(message_size)
        except Exception as e:
            print("[-] Error:", e)
            return -1

    async def send_file(self, socket: socket.socket, filename: str) -> int:
        total_bytes = 0
        message_size = self.get_size(filename)
        real_filename = self.remove_index(filename)
        command_error = commands[3]
        command_exist = commands[4]

        if self.is_filename_modify(filename):
            real_filename = self.remove_index(filename)
        else:
            real_filename = filename

        if self.is_filename_change(socket, filename):
            await self.send_message(socket, command_exist)
            return 0

        message = f"{message_size}:"
        try:
            await self.send_message(socket, message)
        except Exception as e:
            print("[-] Error:", e)
            return -1

        for offset in range(0, message_size, BUFFER_SIZE):
            chunk_size = min(message_size - offset, BUFFER_SIZE)
            client_socket_communicate = self.find_socket(filename)[0][1]
            timeout = 10

            message = f"{commands[1]}:{offset}:{chunk_size}:{real_filename}"
            if not self.check_connection(client_socket_communicate):
                return -1

            try:
                await self.send_message(client_socket_communicate, message)
                await asyncio.wait_for(self.receive_bytes(client_socket_communicate, chunk_size), timeout)
                bytes_received = await self.receive_bytes(client_socket_communicate, chunk_size)
            except asyncio.TimeoutError:
                continue
            except Exception as e:
                print("[-] Error:", e)
                continue

            if bytes_received < 0:
                continue

            if bytes_received == command_error:
                return -2

            try:
                await self.send_bytes(socket, bytes_received)
                total_bytes += bytes_received
            except Exception as e:
                print("[-] Error:", e)
                continue

        return total_bytes

    def find_socket(self, filename: str) -> List[Tuple[int, int]]:
        result = []
        for key, value in self.storage.items():
            if value['filename'] == filename:
                result.append(key)
        return result

    def get_list_files(self) -> List[str]:
        filename_counts = {}
        unique_filenames = []
        duplicate_filenames = []
        for value in self.storage.values():
            filename_counts[value['filename']] = filename_counts.get(value['filename'], 0) + 1

        for filename, count in filename_counts.items():
            if count == 1:
                unique_filenames.append(filename)
            else:
                duplicate_filenames.append(filename)

        unique_filenames.extend(duplicate_filenames)
        return unique_filenames

    def get_size(self, filename: str) -> int:
        for value in self.storage.values():
            if value['filename'] == filename:
                return value['size']
        return -1

    def update_storage(self):
        hash_count = {}
        filename_count = {}
        for value in self.storage.values():
            hash_count[value['hash']] = hash_count.get(value['hash'], 0) + 1

        for first_key, first_value in self.storage.items():
            for second_key, second_value in self.storage.items():
                file_occurrences = hash_count[first_value['hash']]
                if first_value['hash'] != second_value['hash'] and first_value['filename'] == second_value[
                    'filename']:
                    if file_occurrences > 1:
                        first_value['filename'] += '(' + str(file_occurrences - 1) + ')'
                        second_value['filename'] += '(' + str(file_occurrences) + ')'
                        first_value['is_filename_modify'] = True
                        second_value['is_filename_modify'] = True
                        hash_count[first_value['hash']] -= 1
                        hash_count[second_value['hash']] -= 1
                elif first_value['hash'] == second_value['hash'] and first_value['filename'] != second_value[
                    'filename']:
                    second_value['filename'] = first_value['filename']
                    second_value['is_filename_changed'] = True

    def store_files(self, pair: Tuple[int, int], filename: str, size: int, hash_val: str):
        data = {'size': size, 'hash': hash_val, 'filename': filename, 'is_filename_modify': False,
                'is_filename_changed': False}
        self.storage[pair] = data
        print("[+] Success: Stored the file:", filename)

    def remove_clients(self, pair: Tuple[int, int]):
        for key, value in list(self.storage.items()):
            if key == pair:
                filename = self.remove_index(value['filename'])
                for entry_key, entry_value in self.storage.items():
                    if entry_value['filename'].startswith(filename):
                        entry_value['filename'] = self.remove_index(entry_value['filename'])
                del self.storage[key]
        self.update_storage()

    @staticmethod
    def split(string: str, delim: str, tokens: List[str]):
        tokens.extend(string.split(delim))

    @staticmethod
    def remove_index(filename: str) -> str:
        pos = filename.rfind('(')
        if pos != -1:
            filename = filename[:pos]
        return filename

    def is_filename_modify(self, filename: str) -> bool:
        for value in self.storage.values():
            if value['filename'] == filename:
                return value['is_filename_modify']
        return False

    def is_filename_change(self, socket: Tuple[int, int], filename: str) -> bool:
        for key, value in self.storage.items():
            if value['filename'] == filename and socket[0] in key or socket[1] in key:
                return value['is_filename_changed']
        return False


def main():
    connection = Connection()
    connection.wait_connection()


if __name__ == "__main__":
    main()
