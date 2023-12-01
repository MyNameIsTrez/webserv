#!/bin/bash

# export AFL_AUTORESUME=1
# export AFL_HANG_TMOUT=1000

# For debugging crashes, from here:
# https://github.com/AFLplusplus/AFLplusplus/issues/696#issuecomment-764572672
# export AFL_DEBUG=1
# ulimit -c unlimited
# export ASAN_OPTIONS="disable_coredump=0:unmap_shadow_on_exit=1:abort_on_error=1:detect_leaks=0:symbolize=1"

if [ -z "$FUZZ_CLIENT" ] # If FUZZ_CLIENT is not defined
then
dict_path=/src/fuzzing/conf_fuzzing_logger.dict
else
dict_path=/src/fuzzing/conf_fuzzing_client.dict
fi
# echo '$dict_path is:' $dict_path

# -D enables fuzzing strategies "bit flips", "byte flips", "arithmetics", "known ints", "dictionary", "eff"; you may want to omit this optional flag
# According to the --help page: "-D: enable deterministic fuzzing (once per queue entry)", which is strange, since it doesn't mention the enabled fuzzing strategies

echo "Fuzzing core 0"
# Explanation of this command:
# https://unix.stackexchange.com/a/47279/544554
screen -dmS master sh -c "afl-fuzz -D -i /src/fuzzing/afl/trimmed-tests -x $dict_path -o /src/fuzzing/afl/afl-output -M master -- /src/fuzzing/fuzzing_afl /src/fuzzing/fuzzing_client_master_webserv.json 2> /src/fuzzing/master.log; exec bash"

sleep 1

export slave_count=3 # 3, because I have 4 cores on my computer
echo "\$slave_count is $slave_count"

# -0 means defaulting to 0 slaves
for i in $(seq 1 "${slave_count:-0}")
do
echo "Fuzzing core $i"

screen -dmS "slave_$i" sh -c "afl-fuzz -D -i /src/fuzzing/afl/trimmed-tests -x $dict_path -o /src/fuzzing/afl/afl-output -S "slave_$i" -- /src/fuzzing/fuzzing_afl /src/fuzzing/fuzzing_client_slave_$i\_webserv.json 2> /src/fuzzing/slave_$i.log; exec bash"
sleep 1
done
