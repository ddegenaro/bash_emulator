#!/bin/bash

# test_phase2_mac.sh
# Test script for MyShell Phase 2 - I/O Redirection
# Tests: input redirection (<), output redirection (>), append (>>)
# Mac compatible (no timeout command)

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0
RESULTS_FILE="test_results.txt"
TEST_DIR="/tmp/myshell_test_$$"

# Create temp directory for test files
mkdir -p "$TEST_DIR"

> $RESULTS_FILE

print_header() {
    echo ""
    echo -e "${BLUE}================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}================================${NC}"
}

test_pass() {
    echo -e "${GREEN}√ PASS${NC}: $1"
    echo "PASS: $1" >> $RESULTS_FILE
    ((TESTS_PASSED++))
    ((TESTS_RUN++))
}

test_fail() {
    echo -e "${RED}x FAIL${NC}: $1"
    echo "FAIL: $1" >> $RESULTS_FILE
    ((TESTS_FAILED++))
    ((TESTS_RUN++))
}

test_info() {
    echo -e "${YELLOW}ℹ INFO${NC}: $1"
    echo "INFO: $1" >> $RESULTS_FILE
}

# Check for myshell
check_executable() {
    if [ ! -f "./application/myshell" ]; then
        echo -e "${RED}Error: ./myshell symlink not found${NC}"
        echo "Expected: TestCase/myshell -> ../bin/myshell"
        exit 1
    fi
    
    if [ ! -x "./application/myshell" ]; then
        chmod +x ./application/myshell
    fi
    
    MYSHELL="./application/myshell"
}

