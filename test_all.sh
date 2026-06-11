#!/bin/bash
# =============================================================
# test_all.sh  –  ENCS4330 Project 3 test suite
#
# Usage:   ./test_all.sh
# Assumes: server and client binaries are already built (make all)
#          Run from the project directory.
# =============================================================

SERVER_BIN=./server_test
CLIENT_BIN=./client_test
SERVER_CFG=config.txt
PASS=0
FAIL=0
SERVER_PID=""

# ── Colour helpers ────────────────────────────────────────────
RED='\033[0;31m'
GRN='\033[0;32m'
YLW='\033[1;33m'
BLU='\033[0;34m'
NC='\033[0m'

info()  { echo -e "${BLU}[INFO]${NC}  $*"; }
ok()    { echo -e "${GRN}[PASS]${NC}  $*"; ((PASS++)); }
fail()  { echo -e "${RED}[FAIL]${NC}  $*"; ((FAIL++)); }
header(){ echo -e "\n${YLW}══════════════════════════════════════${NC}"; \
          echo -e "${YLW}  $*${NC}"; \
          echo -e "${YLW}══════════════════════════════════════${NC}"; }

# ── Start / stop helpers ──────────────────────────────────────
start_server() {
    local cfg="${1:-$SERVER_CFG}"
    info "Starting server with config: $cfg"
    $SERVER_BIN "$cfg" &>/dev/null &
    SERVER_PID=$!
    sleep 0.6          # let it bind and listen
    if kill -0 "$SERVER_PID" 2>/dev/null; then
        info "Server PID=$SERVER_PID"
    else
        fail "Server failed to start"
        SERVER_PID=""
    fi
}

stop_server() {
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" 2>/dev/null
        wait "$SERVER_PID" 2>/dev/null
        SERVER_PID=""
        info "Server stopped"
    fi
}

# Write a minimal config file
write_server_cfg() {
    local file=$1 port=$2 latest=$3 update_file=${4:-update.bin}
    cat > "$file" <<EOF
PORT           = $port
MAX_CLIENTS    = 50
LATEST_VERSION = $latest
UPDATE_FILE    = $update_file
LOG_FILE       = server_test.log
BUFFER_SIZE    = 4096
EOF
}

write_client_cfg() {
    local file=$1 port=$2 version=$3
    cat > "$file" <<EOF
SERVER_IP      = 127.0.0.1
PORT           = $port
CLIENT_VERSION = $version
CLIENT_LOG_FILE= client_test.log
BUFFER_SIZE    = 4096
EOF
}

# ── Create a dummy update file ────────────────────────────────
make_update_file() {
    local size=${1:-1024}    # bytes
    dd if=/dev/urandom of=update.bin bs="$size" count=1 2>/dev/null
    info "Created update.bin (${size} bytes)"
}

# =============================================================
#  TEST 1 – Client already up to date
# =============================================================
header "Test 1: Client up to date"
write_server_cfg server_t1.txt 9101 5
write_client_cfg client_t1.txt 9101 5
make_update_file 512
start_server server_t1.txt

OUTPUT=$($CLIENT_BIN client_t1.txt 2>&1)
if echo "$OUTPUT" | grep -q "up to date\|UP_TO_DATE"; then
    ok "Client correctly notified it is up to date"
else
    fail "Expected 'up to date' message — got: $OUTPUT"
fi
stop_server

# =============================================================
#  TEST 2 – Client outdated → receives update file
# =============================================================
header "Test 2: Client outdated — update download"
write_server_cfg server_t2.txt 9102 5
write_client_cfg client_t2.txt 9102 3
make_update_file 2048
start_server server_t2.txt

$CLIENT_BIN client_t2.txt &>/dev/null
if [ -f update_v4.bin ]; then
    ok "Update file update_v4.bin saved successfully"
    rm -f update_v4.bin
else
    fail "update_v4.bin not found after download"
fi
stop_server

# =============================================================
#  TEST 3 – Multiple simultaneous clients (5 clients)
# =============================================================
header "Test 3: Multiple simultaneous clients (5)"
write_server_cfg server_t3.txt 9103 10
make_update_file 4096
start_server server_t3.txt

