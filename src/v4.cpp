#include <benchmark/benchmark.h>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

using RequestID = std::uint64_t;
using ClientOrderID = std::uint64_t;
using sv = std::string_view;

constexpr int METHOD_PLACE_SIZE = 12;
constexpr int ACCESS_TKN_SIZE = 400;
constexpr int INSTRUMENT_SIZE = 35;
constexpr int TIF_SIZE = 19;

#define FORCE_INLINE __attribute__((always_inline)) inline

template <size_t Capacity>
class StaticBuffer {
 public:
  StaticBuffer() : size_(0) {}

  FORCE_INLINE void clear() { size_ = 0; }

  [[nodiscard]] FORCE_INLINE const char* data() const { return data_; }
  [[nodiscard]] FORCE_INLINE char* data() { return data_; }
  [[nodiscard]] FORCE_INLINE size_t size() const { return size_; }
  [[nodiscard]] FORCE_INLINE sv view() const { return {data_, size_}; }

  FORCE_INLINE void set_size(size_t new_size) {
    if (new_size <= Capacity) {
      size_ = new_size;
    }
  }

 private:
  char data_[Capacity];
  size_t size_;
};

struct jsonrpc_t {
  constexpr static const char* name = "jsonrpc";
};
struct method_t {
  constexpr static const char* name = "method";
};
struct request_id_t {
  constexpr static const char* name = "id";
};
struct params_t {
  constexpr static const char* name = "params";
};
struct access_token_t {
  constexpr static const char* name = "access_token";
};
struct instrument_t {
  constexpr static const char* name = "instrument_name";
};
struct amount_t {
  constexpr static const char* name = "amount";
};
struct label_t {
  constexpr static const char* name = "label";
};
struct price_t {
  constexpr static const char* name = "price";
};
struct post_only_t {
  constexpr static const char* name = "post_only";
};
struct reject_post_only_t {
  constexpr static const char* name = "reject_post_only";
};
struct reduce_only_t {
  constexpr static const char* name = "reduce_only";
};
struct time_in_force_t {
  constexpr static const char* name = "time_in_force";
};
struct order_id_t {
  constexpr static const char* name = "order_id";
};

namespace schema {
template <size_t N>
struct string {
  using type = sv;
  static constexpr size_t max_size = N;
};

template <typename T>
struct number {
  using type = T;
};

struct boolean {
  using type = bool;
};

template <typename Key>
struct fixed_key_value {
  using key_type = Key;
};

template <typename Key, typename ValueType>
struct key_value {
  using key_type = Key;
  using value_type = ValueType;
};

template <typename... Fields>
struct object {};
}  // namespace schema

template <typename T>
FORCE_INLINE int int_to_str(char* buffer, T value) {
  if (value == 0) {
    buffer[0] = '0';
    return 1;
  }

  bool negative = false;
  if (value < 0) {
    negative = true;
    value = -value;
  }

  char temp[32];
  int i = 0;
  while (value > 0) {
    temp[i++] = '0' + (value % 10);
    value /= 10;
  }

  int result_len = i + (negative ? 1 : 0);
  if (negative) {
    buffer[0] = '-';
  }

  for (int j = 0; j < i; j++) {
    buffer[(negative ? 1 : 0) + j] = temp[i - j - 1];
  }

  return result_len;
}

FORCE_INLINE int double_to_str(char* buffer, double value) {
  int64_t int_part = static_cast<int64_t>(value);
  int len = int_to_str(buffer, int_part);

  double frac_part = value - int_part;
  if (frac_part > 0.0001 || frac_part < -0.0001) {
    buffer[len++] = '.';

    int64_t frac_digit = static_cast<int64_t>(frac_part * 10);
    buffer[len++] = '0' + (frac_digit % 10);
  }
  return len;
}

