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
        self.socket_listen = None
        self.socket_communicate = None

    def __del__(self):
        if self.socket_listen:
            self.socket_listen.close()
        if self.socket_communicate:
            self.socket_communicate.close()

    async def wait_connection(self):
        pass

    @staticmethod
    def is_connect(client_socket_listen: socket, client_socket_communicate: socket):
        pass

    @staticmethod
    def check_connection(socket: socket):
        pass

    async def handle_clients(self, client_socket_listen: socket, client_socket_communicate: socket):
        pass

    async def send_message(self, socket: socket, message: str, flags: int):
        pass

    async def receive_message(self, socket: socket, message: str, flags: int):
        pass

    async def send_bytes(self, socket: socket, buffer: List, size: int, flags: int):
        pass

    async def receive_bytes(self, socket: socket, buffer: List, size: int, flags: int):
        pass

    @staticmethod
    def process_response(message: str):
        pass

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
