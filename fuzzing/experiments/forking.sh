#!/bin/bash

rm -rf /src/fuzzing/afl/afl-output/
rm -f /src/fuzzing/experiments/forking
rm -f /src/fuzzing/experiments/forking_gcov
rm -rf /src/fuzzing/obj_gcov/
rm -f /src/fuzzing/wtf.txt

# export AFL_DEBUG=1

afl-clang-lto++ /src/fuzzing/experiments/forking.cpp -o /src/fuzzing/experiments/forking

mkdir -p /src/fuzzing/obj_gcov

afl-g++-fast -Wno-gnu-statement-expression -fprofile-arcs -ftest-coverage -DGCOV=1 -c /src/fuzzing/experiments/forking.cpp -o /src/fuzzing/obj_gcov/forking.o

afl-g++-fast -Wno-gnu-statement-expression -fprofile-arcs -ftest-coverage -DGCOV=1 /src/fuzzing/obj_gcov/forking.o -o /src/fuzzing/experiments/forking_gcov

afl-cov -d /src/fuzzing/afl/afl-output/master/ --coverage-cmd "cat AFL_FILE | /src/fuzzing/experiments/forking_gcov /src/fuzzing/fuzzing_client_master_webserv.json" --code-dir /src/fuzzing/obj_gcov --live --overwrite --enable-branch-coverage --lcov-web-all --sleep 1 &> /dev/null &

screen -dmS sed_lcov sh -c "while true; do sleep 1; < /src/fuzzing/afl/afl-output/master/cov/lcov/trace.lcov_info_final sed 's#:/src/#:./#g' > /src/fuzzing/afl/afl-output/master/cov/lcov/lcov.info; done; exec bash"

afl-fuzz -i /src/fuzzing/afl/trimmed-tests/ -o /src/fuzzing/afl/afl-output/ -M master -- /src/fuzzing/experiments/forking /src/fuzzing/fuzzing_client_master_webserv.json 2> /src/fuzzing/master.log