using place_schema = schema::object<
    schema::fixed_key_value<jsonrpc_t>,
    schema::key_value<method_t, schema::string<METHOD_PLACE_SIZE>>,
    schema::key_value<request_id_t, schema::number<RequestID>>,
    schema::key_value<
        params_t,
        schema::object<
            schema::key_value<access_token_t, schema::string<ACCESS_TKN_SIZE>>,
            schema::key_value<instrument_t, schema::string<INSTRUMENT_SIZE>>,
            schema::key_value<amount_t, schema::number<double>>,
            schema::key_value<label_t, schema::number<ClientOrderID>>,
            schema::key_value<price_t, schema::number<double>>,
            schema::key_value<post_only_t, schema::boolean>,
            schema::key_value<reject_post_only_t, schema::boolean>,
            schema::key_value<reduce_only_t, schema::boolean>,
            schema::key_value<time_in_force_t, schema::string<TIF_SIZE>>>>>;

using cancel_schema = schema::object<
    schema::fixed_key_value<jsonrpc_t>,
    schema::key_value<method_t, schema::string<METHOD_PLACE_SIZE>>,
    schema::key_value<request_id_t, schema::number<RequestID>>,
    schema::key_value<
        params_t,
        schema::object<
            schema::key_value<access_token_t, schema::string<ACCESS_TKN_SIZE>>,
            schema::key_value<order_id_t, schema::string<METHOD_PLACE_SIZE>>>>>;

using edit_schema = schema::object<
    schema::fixed_key_value<jsonrpc_t>,
    schema::key_value<method_t, schema::string<METHOD_PLACE_SIZE>>,
    schema::key_value<request_id_t, schema::number<RequestID>>,
    schema::key_value<
        params_t,
        schema::object<
            schema::key_value<access_token_t, schema::string<ACCESS_TKN_SIZE>>,
            schema::key_value<order_id_t, schema::string<METHOD_PLACE_SIZE>>,
            schema::key_value<amount_t, schema::number<double>>,
            schema::key_value<price_t, schema::number<double>>,
            schema::key_value<post_only_t, schema::boolean>,
            schema::key_value<reduce_only_t, schema::boolean>>>>;

template <typename Schema>
class Writer {
 public:
  explicit Writer(char* buffer, size_t& size)
      : buffer_(buffer),
        size_(size),
        wrote_jsonrpc_(false),
        wrote_method_(false),
        wrote_id_(false),
        started_params_(false),
        first_param_(true) {
    buffer_[size_++] = '{';
  }

  template <typename Tag, typename T>
  FORCE_INLINE void set(const T& value) {
    if constexpr (std::is_same_v<Tag, jsonrpc_t>) {
      write_kv("jsonrpc", "2.0", true);
      wrote_jsonrpc_ = true;
    } else if constexpr (std::is_same_v<Tag, method_t>) {
      write_kv("method", value, !wrote_jsonrpc_);
      wrote_method_ = true;
    } else if constexpr (std::is_same_v<Tag, request_id_t>) {
      write_kv("id", value, !wrote_jsonrpc_ && !wrote_method_);
      wrote_id_ = true;
    }
  }

  template <typename ParentTag, typename ChildTag, typename T>
  FORCE_INLINE void set(const T& value) {
    if constexpr (std::is_same_v<ParentTag, params_t>) {
      if (!started_params_) start_params();
      write_field(ChildTag::name, value, first_param_);
      first_param_ = false;
    }
  }

  FORCE_INLINE size_t finalize() {
    if (started_params_) {
      buffer_[size_++] = '}';
    }
    buffer_[size_++] = '}';
    return size_;
  }

  template <typename SchemaType>
  FORCE_INLINE void set_fixed_values() {
    set<jsonrpc_t>("2.0");
  }

 private:
  FORCE_INLINE void start_params() {
    add_comma_if_needed();
    write_key("params");
    buffer_[size_++] = '{';
    started_params_ = true;
    first_param_ = true;
  }

  template <typename T>
  FORCE_INLINE void write_kv(const char* key, const T& value, bool first) {
    add_comma_if_needed(!first);
    write_key(key);
    write_value(value);
  }