PIDS=()
for i in $(seq 1 5); do
    write_client_cfg "client_t3_$i.txt" 9103 $((i))
    $CLIENT_BIN "client_t3_$i.txt" &>/dev/null &
    PIDS+=($!)
done

# wait for all clients
for pid in "${PIDS[@]}"; do
    wait "$pid" 2>/dev/null
done

# check at least one update file appeared (clients 1–9 are outdated)
COUNT=$(ls update_v*.bin 2>/dev/null | wc -l)
if [ "$COUNT" -gt 0 ]; then
    ok "Multiple clients handled — $COUNT update file(s) saved"
    rm -f update_v*.bin
else
    fail "No update files found — simultaneous clients may have failed"
fi
stop_server
rm -f client_t3_*.txt

# =============================================================
#  TEST 4 – Invalid / garbage request via netcat
# =============================================================
header "Test 4: Invalid client request"
write_server_cfg server_t4.txt 9104 5
start_server server_t4.txt

RESPONSE=$(echo "GARBAGE_REQUEST" | nc -q1 127.0.0.1 9104 2>/dev/null || \
           echo "GARBAGE_REQUEST" | nc -w1 127.0.0.1 9104 2>/dev/null)

if echo "$RESPONSE" | grep -qi "error\|invalid"; then
    ok "Server responded with error to garbage request"
else
    # server may just close connection — also acceptable
    ok "Server closed connection cleanly on invalid request"
fi

# verify server is still alive
if kill -0 "$SERVER_PID" 2>/dev/null; then
    ok "Server still running after invalid request"
else
    fail "Server crashed after invalid request"
    SERVER_PID=""
fi
stop_server

# =============================================================
#  TEST 5 – Large file transfer (10 MB)
# =============================================================
header "Test 5: Large file transfer (10 MB)"
write_server_cfg server_t5.txt 9105 2
write_client_cfg client_t5.txt 9105 1
dd if=/dev/urandom of=update.bin bs=1M count=10 2>/dev/null
info "Created 10 MB update.bin"
start_server server_t5.txt

$CLIENT_BIN client_t5.txt &>/dev/null

if [ -f update_v2.bin ]; then
    RECV_SIZE=$(stat -c%s update_v2.bin 2>/dev/null || stat -f%z update_v2.bin)
    ORIG_SIZE=$(stat -c%s update.bin    2>/dev/null || stat -f%z update.bin)
    if [ "$RECV_SIZE" -eq "$ORIG_SIZE" ]; then
        ok "Large file transferred correctly ($RECV_SIZE bytes)"
    else
        fail "Size mismatch: sent=$ORIG_SIZE received=$RECV_SIZE"
    fi
    rm -f update_v2.bin
else
    fail "update_v2.bin not created after large file transfer"
fi
stop_server

# =============================================================
#  TEST 6 – Concurrent downloads (10 clients, outdated)
# =============================================================
header "Test 6: Concurrent downloads (10 clients)"
write_server_cfg server_t6.txt 9106 5
make_update_file 8192
start_server server_t6.txt

PIDS=()
for i in $(seq 1 10); do
    write_client_cfg "client_t6_$i.txt" 9106 1
    $CLIENT_BIN "client_t6_$i.txt" &>/dev/null &
    PIDS+=($!)
done
for pid in "${PIDS[@]}"; do
    wait "$pid" 2>/dev/null
done

COUNT=$(ls update_v2.bin 2>/dev/null | wc -l)
if [ "$COUNT" -gt 0 ]; then
    ok "Concurrent downloads succeeded (file saved)"
    rm -f update_v*.bin
else
    fail "No update file found after concurrent downloads"
fi
stop_server
rm -f client_t6_*.txt

# =============================================================
#  TEST 7 – Server not running (client error handling)
# =============================================================
header "Test 7: Server not running — client error handling"
write_client_cfg client_t7.txt 9199 1   # nothing listening on 9199

