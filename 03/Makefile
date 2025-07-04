# Compiler and flags
CXX = g++
DEBUG_FLAGS = -std=c++23 -Wall -Wextra -g -lpthread -DDEBUG
PROD_FLAGS  = -std=c++23 -Wall -Wextra -O2 -lpthread

# Source files
SERVER_SRC = server.cpp avl.cpp hashtable.cpp zset.cpp heap.cpp thread_pool.cpp
CLIENT_SRC = client.cpp
TEST_SRC   = test_offset.cpp

# Executables
SERVER_BIN = server
CLIENT_BIN = client
CLIENT_DEBUG_BIN = client-debug
TEST_BIN = test_offset

# Default target
all: $(SERVER_BIN) $(CLIENT_BIN)

# Server build
$(SERVER_BIN): $(SERVER_SRC)
	@echo "🔧 Building server..."
	$(CXX) $(DEBUG_FLAGS) -o $@ $^

# Production client (default)
$(CLIENT_BIN): $(CLIENT_SRC)
	@echo "🔧 Building client (prod)..."
	$(CXX) $(PROD_FLAGS) -o $@ $^

# Debug build of client (named separately)
$(CLIENT_DEBUG_BIN): $(CLIENT_SRC)
	@echo "🐞 Building client (debug)..."
	$(CXX) $(DEBUG_FLAGS) -o $@ $^

# Test binary
$(TEST_BIN): $(TEST_SRC)
	@echo "🔧 Building test_offset..."
	$(CXX) $(DEBUG_FLAGS) -o $@ $^

# Test target
test: $(TEST_BIN)

# Python test runner
testpy: $(CLIENT_BIN)
	@echo "🐍 Running Python test_cmds.py with production client..."
	python3 test_cmds.py

# Clean up
clean:
	@echo "🧹 Cleaning up..."
	rm -f $(SERVER_BIN) $(CLIENT_BIN) $(CLIENT_DEBUG_BIN) $(TEST_BIN)

# Run targets
run_server: $(SERVER_BIN)
	@echo "🚀 Running server..."
	./$<

run_client: $(CLIENT_BIN)
	@echo "🛰️  Running client (prod)..."
	./$<

run_client_debug: $(CLIENT_DEBUG_BIN)
	@echo "🐞 Running client (debug)..."
	./$<

run_test: test
	@echo "🧪 Running test_offset..."
	./$(TEST_BIN)
