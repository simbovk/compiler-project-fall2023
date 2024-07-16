# Compiler project (FAll 2023)

A simple compiler project based on llvm.

## How to run?
```
mkdir build
cd build
cmake ..
make
cd src
./gsm "<the input you want to be compiled>" > gsm.ll
llc --filetype=obj -o=gsm.o gsm.ll
clang -o gsmbin gsm.o ../../rtGSM.c
```

## Sample inputs
