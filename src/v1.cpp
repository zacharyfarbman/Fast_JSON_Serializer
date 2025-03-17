#include <benchmark/benchmark.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <vector>

class Buffer {
 public:
  explicit Buffer(size_t capacity)
      : data_(std::make_unique<char[]>(capacity)),
        capacity_(capacity),
        size_(0) {}

  // Append Methods:
  void append(const char* str, size_t len) {
    if (size_ + len <= capacity_) {
      std::memcpy(data_.get() + size_, str, len);
      size_ += len;
    }
  }

  void append(char c) {
    if (size_ < capacity_) {
      data_[size_++] = c;
    }
  }

  void append(const char* str) { append(str, strlen(str)); }

  // Reset buffer for reuse
  void reset() { size_ = 0; }

  // Accessor Methods:
  char* current() { return data_.get() + size_; }
  size_t remaining() const { return capacity_ - size_; }
  const char* data() const { return data_.get(); }
  size_t size() const { return size_; }

 private:
  std::unique_ptr<char[]> data_;
  size_t capacity_;
  size_t size_;
};

template <typename BufferType>
class JsonSerializer {
 private:
  // Helper Method
  void write_key(const char* key) {
    if (!first_field_) {
      buffer_.append(',');
    } else {
      first_field_ = false;
    }
    buffer_.append('"');
    buffer_.append(key, strlen(key));
    buffer_.append('"');
    buffer_.append(':');
  }
  BufferType& buffer_;  // Reference to output buffer
  bool first_field_;    // Tracks first field status for comma insertion

 public:
  explicit JsonSerializer(BufferType& buffer)
      : buffer_(buffer), first_field_(true) {}

  // Object Structure Methods
  void begin_object() {
    buffer_.append('{');
    first_field_ = true;
  }

  void end_object() { buffer_.append('}'); }

  // Serialization Methods by Type
  void serialize(const char* key, const std::string& value) {
    write_key(key);
    buffer_.append('"');
    for (char c : value) {
      if (c == '"' || c == '\\') {
        buffer_.append('\\');
        buffer_.append(c);
      } else if (c == '\n') {
        buffer_.append('\\');
        buffer_.append('n');
      } else if (c == '\r') {
        buffer_.append('\\');
        buffer_.append('r');
      } else if (c == '\t') {
        buffer_.append('\\');
        buffer_.append('t');
      } else {
        buffer_.append(c);
      }
    }
    buffer_.append('"');
  }

  void serialize(const char* key, const char* value) {
    write_key(key);
    buffer_.append('"');
    buffer_.append(value, strlen(value));
    buffer_.append('"');
  }

  void serialize(const char* key, double value) {
    write_key(key);
    char buf[32];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
    if (ec == std::errc()) {
      buffer_.append(buf, ptr - buf);
    }
  }

  void serialize(const char* key, int64_t value) {
    write_key(key);
    char buf[32];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
    if (ec == std::errc()) {
      buffer_.append(buf, ptr - buf);
    }
  }

  void serialize(const char* key, bool value) {
    write_key(key);
    if (value) {
      buffer_.append("true", 4);
    } else {
      buffer_.append("false", 5);
    }
  }
};

namespace fields {
constexpr const char SYMBOL[] = "symbol";
constexpr const char PRICE[] = "price";
constexpr const char SIZE[] = "size";
constexpr const char REQUEST_TYPE[] = "request_type";
}  // namespace fields

namespace constants {
constexpr const char PLACE[] = "place";
constexpr const char UPDATE[] = "update";
constexpr const char CANCEL[] = "cancel";
constexpr const char ORDER_ID[] = "order_id";
constexpr const char IS_BUY[] = "is_buy";
constexpr const char TIMESTAMP[] = "timestamp";
constexpr const char ORDERS[] = "orders";
}  // namespace constants

template <typename T, const char* Name, typename Type, Type T::* Member>
struct Field {
  static constexpr const char* name = Name;

