#include <benchmark/benchmark.h>

#include <array>
#include <charconv>
#include <cstdint>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#if defined(__GNUC__) || defined(__clang__)
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif

#if defined(__GNUC__) || defined(__clang__)
#define FORCE_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE inline
#endif

#if defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER)
#define ALIGNED(x) alignas(x)
#else
#define ALIGNED(x)
#endif

class ALIGNED(64) Buffer {
 public:
  explicit Buffer(size_t capacity)
      : data_(std::make_unique<char[]>(capacity)),
        capacity_(capacity),
        size_(0) {}

  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;
  Buffer(Buffer&&) noexcept = default;
  Buffer& operator=(Buffer&&) noexcept = default;

  FORCE_INLINE void reserve(size_t new_capacity) {
    if (LIKELY(new_capacity <= capacity_)) {
      return;
    }

    auto new_data = std::make_unique<char[]>(new_capacity);
    std::memcpy(new_data.get(), data_.get(), size_);
    data_ = std::move(new_data);
    capacity_ = new_capacity;
  }

  FORCE_INLINE void append(const char* str, size_t len) {
    if (UNLIKELY(size_ + len > capacity_)) {
      reserve((size_ + len) * 2);
    }
    std::memcpy(data_.get() + size_, str, len);
    size_ += len;
  }

  FORCE_INLINE void append(char c) {
    if (UNLIKELY(size_ + 1 > capacity_)) {
      reserve(capacity_ * 2);
    }
    data_[size_++] = c;
  }

  FORCE_INLINE void append(const char* str) { append(str, strlen(str)); }

  FORCE_INLINE void append(std::string_view sv) {
    append(sv.data(), sv.size());
  }

  FORCE_INLINE void reset() { size_ = 0; }

  [[nodiscard]] FORCE_INLINE char* current() const {
    return data_.get() + size_;
  }

  [[nodiscard]] FORCE_INLINE size_t remaining() const {
    return capacity_ - size_;
  }

  [[nodiscard]] FORCE_INLINE const char* data() const { return data_.get(); }

  [[nodiscard]] FORCE_INLINE size_t size() const { return size_; }

  [[nodiscard]] FORCE_INLINE std::string_view view() const {
    return std::string_view(data_.get(), size_);
  }

  [[nodiscard]] FORCE_INLINE size_t capacity() const { return capacity_; }

  [[nodiscard]] FORCE_INLINE size_t& size() { return size_; }

 private:
  std::unique_ptr<char[]> data_;
  size_t capacity_;
  size_t size_;
};

template <typename BufferType>
class DeribitJsonRpc {
 public:
  static constexpr const char JSON_COMMA = ',';
  static constexpr const char JSON_COLON = ':';
  static constexpr const char JSON_QUOTE = '"';
  static constexpr const char JSON_OPEN_BRACE = '{';
  static constexpr const char JSON_CLOSE_BRACE = '}';
  static constexpr const char JSON_OPEN_BRACKET = '[';
  static constexpr const char JSON_CLOSE_BRACKET = ']';
  static constexpr const char* JSON_TRUE = "true";
  static constexpr const char* JSON_FALSE = "false";
  static constexpr const char* JSON_NULL = "null";
  static constexpr const char* JSON_RPC_VERSION = "\"jsonrpc\":\"2.0\"";
  static constexpr const char* JSON_METHOD = "\"method\":";
  static constexpr const char* JSON_ID = "\"id\":";
  static constexpr const char* JSON_PARAMS = "\"params\":";
  static constexpr size_t MAX_INT_CHARS = 32;

  explicit DeribitJsonRpc(BufferType& buffer)
      : buffer_(buffer), first_field_(true) {}

  FORCE_INLINE void begin_object() {
    buffer_.append(JSON_OPEN_BRACE);
    first_field_ = true;
  }

  FORCE_INLINE void end_object() { buffer_.append(JSON_CLOSE_BRACE); }

  FORCE_INLINE void begin_array() {
    buffer_.append(JSON_OPEN_BRACKET);
    first_field_ = true;
  }

  FORCE_INLINE void end_array() { buffer_.append(JSON_CLOSE_BRACKET); }

  FORCE_INLINE void serialize(const char* key, std::string_view value) {
    write_key(key);
    append_escaped_string(value);
  }

