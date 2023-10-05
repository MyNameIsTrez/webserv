#!/bin/bash

# export AFL_AUTORESUME=1
# export AFL_HANG_TMOUT=1000
# export AFL_DEBUG=1

# -D enables fuzzing strategies "bit flips", "byte flips", "arithmetics", "known ints", "dictionary", "eff"; you may want to omit this optional flag
# According to the --help page: "-D: enable deterministic fuzzing (once per queue entry)", which is strange, since it doesn't mention the enabled fuzzing strategies
afl-fuzz -D -i /src/fuzzing/afl/trimmed-tests -x /src/fuzzing/conf.dict -o /src/fuzzing/afl/afl-output -M master -- /src/fuzzing/config_fuzzing_afl
