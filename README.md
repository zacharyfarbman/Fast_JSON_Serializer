```bash
# compile benchmark for version 1
g++ -std=c++20 -O3 src/v1.cpp -o json_serializer_v1 -lbenchmark -pthread

./json_serializer_v1

# compile benchmark for version 3
g++ -std=c++20 -O3 src/v3.cpp -o json_serializer_v3 -lbenchmark -pthread
# run example for version 3
g++ -DRUN_EXAMPLE -std=c++20 -O3 src/v3.cpp -o json_serializer_v3 -lbenchmark -pthread

./json_serializer_v3
```