  FORCE_INLINE void write_key(const char* key) {
    buffer_[size_++] = '"';
    size_t key_len = strlen(key);
    std::memcpy(buffer_ + size_, key, key_len);
    size_ += key_len;
    buffer_[size_++] = '"';
    buffer_[size_++] = ':';
  }

  FORCE_INLINE void add_comma_if_needed(bool needed = true) {
    if (size_ <= 1) return;
    if (buffer_[size_ - 1] == '{' || buffer_[size_ - 1] == '[') return;
    if (needed) buffer_[size_++] = ',';
  }

  template <typename T>
  FORCE_INLINE void write_field(const char* name, const T& value, bool first) {
    add_comma_if_needed(!first);
    write_key(name);
    write_value(value);
  }

  FORCE_INLINE void write_value(const sv& value) {
    buffer_[size_++] = '"';
    std::memcpy(buffer_ + size_, value.data(), value.size());
    size_ += value.size();
    buffer_[size_++] = '"';
  }

  FORCE_INLINE void write_value(const char* value) { write_value(sv(value)); }

  FORCE_INLINE void write_value(const std::string& value) {
    write_value(sv(value));
  }

  FORCE_INLINE void write_value(int value) {
    char temp[32];
    int len = int_to_str(temp, value);
    std::memcpy(buffer_ + size_, temp, len);
    size_ += len;
  }

  FORCE_INLINE void write_value(uint64_t value) {
    char temp[32];
    int len = int_to_str(temp, value);
    std::memcpy(buffer_ + size_, temp, len);
    size_ += len;
  }

  FORCE_INLINE void write_value(double value) {
    char temp[32];
    int len = double_to_str(temp, value);
    std::memcpy(buffer_ + size_, temp, len);
    size_ += len;
  }

  FORCE_INLINE void write_value(bool value) {
    const char* str = value ? "true" : "false";
    size_t len = value ? 4 : 5;
    std::memcpy(buffer_ + size_, str, len);
    size_ += len;
  }

  char* buffer_;
  size_t& size_;
  bool wrote_jsonrpc_;
  bool wrote_method_;
  bool wrote_id_;
  bool started_params_;
  bool first_param_;
};

template <typename Schema, typename BufferType>
struct WriteImpl {
  static FORCE_INLINE sv write(BufferType& buffer, auto&& callback) {
    buffer.clear();
    size_t size = 0;

    Writer<Schema> writer(buffer.data(), size);
    writer.template set_fixed_values<Schema>();
    callback(writer);
    size = writer.finalize();
    buffer.set_size(size);

    return buffer.view();
  }
};

template <typename BufferType>
class Serializer {
 public:
  explicit Serializer(BufferType& buffer) : buffer_(buffer) {}

  template <typename Schema, typename Callback>
  FORCE_INLINE sv write(Callback&& callback) {
    return WriteImpl<Schema, BufferType>::write(
        buffer_, std::forward<Callback>(callback));
  }

 private:
  BufferType& buffer_;
};

void debug_print_json(sv json) {
  std::cout << "JSON size: " << json.size() << " bytes" << std::endl;
  std::cout << "Raw JSON: " << json << std::endl;

  std::cout << "Character-by-character:" << std::endl;
  for (size_t i = 0; i < json.size(); i++) {
    char c = json[i];
    if (i % 80 == 0) std::cout << std::endl << std::setw(4) << i << ": ";

    if (c == ' ')
      std::cout << '.';
    else if (c == '#')
      std::cout << '#';
    else
      std::cout << c;
  }
  std::cout << std::endl;
}