  FORCE_INLINE void serialize(const char* key, const std::string& value) {
    serialize(key, std::string_view(value));
  }

  FORCE_INLINE void serialize(const char* key, const char* value) {
    write_key(key);
    append_escaped_string(std::string_view(value));
  }

  FORCE_INLINE void append_escaped_string(std::string_view value) {
    buffer_.append(JSON_QUOTE);
    buffer_.append(value.data(), value.size());
    buffer_.append(JSON_QUOTE);
  }

  FORCE_INLINE void serialize(const char* key, double value) {
    write_key(key);
    char buf[MAX_INT_CHARS];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
    if (LIKELY(ec == std::errc())) {
      buffer_.append(buf, ptr - buf);
    }
  }

  FORCE_INLINE void serialize(const char* key, int64_t value) {
    write_key(key);
    char buf[MAX_INT_CHARS];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
    if (LIKELY(ec == std::errc())) {
      buffer_.append(buf, ptr - buf);
    }
  }

  FORCE_INLINE void serialize(const char* key, int value) {
    serialize(key, static_cast<int64_t>(value));
  }

  FORCE_INLINE void serialize(const char* key, bool value) {
    write_key(key);
    if (value) {
      buffer_.append(JSON_TRUE, 4);
    } else {
      buffer_.append(JSON_FALSE, 5);
    }
  }

  FORCE_INLINE void serialize_null(const char* key) {
    write_key(key);
    buffer_.append(JSON_NULL, 4);
  }

  FORCE_INLINE void begin_json_rpc(const char* method, int id) {
    begin_object();
    // Optimize common JSON-RPC fields by writing them directly
    if (!first_field_) buffer_.append(JSON_COMMA);
    first_field_ = false;
    buffer_.append(JSON_RPC_VERSION);

    buffer_.append(JSON_COMMA);
    buffer_.append(JSON_METHOD);
    buffer_.append(JSON_QUOTE);
    buffer_.append(method);
    buffer_.append(JSON_QUOTE);

    buffer_.append(JSON_COMMA);
    buffer_.append(JSON_ID);
    char id_buf[16];
    auto [ptr, ec] = std::to_chars(id_buf, id_buf + sizeof(id_buf), id);
    if (LIKELY(ec == std::errc())) {
      buffer_.append(id_buf, ptr - id_buf);
    }

    buffer_.append(JSON_COMMA);
    buffer_.append(JSON_PARAMS);
    begin_object();
  }

  FORCE_INLINE void end_json_rpc() {
    end_object();  // end params
    end_object();  // end rpc object
  }

 private:
  FORCE_INLINE void write_key(const char* key) {
    if (!first_field_) {
      buffer_.append(JSON_COMMA);
    } else {
      first_field_ = false;
    }
    buffer_.append(JSON_QUOTE);
    buffer_.append(key);
    buffer_.append(JSON_QUOTE);
    buffer_.append(JSON_COLON);
  }

  BufferType& buffer_;
  bool first_field_;
};

namespace deribit {
namespace fields {
static constexpr const char INSTRUMENT_NAME[] = "instrument_name";
static constexpr const char AMOUNT[] = "amount";
static constexpr const char PRICE[] = "price";
static constexpr const char TYPE[] = "type";
static constexpr const char LABEL[] = "label";
static constexpr const char ORDER_ID[] = "order_id";
static constexpr const char REDUCE_ONLY[] = "reduce_only";
static constexpr const char POST_ONLY[] = "post_only";
static constexpr const char TIME_IN_FORCE[] = "time_in_force";
static constexpr const char MAX_SHOW[] = "max_show";
}  // namespace fields

namespace methods {
static constexpr const char PRIVATE_BUY[] = "private/buy";
static constexpr const char PRIVATE_SELL[] = "private/sell";
static constexpr const char PRIVATE_EDIT[] = "private/edit";
static constexpr const char PRIVATE_CANCEL[] = "private/cancel";
static constexpr const char PRIVATE_GET_POSITIONS[] = "private/get_positions";
}  // namespace methods

namespace order_types {
static constexpr const char LIMIT[] = "limit";
static constexpr const char MARKET[] = "market";
static constexpr const char STOP_LIMIT[] = "stop_limit";
static constexpr const char STOP_MARKET[] = "stop_market";
}  // namespace order_types

namespace time_in_force {
static constexpr const char GTC[] = "good_til_cancelled";
static constexpr const char IOC[] = "immediate_or_cancel";
static constexpr const char FOK[] = "fill_or_kill";
}  // namespace time_in_force
}  // namespace deribit

