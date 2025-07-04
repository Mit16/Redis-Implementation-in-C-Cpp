# Redis-like In-Memory Data Store (C++)

This project is a custom Redis-like server built from scratch in C++. It supports a subset of Redis commands including strings, sorted sets (`ZSET`), key expiration (TTL), and more. The server uses non-blocking I/O with `poll()`, a custom heap for TTL management, and a thread pool for efficient cleanup of large data structures.

## 🛠 Features

- ✅ String operations: `SET`, `GET`, `DEL`
- ✅ Sorted set operations: `ZADD`, `ZREM`, `ZSCORE`, `ZQUERY`
- ✅ Key expiration support: `PEXPIRE`, `PTTL`
- ✅ Time-based cleanup with a custom heap
- ✅ Thread pool for background cleanup of large datasets
- ✅ Per-connection database isolation
- ✅ Binary protocol (custom wire format)
- ✅ Idle connection cleanup and non-blocking I/O via `poll()`

---

## 🚀 Build Instructions

### Requirements

- Linux or WSL
- `g++` with C++23 support
- `make`

### Build

```bash
make all        # Builds server and debug client
make client   # Alias for building debug client
make test     # Builds test_offset.cpp
make testpy   # Runs Python tests using production client
```

## Run
bash
```
make run_server        # Runs the server
make run_client        # Runs the debug client
make run_client_prod   # Runs the production build client
make run_test          # Runs test_offset
```

### To clean up build artifacts:

bash
```
make clean
```

## 📦 Binary Protocol
The protocol is a custom binary format that supports typed responses:
```
TAG_INT: 64-bit integer
TAG_STR: string with length
TAG_DBL: 64-bit float
TAG_NIL: null
TAG_ERR: error with code and message
TAG_ARR: array
```

## 🧪 Testing

The project includes a test suite:

bash
```
make testpy
```
⚙️ This will automatically build the production version of the client (with optimizations) and run test_cmds.py.<br>
🔍 Note: Some tests assume a shared keyspace across clients.<br>
Since this server uses per-client isolation, those tests will fail by design.

## 🔒 Client DB Isolation
Each client connection has its own isolated keyspace (conn->db).
This means:
Keys created or modified by one client are not visible to another.
TTLs and ZSETs are scoped per connection.
This is useful for sandboxed or multi-tenant scenarios.

📌 If you need a global Redis-like keyspace, modify the code to use ```g_data.db``` instead of ```conn->db```.

## 📁 Project Structure
bash
```
.
├── server.cpp         # Main server logic
├── client.cpp         # Client interface (debug/prod)
├── test_offset.cpp    # Offset-based testing client
├── hashtable.cpp/.h   # Custom hashtable
├── zset.cpp/.h        # Sorted set implementation
├── heap.cpp/.h        # TTL heap management
├── avl.cpp/.h         # AVL tree for ZSET indexing
├── list.h             # Doubly linked list
├── thread_pool.cpp/.h # Thread pool for async deletions
├── Makefile           # Build system
├── test_cmds.py       # Python test runner

```

## ✅ Example Commands

bash
```
./client set key1 value1
./client get key1
./client zadd zset 1.5 member1
./client zscore zset member1
./client pexpire key1 1000
```

## Interactive Debug Mode
For exploring commands manually, the debug client is built by default with:

bash
```
make all
```
This builds the server and the debug version of the client as ./client.

### Run it interactively:

bash
```
./client
```

You can now enter Redis-like commands:
redis
```
set key1 hello
get key1
zadd zset 1.1 name1
zquery zset 1 "" 0 10
```
💡 Debug builds include extra logging and are ideal for development and debugging.

## 🧑‍🤝‍🧑 Multiple Clients
The server supports multiple concurrent clients.

You can open multiple terminals and connect using:
bash
```
./client
```

If you build the production version of the client (for testing), use:
bash
```
make client_prod
./client
```
🧠 Note: The client binary is reused for both debug and prod builds — the last one built takes precedence.

Each connected client operates independently, and if DB isolation is enabled, maintains its own database context.

## 📌 Future Improvements
 Shared keyspace mode toggle
 Snapshot-based persistence
 More Redis commands (e.g., INCR, MGET)
 Pub/Sub support

## 👨‍💻 Author
**Amitkumar Vishwakarma**  
🔗 [GitHub Profile](https://github.com/Mit16)

## 📚 Inspiration
This project was inspired by the [Build Your Own Redis](https://build-your-own.org/redis/) guide.

## 📝 License
This project is released under the MIT License.
