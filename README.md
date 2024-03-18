# File sharing network

## Description

Clients connect to the server via TCP, after connecting,
they automatically send to the server a fileMap of names
of shared files (stored in the same folder), as well as hashes for
the contents of these files. If the contents of the files match,
their hashes match, and the server stores a fileMap of unique files on the
network.

The client can request a fileMap of files shared on the network
and start downloading any of them through the server. If
the same downloaded file is located on 2 or more clients,
the download is carried out simultaneously from all clients, alternating blocks by
1kb. Thus, each client gives the file to the network only partially.
Commands in the user console: fileMap, get_file file name, exit, help.
