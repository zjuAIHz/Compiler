./src/build/src ./tests/Matrix/Array.c
gcc -no-pie Compiler.o lib/io.so -o tests/Matrix/matrix.out
./Ref/tester/matrix/matrix-linux-amd64 ./tests/Matrix/matrix.out