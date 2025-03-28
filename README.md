```bash
# compile benchmark for version 1
g++ -std=c++20 -O3 src/v1.cpp -o json_serializer_v1 -lbenchmark

./json_serializer_v1

# compile benchmark for version 3
g++ -std=c++20 -O3 src/v3.cpp -o json_serializer_v3 -lbenchmark
# compile example for version 3
g++ -DRUN_EXAMPLE -std=c++20 -O3 src/v3.cpp -o json_serializer_v3 -lbenchmark

./json_serializer_v3

# compile benchmark for version 4
clang++ -std=c++23 -O3 src/v4.cpp -lbenchmark -o json_serializer_v4

./json_serializer_v4

# If you experience an error with linking the benchmark library add:
-I/usr/local/include -L/usr/local/lib
# to the compile flags

# if building with cmake:
mkdir build
cd build
cmake .
cmake --build .
./json_serializer_v4
```
