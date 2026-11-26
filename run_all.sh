
#!/usr/bin/env bash

set -u

LIB="./libftpp.1.0"
LIBRARY="./libftpp.a"
CXX="${CXX:-c++}"
CXXFLAGS="-Wall -Wextra -Werror -std=c++17 -pthread -Idata_structures -Idesign_patterns -Iiostream -Imathematics -Inetwork -Ithreading"

mkdir -p build/tests

passed=0
failed=0

for src in "$LIB"/libftpp/*.cpp; do
    [[ -f "$src" ]] || continue

    name=$(basename "$src" .cpp)
    bin="build/tests/$name"

    echo
    echo "================================"
    echo "Compiling: $src"
    echo "================================"

    if ! $CXX $CXXFLAGS -I"$LIB" "$src" "$LIBRARY" -o "$bin"; then
        echo "[COMPILE FAILED] $name"
        ((failed+=1))
        continue
    fi

    echo "Running: $name"

    if "$bin"; then
        echo "[PASS] $name"
        ((passed+=1))
    else
        echo "[FAIL] $name"
        ((failed+=1))
    fi
done

echo
echo "================================"
echo "TEST SUMMARY"
echo "================================"
echo "Passed: $passed"
echo "Failed: $failed"

[[ $failed -eq 0 ]]
