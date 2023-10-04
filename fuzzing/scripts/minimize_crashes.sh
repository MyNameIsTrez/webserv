#!/bin/bash

mkdir -p /src/fuzzing/afl/minimized-crashes
rm -rf /src/fuzzing/afl/minimized-crashes/*
i=0
for file in /src/fuzzing/afl/afl-output/master/crashes/id*; do
afl-tmin -i "$file" -o /src/fuzzing/afl/minimized-crashes/$i -- /src/fuzzing/config_fuzzing_ctmin
i=$(($i + 1))
done

md5sum /src/fuzzing/afl/minimized-crashes/* | sort | awk 'BEGIN{lasthash = ""} $1 == lasthash {print $2} {lasthash = $1}' | xargs rm -f