struct ALIGNED(32) DeribitOrderRequest {
  std::string instrument_name;
  double amount;
  double price;
  std::string type;
  std::string label;
  bool reduce_only;
  bool post_only;
  std::string time_in_force;
  double max_show;
};

struct ALIGNED(32) DeribitEditRequest {
  std::string order_id;
  double amount;
  double price;
  bool post_only;
  double max_show;
};

struct ALIGNED(32) DeribitCancelRequest {
  std::string order_id;
};

// Schema pattern for field serialization
template <typename T, const char* Name, typename Type, Type T::* Member>
struct Field {
  static constexpr const char* name = Name;

  template <typename U>
  [[nodiscard]] static FORCE_INLINE auto get(const U& obj)
      -> decltype(static_cast<const T&>(obj).*Member) {
    return static_cast<const T&>(obj).*Member;
  }
};

// Schema pattern
template <typename... Fields>
struct Schema {
  template <typename T, typename BufferType>
  static FORCE_INLINE void serialize(const T& obj,
                                     DeribitJsonRpc<BufferType>& serializer) {
    serialize_fields<0>(obj, serializer);
  }

  template <size_t I, typename T, typename BufferType>
  static FORCE_INLINE void serialize_fields(
      const T& obj, DeribitJsonRpc<BufferType>& serializer) {
    if constexpr (I < sizeof...(Fields)) {
      using FieldType =
          typename std::tuple_element<I, std::tuple<Fields...>>::type;
      serializer.serialize(FieldType::name, FieldType::get(obj));
      serialize_fields<I + 1>(obj, serializer);
    }
  }
};

// Schema for Deribit requests
using BuySellSchema =
    Schema<Field<DeribitOrderRequest, deribit::fields::INSTRUMENT_NAME,
                 std::string, &DeribitOrderRequest::instrument_name>,
           Field<DeribitOrderRequest, deribit::fields::AMOUNT, double,
                 &DeribitOrderRequest::amount>,
           Field<DeribitOrderRequest, deribit::fields::PRICE, double,
                 &DeribitOrderRequest::price>,
           Field<DeribitOrderRequest, deribit::fields::TYPE, std::string,
                 &DeribitOrderRequest::type>,
           Field<DeribitOrderRequest, deribit::fields::LABEL, std::string,
                 &DeribitOrderRequest::label>,
           Field<DeribitOrderRequest, deribit::fields::REDUCE_ONLY, bool,
                 &DeribitOrderRequest::reduce_only>,
           Field<DeribitOrderRequest, deribit::fields::POST_ONLY, bool,
                 &DeribitOrderRequest::post_only>,
           Field<DeribitOrderRequest, deribit::fields::TIME_IN_FORCE,
                 std::string, &DeribitOrderRequest::time_in_force>,
           Field<DeribitOrderRequest, deribit::fields::MAX_SHOW, double,
                 &DeribitOrderRequest::max_show>>;

using EditSchema = Schema<Field<DeribitEditRequest, deribit::fields::ORDER_ID,
                                std::string, &DeribitEditRequest::order_id>,
                          Field<DeribitEditRequest, deribit::fields::AMOUNT,
                                double, &DeribitEditRequest::amount>,
                          Field<DeribitEditRequest, deribit::fields::PRICE,
                                double, &DeribitEditRequest::price>,
                          Field<DeribitEditRequest, deribit::fields::POST_ONLY,
                                bool, &DeribitEditRequest::post_only>,
                          Field<DeribitEditRequest, deribit::fields::MAX_SHOW,
                                double, &DeribitEditRequest::max_show>>;

using CancelSchema =
    Schema<Field<DeribitCancelRequest, deribit::fields::ORDER_ID, std::string,
                 &DeribitCancelRequest::order_id>>;