void verify_json_serialization() {
  StaticBuffer<4096> buffer;
  Serializer serializer(buffer);

  std::string access_token = "thisismyreallylongaccesstokenstoredontheheap";
  uint64_t request_id = 17;

  std::cout << "\n======== PLACE ORDER TEST ========\n";

  std::string place_endpoint = "private/buy";
  std::string ticker = "BTC-PERPETUAL";
  std::string time_in_force = "immediate_or_cancel";

  auto place_json = serializer.write<place_schema>([&](auto& w) {
    w.template set<method_t>(place_endpoint);
    w.template set<request_id_t>(request_id);
    w.template set<params_t, access_token_t>(access_token);
    w.template set<params_t, instrument_t>(ticker);
    w.template set<params_t, amount_t>(100.0);
    w.template set<params_t, label_t>(23);
    w.template set<params_t, price_t>(99993.0);
    w.template set<params_t, post_only_t>(true);
    w.template set<params_t, reject_post_only_t>(false);
    w.template set<params_t, reduce_only_t>(false);
    w.template set<params_t, time_in_force_t>(time_in_force);
  });

  std::cout << place_json << std::endl;
  debug_print_json(place_json);

  std::cout << "\n======== CANCEL ORDER TEST ========\n";

  buffer.clear();

  std::string cancel_endpoint = "private/cancel";
  std::string order_id = "ETH-349223";

  auto cancel_json = serializer.write<cancel_schema>([&](auto& w) {
    w.template set<method_t>(cancel_endpoint);
    w.template set<request_id_t>(request_id);
    w.template set<params_t, access_token_t>(access_token);
    w.template set<params_t, order_id_t>(order_id);
  });

  std::cout << cancel_json << std::endl;
  debug_print_json(cancel_json);

  std::cout << "\n======== EDIT ORDER TEST ========\n";

  buffer.clear();

  std::string edit_endpoint = "private/edit";
  std::string edit_order_id = "BTC-781456";

  auto edit_json = serializer.write<edit_schema>([&](auto& w) {
    w.template set<method_t>(edit_endpoint);
    w.template set<request_id_t>(request_id);
    w.template set<params_t, access_token_t>(access_token);
    w.template set<params_t, order_id_t>(edit_order_id);
    w.template set<params_t, amount_t>(75.5);
    w.template set<params_t, price_t>(98750.0);
    w.template set<params_t, post_only_t>(false);
    w.template set<params_t, reduce_only_t>(true);
  });

  std::cout << edit_json << std::endl;
  debug_print_json(edit_json);
}

void verify_json_dynamic_length() {
  StaticBuffer<4096> buffer;
  Serializer serializer(buffer);

  std::cout << "\n======== TESTING VARYING PARAMETER LENGTHS ========\n";

  struct TestCase {
    std::string description;
    std::string method;
    uint64_t request_id;
    std::string access_token;
    std::string instrument;
    double amount;
    uint64_t label;
    double price;
    std::string order_id;
    std::string time_in_force;
  };

  TestCase test_cases[] = {
      {
          "Short values test",
          "buy",  // Short method
          1,      // Single digit request ID
          "tk",   // Very short token
          "BTC",  // Short instrument
          0.1,    // Small amount
          5,      // Small label
          1.0,    // Small price
          "A1",   // Very short order ID
          "ioc"   // Short time_in_force
      },
      {
          "Exactly 12 chars test",  // Some placeholders are 12 characters
          "private/buy1",           // Exactly 12 chars
          123456789012,             // 12 digits
          "token12chars",           // Exactly 12 chars
          "BTC-123456789",          // Longer instrument
          12345.67890,              // More decimal places
          9876543210,               // 10 digits
          98765.43210,              // More decimal places
          "ORDER-123456",           // Exactly 12 chars
          "exactly_twelve"          // Exactly 12 chars
      },
      {
          "Longer values test",
          "private/buy/extended/endpoint",  // Long method
          9999999999999999999ULL,           // Very large request ID
          "this_is_a_very_long_access_token_that_exceeds_the_placeholder_"
          "length_substantially_to_test_dynamic_sizing",
          "EXTENDED-INSTRUMENT-NAME-WITH-EXTRA-DETAILS-20230324",  // Long
                                                                   // instrument
          123456789.123456789,  // Large amount with many decimals
          987654321098765ULL,   // Large label
          9999999.9999999,      // Large price with many decimals
          "ORDER-ID-WITH-EXTENDED-INFORMATION-12345-ABCDE",  // Long order ID
          "complex_time_in_force_with_extended_parameters"   // Long
                                                             // time_in_force
      }};

  for (const auto& test : test_cases) {
    std::cout << "\n--- " << test.description << " ---\n";

    std::cout << "Place Order JSON:\n";
    buffer.clear();
    auto place_json = serializer.write<place_schema>([&](auto& w) {
      w.template set<method_t>(test.method);
      w.template set<request_id_t>(test.request_id);
      w.template set<params_t, access_token_t>(test.access_token);
      w.template set<params_t, instrument_t>(test.instrument);
      w.template set<params_t, amount_t>(test.amount);
      w.template set<params_t, label_t>(test.label);
      w.template set<params_t, price_t>(test.price);
      w.template set<params_t, post_only_t>(true);
      w.template set<params_t, reject_post_only_t>(false);
      w.template set<params_t, reduce_only_t>(true);
      w.template set<params_t, time_in_force_t>(test.time_in_force);
    });
    std::cout << place_json << std::endl;

    std::cout << "Cancel Order JSON:\n";
    buffer.clear();
    auto cancel_json = serializer.write<cancel_schema>([&](auto& w) {
      w.template set<method_t>(test.method);
      w.template set<request_id_t>(test.request_id);
      w.template set<params_t, access_token_t>(test.access_token);
      w.template set<params_t, order_id_t>(test.order_id);
    });
    std::cout << cancel_json << std::endl;

    std::cout << "Edit Order JSON:\n";
    buffer.clear();
    auto edit_json = serializer.write<edit_schema>([&](auto& w) {
      w.template set<method_t>(test.method);
      w.template set<request_id_t>(test.request_id);
      w.template set<params_t, access_token_t>(test.access_token);
      w.template set<params_t, order_id_t>(test.order_id);
      w.template set<params_t, amount_t>(test.amount);
      w.template set<params_t, price_t>(test.price);
      w.template set<params_t, post_only_t>(true);
      w.template set<params_t, reduce_only_t>(false);
    });
    std::cout << edit_json << std::endl;
  }
}