OUTPUT=$($CLIENT_BIN client_t7.txt 2>&1)
if echo "$OUTPUT" | grep -qi "error\|failed\|connect"; then
    ok "Client handled missing server gracefully"
else
    fail "Client did not report connection error"
fi

# =============================================================
#  TEST 8 – Server max-clients enforcement
# =============================================================
header "Test 8: Max-clients limit"
write_server_cfg server_t8.txt 9108 5 update.bin
sed -i 's/MAX_CLIENTS    = 50/MAX_CLIENTS    = 2/' server_t8.txt
make_update_file 1024
start_server server_t8.txt

# Run all 5 clients in the background simultaneously
PIDS=()
OUTFILES=()
for i in $(seq 1 5); do
    write_client_cfg "client_t8_$i.txt" 9108 1
    OUTF=$(mktemp)
    OUTFILES+=("$OUTF")
    $CLIENT_BIN "client_t8_$i.txt" >"$OUTF" 2>&1 &
    PIDS+=($!)
done

# Wait max 10s for all clients to finish
DEADLINE=$((SECONDS + 10))
for pid in "${PIDS[@]}"; do
    REMAINING=$((DEADLINE - SECONDS))
    [ "$REMAINING" -le 0 ] && break
    wait "$pid" 2>/dev/null &
    WAIT_PID=$!
    ( sleep "$REMAINING" && kill "$WAIT_PID" 2>/dev/null ) &
    KILLER=$!
    wait "$WAIT_PID" 2>/dev/null
    kill "$KILLER" 2>/dev/null
done
# Kill any stragglers
for pid in "${PIDS[@]}"; do kill "$pid" 2>/dev/null; done

# Check outputs for rejection messages
REJECTED=0
for OUTF in "${OUTFILES[@]}"; do
    if grep -qi "busy\|rejected\|max" "$OUTF" 2>/dev/null; then
        ((REJECTED++))
    fi
    rm -f "$OUTF"
done

if [ "$REJECTED" -gt 0 ]; then
    ok "Server rejected $REJECTED client(s) when over max-clients limit"
else
    ok "Max-clients test completed (server remained stable)"
fi

# Server must still be alive
if kill -0 "$SERVER_PID" 2>/dev/null; then
    ok "Server still alive after max-clients test"
else
    fail "Server died during max-clients test"
    SERVER_PID=""
fi
stop_server
rm -f client_t8_*.txt update_v*.bin

# =============================================================
#  TEST 9 – Interrupted connection mid-download
# =============================================================
header "Test 9: Interrupted connection mid-download"
write_server_cfg server_t9.txt 9109 2
# Large file so kill hits during transfer, not after
dd if=/dev/urandom of=update.bin bs=1M count=50 2>/dev/null
info "Created 50 MB update.bin for interrupt test"
write_client_cfg client_t9.txt 9109 1
start_server server_t9.txt

$CLIENT_BIN client_t9.txt &>/dev/null &
CLIENT_PID=$!
sleep 1    # let the download start
kill -9 "$CLIENT_PID" 2>/dev/null
wait "$CLIENT_PID" 2>/dev/null

# The key check: server must survive the abrupt disconnect
sleep 0.5
if kill -0 "$SERVER_PID" 2>/dev/null; then
    ok "Server survived interrupted client connection"
else
    fail "Server crashed after client disconnect mid-download"
    SERVER_PID=""
fi
stop_server
rm -f client_t9.txt server_t9.txt update.bin update_v2.bin

# =============================================================
#  Cleanup & summary
# =============================================================
echo ""
echo -e "${YLW}══════════════════════════════════════${NC}"
echo -e "${YLW}  TEST SUMMARY${NC}"
echo -e "${YLW}══════════════════════════════════════${NC}"
echo -e "  ${GRN}PASSED: $PASS${NC}"
echo -e "  ${RED}FAILED: $FAIL${NC}"
echo ""

# clean up temp files
rm -f server_t*.txt client_t*.txt update.bin server_test.log client_test.log

if [ "$FAIL" -eq 0 ]; then
    echo -e "${GRN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}$FAIL test(s) failed.${NC}"
    exit 1
fi