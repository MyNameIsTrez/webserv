# llvmfuzzer

See the [documentation](https://llvm.org/docs/LibFuzzer.html) for more information.

## Run fuzzer with address sanitizer

`clear && clang++-14 -fsanitize=fuzzer,undefined,address -Wall -Wextra -Werror -Wpedantic -Wshadow -Wswitch -Wimplicit-fallthrough -Wno-c99-designator -std=c++2b -g3 -Wfatal-errors -fprofile-instr-generate -fcoverage-mapping --coverage -DSUPPRESS_LOGGING webserv_fuzzer.cpp ../src/config/Config.cpp ../src/config/JSON.cpp ../src/config/Node.cpp ../src/config/Tokenizer.cpp ../src/Logger.cpp && mkdir -p ./corp && ./a.out ./corp -dict=conf.dict`

## Create .profdata coverage information

1. `mkdir -p corp_minimized && ./a.out -merge=1 ./corp_minimized ./corp`
2. `./a.out -runs=0 ./corp_minimized`
3. `llvm-profdata-14 merge -sparse default.profraw -o fuzzer.profdata`

### Show coverage

4. `llvm-cov-14 show ./a.out -instr-profile=fuzzer.profdata`

All together now:\
`rm -rf corp_minimized && mkdir -p corp_minimized && ./a.out -merge=1 ./corp_minimized ./corp && ./a.out -runs=0 ./corp_minimized && llvm-profdata-14 merge -sparse default.profraw -o fuzzer.profdata && llvm-cov-14 show ./a.out -instr-profile=fuzzer.profdata`

### Convert to lcov

4. `llvm-cov-14 export ./a.out -format=lcov -instr-profile=fuzzer.profdata | sed 's#/llvmfuzzer/../#/#g' > lcov.info`

All together now:\
`mkdir -p corp_minimized && ./a.out -merge=1 ./corp_minimized ./corp && ./a.out -runs=0 ./corp_minimized && llvm-profdata-14 merge -sparse default.profraw -o fuzzer.profdata && llvm-cov-14 export ./a.out -format=lcov -instr-profile=fuzzer.profdata | sed 's#/llvmfuzzer/../#/#g' > lcov.info`

## Clean directory

`setopt -o nullglob && rm -rf corp corp_minimized a.out *.gcda *.gcno *.profraw *.profdata lcov.info && setopt +o nullglob`