// Deribit API client class
class ALIGNED(64) DeribitClient {
 public:
  DeribitClient() : buffer_(8192), request_id_(1) {}

  // Prevent copies, allow moves
  DeribitClient(const DeribitClient&) = delete;
  DeribitClient& operator=(const DeribitClient&) = delete;
  DeribitClient(DeribitClient&&) noexcept = default;
  DeribitClient& operator=(DeribitClient&&) noexcept = default;

  // Create buy order JSON-RPC
  [[nodiscard]] FORCE_INLINE std::string_view create_buy_request(
      const DeribitOrderRequest& req) {
    buffer_.reset();
    DeribitJsonRpc<Buffer> rpc(buffer_);

    rpc.begin_json_rpc(deribit::methods::PRIVATE_BUY, request_id_++);
    BuySellSchema::serialize(req, rpc);
    rpc.end_json_rpc();

    return buffer_.view();
  }

  // Create sell order JSON-RPC
  [[nodiscard]] FORCE_INLINE std::string_view create_sell_request(
      const DeribitOrderRequest& req) {
    buffer_.reset();
    DeribitJsonRpc<Buffer> rpc(buffer_);

    rpc.begin_json_rpc(deribit::methods::PRIVATE_SELL, request_id_++);
    BuySellSchema::serialize(req, rpc);
    rpc.end_json_rpc();

    return buffer_.view();
  }

  // Create edit order JSON-RPC
  [[nodiscard]] FORCE_INLINE std::string_view create_edit_request(
      const DeribitEditRequest& req) {
    buffer_.reset();
    DeribitJsonRpc<Buffer> rpc(buffer_);

    rpc.begin_json_rpc(deribit::methods::PRIVATE_EDIT, request_id_++);
    EditSchema::serialize(req, rpc);
    rpc.end_json_rpc();

    return buffer_.view();
  }

  // Create cancel order JSON-RPC
  [[nodiscard]] FORCE_INLINE std::string_view create_cancel_request(
      const DeribitCancelRequest& req) {
    buffer_.reset();
    DeribitJsonRpc<Buffer> rpc(buffer_);

    rpc.begin_json_rpc(deribit::methods::PRIVATE_CANCEL, request_id_++);
    CancelSchema::serialize(req, rpc);
    rpc.end_json_rpc();

    return buffer_.view();
  }

  // Create get positions JSON-RPC
  [[nodiscard]] FORCE_INLINE std::string_view create_get_positions_request() {
    buffer_.reset();
    DeribitJsonRpc<Buffer> rpc(buffer_);

    rpc.begin_json_rpc(deribit::methods::PRIVATE_GET_POSITIONS, request_id_++);
    rpc.end_json_rpc();

    return buffer_.view();
  }

  // Manual serialization function for comparison with schema-based approach
  [[nodiscard]] FORCE_INLINE std::string_view create_buy_request_manual(
      const DeribitOrderRequest& req) {
    buffer_.reset();
    DeribitJsonRpc<Buffer> rpc(buffer_);

    rpc.begin_json_rpc(deribit::methods::PRIVATE_BUY, request_id_++);

    // Manually serialize each field (no schema)
    rpc.serialize(deribit::fields::INSTRUMENT_NAME, req.instrument_name);
    rpc.serialize(deribit::fields::AMOUNT, req.amount);
    rpc.serialize(deribit::fields::PRICE, req.price);
    rpc.serialize(deribit::fields::TYPE, req.type);
    rpc.serialize(deribit::fields::LABEL, req.label);
    rpc.serialize(deribit::fields::REDUCE_ONLY, req.reduce_only);
    rpc.serialize(deribit::fields::POST_ONLY, req.post_only);
    rpc.serialize(deribit::fields::TIME_IN_FORCE, req.time_in_force);
    rpc.serialize(deribit::fields::MAX_SHOW, req.max_show);

    rpc.end_json_rpc();

    return buffer_.view();
  }

 private:
  Buffer buffer_;
  int request_id_;
};

// Example of explicit template instantiation to help compiler optimize
template class DeribitJsonRpc<Buffer>;
template void
Schema<Field<DeribitOrderRequest, deribit::fields::INSTRUMENT_NAME, std::string,
             &DeribitOrderRequest::instrument_name>>::
    serialize(const DeribitOrderRequest&, DeribitJsonRpc<Buffer>&);

