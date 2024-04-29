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
        self.storage: Dict[Tuple[int, int], FileInfo]
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
            client_socket_communicate, client_addr_communicate = await self.loop.sock_accept(socket_communicate)
            client_socket_listen, client_addr_listen = await self.loop.sock_accept(socket_listen)

            print(f'[+] Success: Client connected: {client_addr_listen[0]}:{client_addr_listen[1]},'
                  f' {client_addr_communicate[0]}:{client_addr_communicate[1]}.')

            await asyncio.gather(self.synchronization(client_socket_listen, client_socket_communicate))
            await asyncio.create_task(self.handle_clients(client_socket_listen, client_socket_communicate))

    async def handle_clients(self, client_socket_listen: socket.socket, client_socket_communicate: socket.socket):
        while True:
            command = await self.receive_message(client_socket_listen)

            if not command:
                break

            if command == commands[0]:
                self.send_list(client_socket_listen)
            elif command.startswith(commands[1]):
                _, filename = command.split(":")
                self.send_file(client_socket, filename)
            elif command == commands[2]:
                break

        client_socket.close()
        print("[+] Success: Client disconnected.")

    @staticmethod
    def is_connect(client_socket_listen: socket, client_socket_communicate: socket):
        pass

    @staticmethod
    def check_connection(socket: socket):
        pass

    async def handle_clients(self, client_socket_listen: socket, client_socket_communicate: socket):
        pass

    async def sendMessage(self, socket, message, flags):
        bytes = await self.sendBytes(socket, message.encode(), len(message), flags)
        return bytes

    async def receiveMessage(self, socket, flags):
        bytes = await self.receiveBytes(socket, flags)
        if bytes > 0:
            return bytes.decode()
        else:
            return None

    async def sendBytes(self, socket, buffer, size, flags):
        try:
            writer = asyncio.StreamWriter(socket, None, None, loop=asyncio.get_running_loop())
            writer.write(buffer)
            await writer.drain()
            return size
        except Exception as e:
            return -1

    async def receiveBytes(self, socket, flags) -> bytes | int:
        try:
            reader = asyncio.StreamReader(loop=asyncio.get_running_loop())
            asyncio.StreamReaderProtocol(reader)
            await asyncio.wait_for(asyncio.start_server(lambda r, w: w.close(), port=None), timeout=None)
            reader.set_transport(socket)
            data = await reader.read(BUFFER_SIZE)
            return data
        except Exception:
            return -1

    @staticmethod
    async def processResponse(self, message: str) -> bool | int:
        try:
            pos = message.find(':')
            if pos == -1:
                return False
            size = int(message[:pos])
            message = message[pos + 1:]
            return size
        except Exception:
            return False

    async def synchronization(self, client_socket_listen: socket, client_socket_communicate: socket):
        pass

    async def send_list(self, socket: socket):
        pass

    async def send_file(self, socket: socket, filename: str):
        pass

    def find_socket(self, filename: str):
        pass

    def get_list_files(self):
        pass

    def get_size(self, filename: str):
        pass

    def update_storage(self):
        pass

    def store_files(self, pair: List[int, int], filename: str, size: int, hash: str):
        pass

    def remove_clients(self, pair: Tuple[int, int]):
        pass

    @staticmethod
    def split(string: str, delim: str, tokens: List[str]):
        pass

    @staticmethod
    def remove_index(filename: str):
        pass

    def is_filename_modify(self, filename: str):
        pass

    def is_filename_change(self, socket: socket, filename: str):
        pass


if __name__ == "__main__":
    connection = Connection()
    connection.start_server()