static void BM_PlaceOrderSerialization(benchmark::State& state) {
  std::string endpoint = "private/buy";
  uint64_t request_id = 17;
  std::string access_token = "thisismyreallylongaccesstokenstoredontheheap";
  std::string ticker = "BTC-PERPETUAL";
  std::string time_in_force = "immediate_or_cancel";

  for (auto _ : state) {
    StaticBuffer<4096> buffer;
    Serializer serializer(buffer);

    auto json = serializer.write<place_schema>([&](auto& w) {
      w.template set<method_t>(endpoint);
      w.template set<request_id_t>(request_id);
      w.template set<params_t, access_token_t>(access_token);
      w.template set<params_t, instrument_t>(ticker);
      w.template set<params_t, amount_t>(100.0);
      w.template set<params_t, label_t>(23);
      w.template set<params_t, price_t>(99993.0);
      w.template set<params_t, post_only_t>(true);
      w.template set<params_t, reject_post_only_t>(false);
      w.template set<params_t, reduce_only_t>(false);
      w.template set<params_t, time_in_force_t>(time_in_force);
    });

    benchmark::DoNotOptimize(json);
  }
}

static void BM_CancelOrderSerialization(benchmark::State& state) {
  std::string endpoint = "private/cancel";
  uint64_t request_id = 17;
  std::string access_token = "thisismyreallylongaccesstokenstoredontheheap";
  std::string order_id = "ETH-349223";

  for (auto _ : state) {
    StaticBuffer<4096> buffer;
    Serializer serializer(buffer);

    auto json = serializer.write<cancel_schema>([&](auto& w) {
      w.template set<method_t>(endpoint);
      w.template set<request_id_t>(request_id);
      w.template set<params_t, access_token_t>(access_token);
      w.template set<params_t, order_id_t>(order_id);
    });

    benchmark::DoNotOptimize(json);
  }
}