// Create realistic test data for benchmarks
struct TestData {
  static DeribitOrderRequest createOrderRequest() {
    return DeribitOrderRequest{.instrument_name = "BTC-PERPETUAL",
                               .amount = 100.0,
                               .price = 40000.0,
                               .type = deribit::order_types::LIMIT,
                               .label = "test_order",
                               .reduce_only = false,
                               .post_only = true,
                               .time_in_force = deribit::time_in_force::GTC,
                               .max_show = 100.0};
  }

  static DeribitEditRequest createEditRequest() {
    return DeribitEditRequest{.order_id = "1234567890abcdef",
                              .amount = 150.0,
                              .price = 40500.0,
                              .post_only = true,
                              .max_show = 150.0};
  }

  static DeribitCancelRequest createCancelRequest() {
    return DeribitCancelRequest{.order_id = "1234567890abcdef"};
  }

  // Create varied test data for string performance testing
  static std::string createRandomInstrumentName(size_t len) {
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ-0123456789";
    static std::mt19937 rng(42);  // Fixed seed for reproducibility
    static std::uniform_int_distribution<> dist(0, sizeof(alphabet) - 2);

    std::string result;
    result.reserve(len);
    for (size_t i = 0; i < len; ++i) {
      result.push_back(alphabet[dist(rng)]);
    }
    return result;
  }
};