  template <typename U>
  static const Type& get(const U& obj) {
    return static_cast<const T&>(obj).*Member;
  }
};

template <typename T, const char* Name, const char* Value>
struct ConstantField {
  static constexpr const char* name = Name;

  template <typename U>
  static const char* get(const U&) {
    return Value;
  }
};

// Schema definition as a compile-time list of fields
template <typename... Fields>
struct Schema {
  template <typename T, typename BufferType>
  static void serialize(const T& obj, JsonSerializer<BufferType>& serializer) {
    serializer.begin_object();
    serialize_fields<0>(obj, serializer);
    serializer.end_object();
  }

  template <size_t I, typename T, typename BufferType>
  static void serialize_fields(const T& obj,
                               JsonSerializer<BufferType>& serializer) {
    if constexpr (I < sizeof...(Fields)) {
      using FieldType = std::tuple_element_t<I, std::tuple<Fields...>>;
      serializer.serialize(FieldType::name, FieldType::get(obj));
      serialize_fields<I + 1>(obj, serializer);
    }
  }
};

// Simple request structure
struct PlaceReq {
  std::string symbol;
  double price;
  double size;
};

using PlaceReqSchema =
    Schema<ConstantField<PlaceReq, fields::REQUEST_TYPE, constants::PLACE>,
           Field<PlaceReq, fields::SYMBOL, std::string, &PlaceReq::symbol>,
           Field<PlaceReq, fields::PRICE, double, &PlaceReq::price>,
           Field<PlaceReq, fields::SIZE, double, &PlaceReq::size>>;

// Serialize function for PlaceReq
void serialize_place_req(Buffer& buf, const PlaceReq& req) {
  JsonSerializer<Buffer> serializer(buf);
  PlaceReqSchema::serialize(req, serializer);
}

// Additional request types for benchmarking
struct UpdateReq {
  std::string symbol;
  std::string order_id;
  double price;
  double size;
};

using UpdateReqSchema = Schema<
    ConstantField<UpdateReq, fields::REQUEST_TYPE, constants::UPDATE>,
    Field<UpdateReq, fields::SYMBOL, std::string, &UpdateReq::symbol>,
    Field<UpdateReq, constants::ORDER_ID, std::string, &UpdateReq::order_id>,
    Field<UpdateReq, fields::PRICE, double, &UpdateReq::price>,
    Field<UpdateReq, fields::SIZE, double, &UpdateReq::size>>;

// Serialize function for UpdateReq
void serialize_update_req(Buffer& buf, const UpdateReq& req) {
  JsonSerializer<Buffer> serializer(buf);
  UpdateReqSchema::serialize(req, serializer);
}