static void BM_EditOrderSerialization(benchmark::State& state) {
  std::string endpoint = "private/edit";
  uint64_t request_id = 17;
  std::string access_token = "thisismyreallylongaccesstokenstoredontheheap";
  std::string order_id = "BTC-781456";

  for (auto _ : state) {
    StaticBuffer<4096> buffer;
    Serializer serializer(buffer);

    auto json = serializer.write<edit_schema>([&](auto& w) {
      w.template set<method_t>(endpoint);
      w.template set<request_id_t>(request_id);
      w.template set<params_t, access_token_t>(access_token);
      w.template set<params_t, order_id_t>(order_id);
      w.template set<params_t, amount_t>(75.5);
      w.template set<params_t, price_t>(98750.0);
      w.template set<params_t, post_only_t>(false);
      w.template set<params_t, reduce_only_t>(true);
    });

    benchmark::DoNotOptimize(json);
  }
}

static void BM_StringLengthImpact(benchmark::State& state) {
  std::string access_token(state.range(0), 'a');
  uint64_t request_id = 17;
  std::string endpoint = "private/buy";
  std::string ticker = "BTC-PERPETUAL";
  std::string time_in_force = "immediate_or_cancel";

  for (auto _ : state) {
    StaticBuffer<8192> buffer;
    Serializer serializer(buffer);

    auto json = serializer.write<place_schema>([&](auto& w) {
      w.template set<method_t>(endpoint);
      w.template set<request_id_t>(request_id);
      w.template set<params_t, access_token_t>(access_token);
      w.template set<params_t, instrument_t>(ticker);
      w.template set<params_t, amount_t>(100.0);
      w.template set<params_t, label_t>(23);
      w.template set<params_t, price_t>(99993.0);
      w.template set<params_t, post_only_t>(true);
      w.template set<params_t, reject_post_only_t>(false);
      w.template set<params_t, reduce_only_t>(false);
      w.template set<params_t, time_in_force_t>(time_in_force);
    });

    benchmark::DoNotOptimize(json);
  }

  state.SetItemsProcessed(state.iterations() * state.range(0));
  state.SetLabel(std::to_string(state.range(0)) + " chars");
}

static void BM_NumericPrecisionImpact(benchmark::State& state) {
  int precision = state.range(0);

  double price = 99990.0;
  double fraction = 0.0;

  for (int i = 1; i <= precision; i++) {
    fraction += (9.0 / std::pow(10, i));  // Add 0.9, 0.09, 0.009, etc.
  }
  price += fraction;

  uint64_t request_id = 17;
  std::string access_token = "token";
  std::string endpoint = "private/buy";
  std::string ticker = "BTC-PERPETUAL";

  for (auto _ : state) {
    StaticBuffer<4096> buffer;
    Serializer serializer(buffer);

    auto json = serializer.write<place_schema>([&](auto& w) {
      w.template set<method_t>(endpoint);
      w.template set<request_id_t>(request_id);
      w.template set<params_t, access_token_t>(access_token);
      w.template set<params_t, instrument_t>(ticker);
      w.template set<params_t, amount_t>(100.0);
      w.template set<params_t, label_t>(23);
      w.template set<params_t, price_t>(price);
      w.template set<params_t, post_only_t>(true);
      w.template set<params_t, reject_post_only_t>(false);
      w.template set<params_t, reduce_only_t>(false);
    });

    benchmark::DoNotOptimize(json);
  }

  state.SetLabel(std::to_string(precision) + " decimal places");
}

static void BM_BufferReuse(benchmark::State& state) {
  StaticBuffer<4096> buffer;
  Serializer serializer(buffer);

  std::string endpoint = "private/buy";
  uint64_t request_id = 17;
  std::string access_token = "thisismyreallylongaccesstokenstoredontheheap";
  std::string ticker = "BTC-PERPETUAL";
  std::string time_in_force = "immediate_or_cancel";

  for (auto _ : state) {
    buffer.clear();

    auto json = serializer.write<place_schema>([&](auto& w) {
      w.template set<method_t>(endpoint);
      w.template set<request_id_t>(request_id);
      w.template set<params_t, access_token_t>(access_token);
      w.template set<params_t, instrument_t>(ticker);
      w.template set<params_t, amount_t>(100.0);
      w.template set<params_t, label_t>(23);
      w.template set<params_t, price_t>(99993.0);
      w.template set<params_t, post_only_t>(true);
      w.template set<params_t, reject_post_only_t>(false);
      w.template set<params_t, reduce_only_t>(false);
      w.template set<params_t, time_in_force_t>(time_in_force);
    });

    benchmark::DoNotOptimize(json);
  }
}

