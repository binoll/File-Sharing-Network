# File sharing network

## Overview
# Task №1 (P2P-файлообменная сеть):
Клиенты подключаются к серверу по протоколу TCP, после подключения
они автоматически отправляют на сервер хранилище имен
общих файлов (хранящихся в той же папке), а также хэши для
содержимого этих файлов. Если содержимое файлов совпадает,
их хэши совпадают, и сервер сохраняет хранилище уникальных файлов в
сети.

Клиент может запросить хранилище файлов, совместно используемых в сети
, и начать загрузку любого из них через сервер. Если
один и тот же загруженный файл находится на 2 или более клиентах,
загрузка осуществляется одновременно со всех клиентов, чередуя блоки по
1 кбайт. Таким образом, каждый клиент передает файл в сеть только частично. 

Команды в пользовательской консоли: **list** -
список общих файлов, **get "filename"** - загрузить файл, **exit** - выйти из приложения,
**help** - вывести возможные команды.

# Task №2 (MITM коммутатор):

MITM коммутатор модифицирует указатель срочности в TCP сегменте.

## Dependencies
```bash
sudo apt install libboost-all-dev libpcap-dev -y 
```

## Build
From the root of the repository:

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

## Run
To start the multithreaded server:

```bash
./build/multithreaded_server/multithreaded_server
```

To start the asynchronous server:

```bash
python3 ../asynchronous_server/asynchronous_server.py
```

To start the client:

```bash
./build/client/client
```