# ============ TEST GROUP 1: Basic I/O Redirection Support ============
test_group_1() {
    print_header "GROUP 1: Basic I/O Redirection Support"
    
    # Test 1: Output redirection (>)
    output=$(echo -e "echo hello > $TEST_DIR/test1.txt\nexit" | $MYSHELL 2>&1)
    if [ -f "$TEST_DIR/test1.txt" ]; then
        content=$(cat "$TEST_DIR/test1.txt")
        if [ "$content" = "hello" ]; then
            test_pass "Output redirection (>) creates file"
        else
            test_fail "Output redirection (>) creates file but content wrong"
        fi
    else
        test_fail "Output redirection (>) does not create file"
    fi
    
    # Test 2: Append redirection (>>)
    echo "line1" > "$TEST_DIR/test2.txt"
    output=$(echo -e "echo line2 >> $TEST_DIR/test2.txt\nexit" | $MYSHELL 2>&1)
    if [ -f "$TEST_DIR/test2.txt" ]; then
        lines=$(wc -l < "$TEST_DIR/test2.txt")
        if [ "$lines" -ge 2 ]; then
            test_pass "Append redirection (>>) appends to file"
        else
            test_fail "Append redirection (>>) does not append"
        fi
    else
        test_fail "Append redirection (>>) file not found"
    fi
    
    # Test 3: Input redirection (<)
    echo "test input" > "$TEST_DIR/test3.txt"
    output=$(echo -e "cat < $TEST_DIR/test3.txt\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "test input"; then
        test_pass "Input redirection (<) reads from file"
    else
        test_fail "Input redirection (<) does not read file"
    fi
}

# ============ TEST GROUP 2: Output Redirection (>) ============
test_group_2() {
    print_header "GROUP 2: Output Redirection (>)"
    
    # Test 1: Simple output redirection
    echo -e "echo 'test content' > $TEST_DIR/output1.txt\nexit" | $MYSHELL > /dev/null 2>&1
    if [ -f "$TEST_DIR/output1.txt" ]; then
        test_pass "Output redirection creates file"
    else
        test_fail "Output redirection does not create file"
    fi
    
    # Test 2: Output overwrites existing file
    echo "old content" > "$TEST_DIR/overwrite.txt"
    echo -e "echo 'new content' > $TEST_DIR/overwrite.txt\nexit" | $MYSHELL > /dev/null 2>&1
    if [ -f "$TEST_DIR/overwrite.txt" ]; then
        content=$(cat "$TEST_DIR/overwrite.txt")
        if echo "$content" | grep -q "new content"; then
            test_pass "Output redirection overwrites existing file"
        else
            test_fail "Output redirection does not overwrite correctly"
        fi
    else
        test_fail "Output redirection file not found"
    fi
    
    # Test 3: Multiple output redirections
    echo -e "echo 'file1' > $TEST_DIR/multi1.txt\necho 'file2' > $TEST_DIR/multi2.txt\nexit" | $MYSHELL > /dev/null 2>&1
    if [ -f "$TEST_DIR/multi1.txt" ] && [ -f "$TEST_DIR/multi2.txt" ]; then
        test_pass "Multiple output redirections work"
    else
        test_fail "Multiple output redirections failed"
    fi
    
    # Test 4: Output redirection with path
    echo -e "echo 'path test' > $TEST_DIR/subdir/output.txt\nexit" | $MYSHELL > /dev/null 2>&1
    if [ -f "$TEST_DIR/subdir/output.txt" ]; then
        test_pass "Output redirection creates file in subdirectory"
    else
        test_fail "Output redirection does not handle subdirectories"
    fi
}

# ============ TEST GROUP 3: Append Redirection (>>) ============
test_group_3() {
    print_header "GROUP 3: Append Redirection (>>)"
    
    # Test 1: Simple append
    echo "line1" > "$TEST_DIR/append1.txt"
    echo -e "echo 'line2' >> $TEST_DIR/append1.txt\nexit" | $MYSHELL > /dev/null 2>&1
    if [ -f "$TEST_DIR/append1.txt" ]; then
        lines=$(wc -l < "$TEST_DIR/append1.txt")
        if [ "$lines" -ge 2 ]; then
            test_pass "Append redirection adds lines to file"
        else
            test_fail "Append redirection does not add lines"
        fi
    else
        test_fail "Append redirection file not found"
    fi
    
    # Test 2: Append to non-existent file (creates it)
    rm -f "$TEST_DIR/append2.txt"
    echo -e "echo 'first' >> $TEST_DIR/append2.txt\nexit" | $MYSHELL > /dev/null 2>&1
    if [ -f "$TEST_DIR/append2.txt" ]; then
        test_pass "Append redirection creates file if not exists"
    else
        test_fail "Append redirection does not create new file"
    fi
    
    # Test 3: Multiple appends
    rm -f "$TEST_DIR/append3.txt"
    echo -e "echo 'a' >> $TEST_DIR/append3.txt\necho 'b' >> $TEST_DIR/append3.txt\necho 'c' >> $TEST_DIR/append3.txt\nexit" | $MYSHELL > /dev/null 2>&1
    if [ -f "$TEST_DIR/append3.txt" ]; then
        lines=$(wc -l < "$TEST_DIR/append3.txt")
        if [ "$lines" -ge 3 ]; then
            test_pass "Multiple appends work correctly"
        else
            test_fail "Multiple appends do not add all lines"
        fi
    else
        test_fail "Multiple appends file not found"
    fi
}

# ============ TEST GROUP 4: Input Redirection (<) ============
test_group_4() {
    print_header "GROUP 4: Input Redirection (<)"
    
    # Test 1: Simple input redirection
    echo "input file content" > "$TEST_DIR/input1.txt"
    output=$(echo -e "cat < $TEST_DIR/input1.txt\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "input file content"; then
        test_pass "Input redirection reads file content"
    else
        test_fail "Input redirection does not read file"
    fi
    
    # Test 2: Input with grep
    echo -e "line1\nline2\nline3" > "$TEST_DIR/input2.txt"
    output=$(echo -e "grep line2 < $TEST_DIR/input2.txt\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "line2"; then
        test_pass "Input redirection works with grep"
    else
        test_fail "Input redirection with grep failed"
    fi
    
    # Test 3: Input file not found
    output=$(echo -e "cat < $TEST_DIR/nonexistent.txt\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -qE "No such file|cannot open"; then
        test_pass "Input redirection handles missing file"
    else
        test_info "Input redirection missing file error not shown"
    fi
}

# ============ TEST GROUP 5: Combined Redirections ============
test_group_5() {
    print_header "GROUP 5: Combined Redirections"
    
    # Test 1: Input and output redirection (cat < input > output)
    echo "input data" > "$TEST_DIR/combined1.txt"
    echo -e "cat < $TEST_DIR/combined1.txt > $TEST_DIR/combined_out1.txt\nexit" | $MYSHELL > /dev/null 2>&1
    if [ -f "$TEST_DIR/combined_out1.txt" ]; then
        content=$(cat "$TEST_DIR/combined_out1.txt")
        if echo "$content" | grep -q "input data"; then
            test_pass "Input and output redirection together work"
        else
            test_fail "Combined redirection output is incorrect"
        fi
    else
        test_fail "Combined redirection does not create output file"
    fi
    
    # Test 2: Input redirection with command piping
    echo "test" > "$TEST_DIR/combined2.txt"
    output=$(echo -e "cat < $TEST_DIR/combined2.txt\nexit" | $MYSHELL 2>&1)
    if echo "$output" | grep -q "test"; then
        test_pass "Input redirection with multiple commands"
    else
        test_fail "Input with multiple commands failed"
    fi
}

# ============ TEST GROUP 6: Error Redirection (2>) ============
test_group_6() {
    print_header "GROUP 6: Error Redirection (2>)"
    
    # Test 1: Redirect stderr to file
    echo -e "cat /nonexistent/file 2> $TEST_DIR/error1.txt\nexit" | $MYSHELL > /dev/null 2>&1
    if [ -f "$TEST_DIR/error1.txt" ]; then
        size=$(wc -c < "$TEST_DIR/error1.txt")
        if [ "$size" -gt 0 ]; then
            test_pass "Error redirection (2>) captures errors"
        else
            test_fail "Error redirection (2>) file is empty"
        fi
    else
        test_fail "Error redirection (2>) does not create file"
    fi
    
    # Test 2: Redirect to /dev/null (suppress errors)
    output=$(echo -e "cat /nonexistent 2> /dev/null\nexit" | $MYSHELL 2>&1)
    if ! echo "$output" | grep -q "No such file"; then
        test_pass "Error redirection to /dev/null suppresses errors"
    else
        test_fail "Error redirection to /dev/null not working"
    fi
}

# ============ TEST GROUP 7: Redirection with Built-in Commands ============
test_group_7() {
    print_header "GROUP 7: Redirection with Built-in Commands"
    
    # Test 1: pwd output redirection
    echo -e "pwd > $TEST_DIR/pwd_out.txt\nexit" | $MYSHELL > /dev/null 2>&1
    if [ -f "$TEST_DIR/pwd_out.txt" ]; then
        content=$(cat "$TEST_DIR/pwd_out.txt")
        if [ ! -z "$content" ]; then
            test_pass "pwd with output redirection works"
        else
            test_fail "pwd output redirection file is empty"
        fi
    else
        test_fail "pwd output redirection does not create file"
    fi
    
    # Test 2: echo with special characters redirection
    echo -e "echo 'hello > world' > $TEST_DIR/special.txt\nexit" | $MYSHELL > /dev/null 2>&1
    if [ -f "$TEST_DIR/special.txt" ]; then
        test_pass "Output redirection handles special characters"
    else
        test_fail "Output redirection with special characters failed"
    fi
}

# ============ TEST GROUP 8: Edge Cases ============
test_group_8() {
    print_header "GROUP 8: Edge Cases"
    
    # Test 1: Multiple spaces around redirection
    echo -e "echo test  >  $TEST_DIR/spaces.txt\nexit" | $MYSHELL > /dev/null 2>&1
    if [ -f "$TEST_DIR/spaces.txt" ]; then
        test_pass "Output redirection with extra spaces"
    else
        test_fail "Output redirection with spaces failed"
    fi
    
    # Test 2: Redirection with empty command
    echo -e "> $TEST_DIR/empty.txt\nexit" | $MYSHELL > /dev/null 2>&1
    if [ -f "$TEST_DIR/empty.txt" ]; then
        test_pass "Empty command with redirection handled"
    else
        test_info "Empty redirection not supported (optional)"
    fi
    
    # Test 3: Output to current directory
    echo -e "echo 'test' > testfile.txt\nexit" | $MYSHELL > /dev/null 2>&1
    if [ -f "testfile.txt" ]; then
        test_pass "Output redirection to current directory"
        rm -f testfile.txt
    else
        test_fail "Output redirection to current directory failed"
    fi
}

# ============ MAIN ============
main() {
    echo ""
    echo -e "${BLUE}╔════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║  MyShell Phase 2 - I/O Redirection Tests   ║${NC}"
    echo -e "${BLUE}║  Georgetown University                     ║${NC}"
    echo -e "${BLUE}╚════════════════════════════════════════════╝${NC}"
    echo ""
    
    check_executable
    echo -e "${GREEN}Found executable: $MYSHELL${NC}"
    echo -e "${GREEN}Test directory: $TEST_DIR${NC}"
    echo ""
    
    test_group_1
    test_group_2
    test_group_3
    test_group_4
    test_group_5
    test_group_6
    test_group_7
    test_group_8
    
    # Cleanup
    # rm -rf "$TEST_DIR"
    
    # Print summary
    print_header "TEST SUMMARY"
    echo -e "Total Tests Run:  ${BLUE}$TESTS_RUN${NC}"
    echo -e "Tests Passed:     ${GREEN}$TESTS_PASSED${NC}"
    echo -e "Tests Failed:     ${RED}$TESTS_FAILED${NC}"
    echo ""
    
    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "${GREEN}All tests passed! ✓${NC}"
        echo "Results saved to: $RESULTS_FILE"
        return 0
    else
        if [ $TESTS_FAILED -le 3 ]; then
            echo -e "${YELLOW}Most tests passed! ⚠${NC}"
        else
            echo -e "${RED}Some tests failed.${NC}"
        fi
        echo "Results saved to: $RESULTS_FILE"
        return 1
    fi
}

main
exit $?