// Benchmark appending small strings to Buffer
static void BM_BufferAppendSmallString(benchmark::State& state) {
  for (auto _ : state) {
    Buffer buffer(1024);
    for (int i = 0; i < 100; ++i) {
      buffer.append("small_string", 12);
    }
    benchmark::DoNotOptimize(buffer.data());
    // benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_BufferAppendSmallString);

// Benchmark appending a large string to Buffer with auto-resize
static void BM_BufferAppendLargeString(benchmark::State& state) {
  // Size of string to append
  const size_t size = state.range(0);

  for (auto _ : state) {
    state.PauseTiming();
    std::string large_string(size, 'X');
    Buffer buffer(64);  // Start with small buffer to test resize
    state.ResumeTiming();

    buffer.append(large_string.c_str(), large_string.size());
    benchmark::DoNotOptimize(buffer.data());
    // benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_BufferAppendLargeString)->RangeMultiplier(4)->Range(64, 16384);

// Benchmark serializing different types of values
static void BM_SerializeString(benchmark::State& state) {
  const size_t str_len = state.range(0);

  for (auto _ : state) {
    state.PauseTiming();
    std::string test_string = TestData::createRandomInstrumentName(str_len);
    Buffer buffer(1024);
    DeribitJsonRpc<Buffer> rpc(buffer);
    state.ResumeTiming();

    rpc.begin_object();
    rpc.serialize("test_key", test_string);
    rpc.end_object();

    benchmark::DoNotOptimize(buffer.data());
    // benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_SerializeString)->RangeMultiplier(2)->Range(8, 1024);

// Benchmark serializing numeric values
static void BM_SerializeNumeric(benchmark::State& state) {
  const double value = state.range(0) * 1.0;

  for (auto _ : state) {
    Buffer buffer(1024);
    DeribitJsonRpc<Buffer> rpc(buffer);

    rpc.begin_object();
    rpc.serialize("int_value", static_cast<int64_t>(value));
    rpc.serialize("double_value", value);
    rpc.end_object();

    benchmark::DoNotOptimize(buffer.data());
    // benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_SerializeNumeric)->RangeMultiplier(10)->Range(1, 1000000000);

// Benchmark schema-based serialization
static void BM_SchemaBasedSerialization(benchmark::State& state) {
  DeribitOrderRequest req = TestData::createOrderRequest();
  DeribitClient client;

  for (auto _ : state) {
    auto result = client.create_buy_request(req);
    benchmark::DoNotOptimize(result.data());
    // benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_SchemaBasedSerialization);

// Benchmark manual serialization
static void BM_ManualSerialization(benchmark::State& state) {
  DeribitOrderRequest req = TestData::createOrderRequest();
  DeribitClient client;

  for (auto _ : state) {
    auto result = client.create_buy_request_manual(req);
    benchmark::DoNotOptimize(result.data());
    // benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_ManualSerialization);

// Setup fixture for API benchmarks
class DeribitBenchmark : public benchmark::Fixture {
 public:
  void SetUp(const ::benchmark::State& state) {
    buy_req = TestData::createOrderRequest();
    edit_req = TestData::createEditRequest();
    cancel_req = TestData::createCancelRequest();
  }

  DeribitClient client;
  DeribitOrderRequest buy_req;
  DeribitEditRequest edit_req;
  DeribitCancelRequest cancel_req;
};

// Benchmark buy request
BENCHMARK_F(DeribitBenchmark, BM_BuyRequest)(benchmark::State& state) {
  for (auto _ : state) {
    auto result = client.create_buy_request(buy_req);
    benchmark::DoNotOptimize(result.data());
    // benchmark::ClobberMemory();
  }
}

// Benchmark sell request
BENCHMARK_F(DeribitBenchmark, BM_SellRequest)(benchmark::State& state) {
  for (auto _ : state) {
    auto result = client.create_sell_request(buy_req);
    benchmark::DoNotOptimize(result.data());
    // benchmark::ClobberMemory();
  }
}

// Benchmark edit request
BENCHMARK_F(DeribitBenchmark, BM_EditRequest)(benchmark::State& state) {
  for (auto _ : state) {
    auto result = client.create_edit_request(edit_req);
    benchmark::DoNotOptimize(result.data());
    // benchmark::ClobberMemory();
  }
}

// Benchmark cancel request
BENCHMARK_F(DeribitBenchmark, BM_CancelRequest)(benchmark::State& state) {
  for (auto _ : state) {
    auto result = client.create_cancel_request(cancel_req);
    benchmark::DoNotOptimize(result.data());
    // benchmark::ClobberMemory();
  }
}

// Benchmark get positions request
BENCHMARK_F(DeribitBenchmark, BM_GetPositionsRequest)(benchmark::State& state) {
  for (auto _ : state) {
    auto result = client.create_get_positions_request();
    benchmark::DoNotOptimize(result.data());
    // benchmark::ClobberMemory();
  }
}

// Measure latency distribution for order creation
static void BM_OrderLatencyPercentiles(benchmark::State& state) {
  const int iterations = 10000;
  std::vector<int64_t> latencies;
  latencies.reserve(iterations);

  DeribitOrderRequest req = TestData::createOrderRequest();
  DeribitClient client;

  for (auto _ : state) {
    state.PauseTiming();
    latencies.clear();
    state.ResumeTiming();

    for (int i = 0; i < iterations; ++i) {
      auto start = std::chrono::high_resolution_clock::now();

      auto result = client.create_buy_request(req);
      benchmark::DoNotOptimize(result.data());

      auto end = std::chrono::high_resolution_clock::now();
      auto duration =
          std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
              .count();
      latencies.push_back(duration);
    }

    // Sort to calculate percentiles
    std::sort(latencies.begin(), latencies.end());

    // Report percentiles
    state.counters["p50_ns"] = latencies[static_cast<size_t>(iterations * 0.5)];
    state.counters["p90_ns"] = latencies[static_cast<size_t>(iterations * 0.9)];
    state.counters["p99_ns"] =
        latencies[static_cast<size_t>(iterations * 0.99)];
    state.counters["p999_ns"] =
        latencies[static_cast<size_t>(iterations * 0.999)];
    state.counters["max_ns"] = latencies.back();
  }
}
BENCHMARK(BM_OrderLatencyPercentiles)->Iterations(3);

// Main function for the benchmark mode
int main(int argc, char** argv) {
#ifdef RUN_EXAMPLE
  // Run the example code if not in benchmark mode
  DeribitClient client;

  // Create a test buy request
  DeribitOrderRequest buy_req = TestData::createOrderRequest();

  // Test serialization
  auto buy_json = client.create_buy_request(buy_req);

  // Print output
  std::cout << "Buy request: " << buy_json << std::endl;

  return 0;
#else
  // Run the benchmark
  ::benchmark::Initialize(&argc, argv);
  if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
  ::benchmark::RunSpecifiedBenchmarks();
  ::benchmark::Shutdown();
  return 0;
#endif
}
