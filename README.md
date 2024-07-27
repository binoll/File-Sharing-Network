# File sharing network

## Overview

A client-server application for transferring 
files over a network using the TCP protocol. 

Commands in the user console: **list** -
list of shared files, **get filename** - upload the file, **exit** - exit the application,
**help** - output possible commands.

## Realization

Clients connect to the server over TCP, after connecting
, they automatically send to the server a repository of names
of shared files (stored in the same folder), as well as hashes for
the contents of these files. If the contents of the files match,
their hashes match, and the server stores a repository of unique files on the
network.

The client can request a repository of files shared on the network
and start downloading any of them through the server. If
the same downloaded file is on 2 or more clients,
the download is performed simultaneously from all clients, alternating blocks by
1 KB. Thus, each client transfers the file to the network only partially.

## Dependencies
```bash
sudo apt install libboost-all-dev libssl-dev -y 
```

## Build
From the root of the repository:

```bash
mkdir build && cd build
```

```bash
cmake ..
```

```bash
cmake --build .
```

## Run

To start the client:

```bash
./build/client/client
```

To start the multithreaded server:

```bash
./build/multithreaded_server/multithreaded_server
```

To start the asynchronous server:

```bash
python3 ../asynchronous_server/asynchronous_server.py
```
