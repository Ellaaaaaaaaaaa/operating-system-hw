#!/bin/bash

# --- Configuration ---
SERVER_IP="127.0.0.1" # Loopback address for local testing
SERVER_PORT=8080
SOURCE_DIR="test_source"
SYNC_ROOT_DIR="sync_root" # Server's sync root directory
SERVER_EXEC="./server"
CLIENT_EXEC="./client"

# --- Functions ---

# Function to clean up previous test runs
cleanup() {
    echo "Cleaning up previous test environment..."
    killall server 2>/dev/null # Kill any running server instances
    rm -rf "$SOURCE_DIR"
    rm -rf "$SYNC_ROOT_DIR"
    echo "Cleanup complete."
}

# Function to start the server
start_server() {
    echo "Starting server..."
    # Run server in background, redirect output to a log file
    "$SERVER_EXEC" > server.log 2>&1 &
    SERVER_PID=$!
    echo "Server started with PID: $SERVER_PID"
    sleep 2 # Give server time to start
}

# Function to create test data
create_test_data() {
    echo "Creating test data in $SOURCE_DIR..."
    mkdir -p "$SOURCE_DIR/dir1" "$SOURCE_DIR/dir2" "$SOURCE_DIR/dir1/subdir"

    echo "This is file1 in root." > "$SOURCE_DIR/file1.txt"
    echo "Content of file2 in dir1." > "$SOURCE_DIR/dir1/file2.txt"
    echo "Another file in dir1/subdir." > "$SOURCE_DIR/dir1/subdir/file3.log"
    dd if=/dev/urandom of="$SOURCE_DIR/large_file.bin" bs=1M count=5 2>/dev/null # 5MB binary file

    echo "Test data created."
}

# Function to verify sync
verify_sync() {
    echo "Verifying synchronization..."
    
    # Expected synced path on server
    # The client uses the last component of SOURCE_DIR as the remote_base_path
    REMOTE_SYNC_PATH="$SYNC_ROOT_DIR/$(basename "$SOURCE_DIR")"

    if [ ! -d "$REMOTE_SYNC_PATH" ]; then
        echo "Error: Remote sync directory '$REMOTE_SYNC_PATH' not found on server."
        return 1
    fi

    # Use diff -r to compare source and synced directories
    # -r: recursive
    # -q: quiet, only report when files differ
    # --no-dereference: don't follow symlinks
    diff_output=$(diff -r -q "$SOURCE_DIR" "$REMOTE_SYNC_PATH")

    if [ -z "$diff_output" ]; then
        echo "Verification SUCCESS: All files and directories are identical."
        return 0
    else
        echo "Verification FAILED: Differences found."
        echo "$diff_output"
        return 1
    fi
}

# --- Main Test Script ---

# 1. Cleanup
cleanup

# 2. Start server
start_server

# 3. Create test data
create_test_data

# 4. Run client
echo "Running client to sync '$SOURCE_DIR' to server..."
"$CLIENT_EXEC" "$SERVER_IP" "$SOURCE_DIR"
CLIENT_EXIT_CODE=$?

if [ $CLIENT_EXIT_CODE -ne 0 ]; then
    echo "Client exited with error code $CLIENT_EXIT_CODE"
    kill "$SERVER_PID"
    exit 1
fi

sleep 1 # Give server a moment to finish processing

# 5. Verify sync
verify_sync
TEST_RESULT=$?

# 6. Kill server
echo "Stopping server (PID: $SERVER_PID)..."
kill "$SERVER_PID"
wait "$SERVER_PID" 2>/dev/null # Wait for server to terminate gracefully

if [ $TEST_RESULT -eq 0 ]; then
    echo "==================================="
    echo "  TEST SUITE PASSED SUCCESSFULLY!  "
    echo "==================================="
else
    echo "==================================="
    echo "      TEST SUITE FAILED!           "
    echo "==================================="
fi

# Optional: keep files for inspection on failure
# if [ $TEST_RESULT -ne 0 ]; then
#     echo "Keeping test files for inspection due to failure."
# else
#     cleanup # Clean up everything on success
# fi

exit $TEST_RESULT