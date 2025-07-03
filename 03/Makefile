# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -g -lpthread

# Source files
SERVER_SRC = server.cpp avl.cpp hashtable.cpp zset.cpp heap.cpp thread_pool.cpp
CLIENT_SRC = client.cpp
TEST_SRC = test_offset.cpp

# Executables
SERVER_BIN = server
CLIENT_BIN = client
TEST_BIN = test_offset

# Default target
all: $(SERVER_BIN) $(CLIENT_BIN)

# Server build
$(SERVER_BIN): $(SERVER_SRC)
	@echo "🔧 Building server..."
	$(CXX) $(CXXFLAGS) -o $(SERVER_BIN) $(SERVER_SRC)

# Client build
$(CLIENT_BIN): $(CLIENT_SRC)
	@echo "🔧 Building client..."
	$(CXX) $(CXXFLAGS) -o $(CLIENT_BIN) $(CLIENT_SRC)

# Test build
test: $(TEST_BIN)

$(TEST_BIN): $(TEST_SRC)
	@echo "🔧 Building test_offset..."
	$(CXX) $(CXXFLAGS) -o $(TEST_BIN) $(TEST_SRC)

# Python test
testpy:
	@echo "🐍 Running Python test_cmds.py..."
	python3 test_cmds.py

# Clean build files
clean:
	@echo "🧹 Cleaning up..."
	rm -f $(SERVER_BIN) $(CLIENT_BIN) $(TEST_BIN)

# Run targets
run_server: $(SERVER_BIN)
	@echo "🚀 Running server..."
	./$(SERVER_BIN)

run_client: $(CLIENT_BIN)
	@echo "🛰️  Running client..."
	./$(CLIENT_BIN)

run_test: test
	@echo "🧪 Running test_offset..."
	./$(TEST_BIN)
