#!/bin/bash

# Leave this commented out!
# export AFL_DEBUG=1

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

# TODO: Try this: `Are you aware of the '-T all' parallelize option that improves the speed for large/slow corpuses?`

mkdir -p /src/fuzzing/afl/minimized-tests
rm -rf /src/fuzzing/afl/minimized-tests/*
if [ -z "$FUZZ_CLIENT" ] # If FUZZ_CLIENT is not defined
then
afl-cmin -i /src/fuzzing/tests_fuzzing_logger -o /src/fuzzing/afl/minimized-tests -- /src/fuzzing/config_fuzzing_ctmin
else
afl-cmin -i /src/fuzzing/tests_fuzzing_client -o /src/fuzzing/afl/minimized-tests -- /src/fuzzing/config_fuzzing_ctmin
fi

mkdir -p /src/fuzzing/afl/trimmed-tests
rm -rf /src/fuzzing/afl/trimmed-tests/*
for file in /src/fuzzing/afl/minimized-tests/**; do
afl-tmin -i "$file" -o /src/fuzzing/afl/trimmed-tests/$(basename $file) -- /src/fuzzing/config_fuzzing_ctmin
done

# TODO: Not sure if the DEBUG=1 is necessary for afl
make -C /src/fuzzing DEBUG=1 AFL=1 #SAN=1