static void BM_BufferRecreate(benchmark::State& state) {
  std::string endpoint = "private/buy";
  uint64_t request_id = 17;
  std::string access_token = "thisismyreallylongaccesstokenstoredontheheap";
  std::string ticker = "BTC-PERPETUAL";
  std::string time_in_force = "immediate_or_cancel";

  for (auto _ : state) {
    StaticBuffer<4096> buffer;
    Serializer serializer(buffer);

    auto json = serializer.write<place_schema>([&](auto& w) {
      w.template set<method_t>(endpoint);
      w.template set<request_id_t>(request_id);
      w.template set<params_t, access_token_t>(access_token);
      w.template set<params_t, instrument_t>(ticker);
      w.template set<params_t, amount_t>(100.0);
      w.template set<params_t, label_t>(23);
      w.template set<params_t, price_t>(99993.0);
      w.template set<params_t, post_only_t>(true);
      w.template set<params_t, reject_post_only_t>(false);
      w.template set<params_t, reduce_only_t>(false);
      w.template set<params_t, time_in_force_t>(time_in_force);
    });

    benchmark::DoNotOptimize(json);
  }
}

static void BM_BatchOrders(benchmark::State& state) {
  int batch_size = state.range(0);

  std::string endpoint = "private/buy";
  std::string access_token = "thisismyreallylongaccesstokenstoredontheheap";
  std::string ticker = "BTC-PERPETUAL";
  std::string time_in_force = "immediate_or_cancel";

  std::vector<uint64_t> request_ids(batch_size);
  for (int i = 0; i < batch_size; i++) {
    request_ids[i] = 1000 + i;
  }

  StaticBuffer<65536> buffer;
  Serializer serializer(buffer);

  for (auto _ : state) {
    std::vector<sv> results;
    results.reserve(batch_size);

    for (int i = 0; i < batch_size; i++) {
      buffer.clear();

      auto json = serializer.write<place_schema>([&](auto& w) {
        w.template set<method_t>(endpoint);
        w.template set<request_id_t>(request_ids[i]);
        w.template set<params_t, access_token_t>(access_token);
        w.template set<params_t, instrument_t>(ticker);
        w.template set<params_t, amount_t>(100.0 + i);
        w.template set<params_t, label_t>(23 + i);
        w.template set<params_t, price_t>(99990.0 + i);
        w.template set<params_t, post_only_t>(i % 2 == 0);
        w.template set<params_t, reject_post_only_t>(i % 3 == 0);
        w.template set<params_t, reduce_only_t>(i % 4 == 0);
        w.template set<params_t, time_in_force_t>(time_in_force);
      });

      benchmark::DoNotOptimize(json);
    }
  }

  state.SetItemsProcessed(state.iterations() * batch_size);
  state.SetLabel(std::to_string(batch_size) + " orders");
}

BENCHMARK(BM_PlaceOrderSerialization);
BENCHMARK(BM_CancelOrderSerialization);
BENCHMARK(BM_EditOrderSerialization);
BENCHMARK(BM_StringLengthImpact)->Range(8, 1 << 12);
BENCHMARK(BM_NumericPrecisionImpact)->DenseRange(0, 9, 1);
BENCHMARK(BM_BufferReuse);
BENCHMARK(BM_BufferRecreate);
BENCHMARK(BM_BatchOrders)->Range(1, 1 << 10);

int main(int argc, char** argv) {
  verify_json_serialization();
  verify_json_dynamic_length();
  ::benchmark::Initialize(&argc, argv);
  ::benchmark::RunSpecifiedBenchmarks();
  return 0;
}