// Helper for generating test data
class TestData {
 public:
  static std::string random_string(size_t length,
                                   bool with_special_chars = false) {
    static const char alphanum[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789";
    static const char special[] = "\"\\'\n\r\t";

    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> alpha_dist(0, sizeof(alphanum) - 2);
    static std::uniform_int_distribution<> special_dist(0, sizeof(special) - 2);

    std::string result;
    result.reserve(length);

    for (size_t i = 0; i < length; ++i) {
      if (with_special_chars && (rand() % 10 == 0)) {  // 10% special chars
        result += special[special_dist(gen)];
      } else {
        result += alphanum[alpha_dist(gen)];
      }
    }

    return result;
  }

  static PlaceReq create_place_req() {
    PlaceReq req;
    req.symbol = random_string(6);
    req.price = 40000.0 + (rand() % 10000) / 10.0;
    req.size = 1.0 + (rand() % 1000) / 100.0;
    return req;
  }

  static UpdateReq create_update_req() {
    UpdateReq req;
    req.symbol = random_string(6);
    req.order_id = random_string(16);
    req.price = 40000.0 + (rand() % 10000) / 10.0;
    req.size = 1.0 + (rand() % 1000) / 100.0;
    return req;
  }
};

// Benchmark appending a small fixed string
static void BM_BufferAppendSmallString(benchmark::State& state) {
  for (auto _ : state) {
    Buffer buffer(1024);
    for (int i = 0; i < 100; ++i) {
      buffer.append("small_string", 12);
    }
    benchmark::DoNotOptimize(buffer.data());
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_BufferAppendSmallString);

// Benchmark appending individual characters
static void BM_BufferAppendChar(benchmark::State& state) {
  for (auto _ : state) {
    Buffer buffer(1024);
    for (int i = 0; i < 1000; ++i) {
      buffer.append('x');
    }
    benchmark::DoNotOptimize(buffer.data());
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_BufferAppendChar);

// Benchmark appending strings of different sizes
static void BM_BufferAppendVaryingString(benchmark::State& state) {
  // String length to test
  const size_t length = state.range(0);

  for (auto _ : state) {
    state.PauseTiming();
    std::string test_string(length, 'x');
    Buffer buffer(length + 100);  // Buffer slightly larger than string
    state.ResumeTiming();

    buffer.append(test_string.c_str(), test_string.size());
    benchmark::DoNotOptimize(buffer.data());
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_BufferAppendVaryingString)
    ->Range(10, 4096);  // Test with different string sizes

// Benchmark serializing simple string values
static void BM_SerializeSimpleString(benchmark::State& state) {
  const size_t length = state.range(0);

  for (auto _ : state) {
    state.PauseTiming();
    std::string test_string = TestData::random_string(length, false);
    Buffer buffer(2048);
    JsonSerializer<Buffer> serializer(buffer);
    state.ResumeTiming();

    serializer.begin_object();
    serializer.serialize("key", test_string);
    serializer.end_object();

    benchmark::DoNotOptimize(buffer.data());
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_SerializeSimpleString)->Range(10, 1024);

// Benchmark serializing strings with special characters
static void BM_SerializeComplexString(benchmark::State& state) {
  const size_t length = state.range(0);

  for (auto _ : state) {
    state.PauseTiming();
    std::string test_string = TestData::random_string(length, true);
    Buffer buffer(4096);
    JsonSerializer<Buffer> serializer(buffer);
    state.ResumeTiming();

    serializer.begin_object();
    serializer.serialize("key", test_string);
    serializer.end_object();

    benchmark::DoNotOptimize(buffer.data());
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_SerializeComplexString)->Range(10, 1024);

// Benchmark serializing numeric values
static void BM_SerializeNumeric(benchmark::State& state) {
  for (auto _ : state) {
    Buffer buffer(1024);
    JsonSerializer<Buffer> serializer(buffer);

    serializer.begin_object();
    serializer.serialize("int", int64_t(123456789));
    serializer.serialize("double", 12345.6789);
    serializer.serialize("price", 42069.25);
    serializer.end_object();

    benchmark::DoNotOptimize(buffer.data());
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_SerializeNumeric);

// Benchmark serializing boolean values
static void BM_SerializeBoolean(benchmark::State& state) {
  for (auto _ : state) {
    Buffer buffer(1024);
    JsonSerializer<Buffer> serializer(buffer);

    serializer.begin_object();
    serializer.serialize("true_val", true);
    serializer.serialize("false_val", false);
    serializer.end_object();

    benchmark::DoNotOptimize(buffer.data());
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_SerializeBoolean);

// Benchmark serializing PlaceReq using schema
static void BM_SchemaSerializePlaceReq(benchmark::State& state) {
  PlaceReq req = TestData::create_place_req();

  for (auto _ : state) {
    Buffer buffer(1024);
    serialize_place_req(buffer, req);
    benchmark::DoNotOptimize(buffer.data());
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_SchemaSerializePlaceReq);

// Benchmark manually serializing PlaceReq
static void BM_ManualSerializePlaceReq(benchmark::State& state) {
  PlaceReq req = TestData::create_place_req();

  for (auto _ : state) {
    Buffer buffer(1024);
    JsonSerializer<Buffer> serializer(buffer);

    serializer.begin_object();
    serializer.serialize(fields::REQUEST_TYPE, constants::PLACE);
    serializer.serialize(fields::SYMBOL, req.symbol);
    serializer.serialize(fields::PRICE, req.price);
    serializer.serialize(fields::SIZE, req.size);
    serializer.end_object();

    benchmark::DoNotOptimize(buffer.data());
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_ManualSerializePlaceReq);

// Benchmark serializing UpdateReq using schema
static void BM_SchemaSerializeUpdateReq(benchmark::State& state) {
  UpdateReq req = TestData::create_update_req();

  for (auto _ : state) {
    Buffer buffer(1024);
    serialize_update_req(buffer, req);
    benchmark::DoNotOptimize(buffer.data());
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_SchemaSerializeUpdateReq);

// Benchmark manually serializing UpdateReq
static void BM_ManualSerializeUpdateReq(benchmark::State& state) {
  UpdateReq req = TestData::create_update_req();

  for (auto _ : state) {
    Buffer buffer(1024);
    JsonSerializer<Buffer> serializer(buffer);

    serializer.begin_object();
    serializer.serialize(fields::REQUEST_TYPE, constants::UPDATE);
    serializer.serialize(fields::SYMBOL, req.symbol);
    serializer.serialize(constants::ORDER_ID, req.order_id);
    serializer.serialize(fields::PRICE, req.price);
    serializer.serialize(fields::SIZE, req.size);
    serializer.end_object();

    benchmark::DoNotOptimize(buffer.data());
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_ManualSerializeUpdateReq);

// Measure serialization latency distribution
static void BM_SerializationLatencyPercentiles(benchmark::State& state) {
  const int iterations = 10000;
  std::vector<int64_t> latencies;
  latencies.reserve(iterations);

  PlaceReq req = TestData::create_place_req();

  for (auto _ : state) {
    state.PauseTiming();
    latencies.clear();
    Buffer buffer(1024);
    state.ResumeTiming();

    for (int i = 0; i < iterations; ++i) {
      buffer.reset();

      auto start = std::chrono::high_resolution_clock::now();
      serialize_place_req(buffer, req);
      auto end = std::chrono::high_resolution_clock::now();

      auto duration =
          std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
              .count();
      latencies.push_back(duration);

      benchmark::DoNotOptimize(buffer.data());
      benchmark::ClobberMemory();
    }

    // Sort to calculate percentiles
    std::sort(latencies.begin(), latencies.end());

    // Report percentiles
    state.counters["p50_ns"] = latencies[iterations / 2];
    state.counters["p90_ns"] = latencies[iterations * 9 / 10];
    state.counters["p99_ns"] = latencies[iterations * 99 / 100];
    state.counters["p999_ns"] = latencies[iterations * 999 / 1000];
    state.counters["max_ns"] = latencies.back();
  }
}
BENCHMARK(BM_SerializationLatencyPercentiles)->Iterations(3);

// Benchmark with buffer reuse
static void BM_BufferReuse(benchmark::State& state) {
  Buffer buffer(1024);
  PlaceReq req = TestData::create_place_req();

  for (auto _ : state) {
    buffer.reset();
    serialize_place_req(buffer, req);
    benchmark::DoNotOptimize(buffer.data());
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_BufferReuse);

// Benchmark without buffer reuse (new buffer for each iteration)
static void BM_NoBufferReuse(benchmark::State& state) {
  PlaceReq req = TestData::create_place_req();

  for (auto _ : state) {
    Buffer buffer(1024);
    serialize_place_req(buffer, req);
    benchmark::DoNotOptimize(buffer.data());
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_NoBufferReuse);

// Main benchmark function
BENCHMARK_MAIN();
