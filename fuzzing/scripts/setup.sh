#!/bin/bash

make fclean
make -C fuzzing clean

export AFL_LLVM_LAF_ALL=1

# These are mutually exclusive
# export AFL_HARDEN=1
# TODO: Right now this prints "ld.lld: error: cannot open /usr/lib/llvm-14/lib/clang/14.0.6/lib/linux/libclang_rt.asan_static-x86_64.a: No such file or directory"
# TODO: Might not work well with parallelization "you should only run one afl-fuzz instance per sanitizer type."
# https://github.com/AFLplusplus/AFLplusplus/blob/stable/docs/fuzzing_in_depth.md#c-selecting-sanitizers
export AFL_USE_ASAN=1
export AFL_USE_UBSAN=1

# TODO: Not sure if the DEBUG=1 is necessary for afl
make -C /src/fuzzing DEBUG=1 CTMIN=1 #SAN=1

mkdir -p /src/fuzzing/afl

mkdir -p /src/fuzzing/afl/minimized-tests
rm -rf /src/fuzzing/afl/minimized-tests/*
afl-cmin -i /src/fuzzing/tests -o /src/fuzzing/afl/minimized-tests -- /src/fuzzing/config_fuzzing_ctmin

mkdir -p /src/fuzzing/afl/trimmed-tests
rm -rf /src/fuzzing/afl/trimmed-tests/*
for file in /src/fuzzing/afl/minimized-tests/**; do
afl-tmin -i "$file" -o /src/fuzzing/afl/trimmed-tests/$(basename $file) -- /src/fuzzing/config_fuzzing_ctmin
done

# TODO: Not sure if the DEBUG=1 is necessary for afl
make -C /src/fuzzing DEBUG=1 AFL=1 #SAN=1
