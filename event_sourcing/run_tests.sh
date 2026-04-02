#!/bin/bash
# Run all test executables from the build directory

BUILD_DIR="$(dirname "$0")/build"
PASS=0
FAIL=0
FAILED_TESTS=()

tests=(
    branch_entity_simple
    project
    test_entity_lifecycle
    test_entity_simple
    test_event_sourcing
    test_relationships
    test_track_entity_order
)

for test in "${tests[@]}"; do
    exe="$BUILD_DIR/$test"
    if [ ! -x "$exe" ]; then
        echo "SKIP  $test (not found)"
        continue
    fi

    echo "--- $test ---"
    output=$("$exe" 2>&1)
    rc=$?
    if [ $rc -eq 0 ]; then
        echo "PASS  $test"
        ((PASS++))
    else
        echo "$output" | tail -5
        echo "FAIL  $test (exit code $rc)"
        ((FAIL++))
        FAILED_TESTS+=("$test")
    fi
    echo
done

echo "================================"
echo "Results: $PASS passed, $FAIL failed"
if [ ${#FAILED_TESTS[@]} -gt 0 ]; then
    echo "Failed: ${FAILED_TESTS[*]}"
fi
exit $FAIL
