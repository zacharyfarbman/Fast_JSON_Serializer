#include <benchmark/benchmark.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string_view>

using SV = std::string_view;

#define FORCE_INLINE inline __attribute__((always_inline))

template <size_t Capacity>
class StaticBuffer {
 public:
  StaticBuffer() : size_(0) {}

  FORCE_INLINE void clear() { size_ = 0; }

  // Data access methods
  [[nodiscard]] FORCE_INLINE const char* data() const { return data_; }
  [[nodiscard]] FORCE_INLINE char* data() { return data_; }
  [[nodiscard]] FORCE_INLINE size_t size() const { return size_; }
  [[nodiscard]] FORCE_INLINE SV view() const { return {data_, size_}; }

  // Set buffer size directly
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
struct method_place_t {
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
// String type with maximum size
template <size_t N>
struct string {
  using type = SV;
  static constexpr size_t max_size = N;
};

// Number type with specific numeric type
template <typename T>
struct number {
  using type = T;
};

// Boolean type
struct boolean {
  using type = bool;
};

// Fixed key-value pair
template <typename Key>
struct fixed_key_value {
  using key_type = Key;
};

// Variable key-value pair
template <typename Key, typename ValueType>
struct key_value {
  using key_type = Key;
  using value_type = ValueType;
};

// Object type for nesting
template <typename... Fields>
struct object {};
}  // namespace schema

struct FieldOffset {
  size_t offset;  // Position in template
  size_t size;    // Size of placeholder
};

constexpr char TEMPLATE[] =
    R"({"jsonrpc":"2.0","method":"############","id":############,"params":{"access_token":"############","instrument_name":"############","amount":############,"label":############,"price":############,"post_only":############,"reject_post_only":############,"reduce_only":############,"time_in_force":"############"}})";

constexpr size_t TEMPLATE_SIZE = sizeof(TEMPLATE) - 1;

constexpr char CANCEL_TEMPLATE[] =
    R"({"jsonrpc":"2.0","method":"############","id":############,"params":{"access_token":"############","order_id":"############"}})";

constexpr size_t CANCEL_TEMPLATE_SIZE = sizeof(CANCEL_TEMPLATE) - 1;

constexpr char EDIT_TEMPLATE[] =
    R"({"jsonrpc":"2.0","method":"############","id":############,"params":{"access_token":"############","order_id":"############","amount":############,"price":############,"post_only":############,"reduce_only":############}})";

constexpr size_t EDIT_TEMPLATE_SIZE = sizeof(EDIT_TEMPLATE) - 1;

// Pre-calculated field offsets in template
constexpr FieldOffset METHOD_OFFSET = {27, 12};
constexpr FieldOffset ID_OFFSET = {48, 10};
constexpr FieldOffset ACCESS_TOKEN_OFFSET = {87, 12};
constexpr FieldOffset INSTRUMENT_OFFSET = {122, 12};
constexpr FieldOffset AMOUNT_OFFSET = {147, 12};
constexpr FieldOffset LABEL_OFFSET = {169, 12};
constexpr FieldOffset PRICE_OFFSET = {190, 12};
constexpr FieldOffset POST_ONLY_OFFSET = {216, 12};
constexpr FieldOffset REJECT_POST_ONLY_OFFSET = {249, 12};
constexpr FieldOffset REDUCE_ONLY_OFFSET = {277, 12};
constexpr FieldOffset TIME_IN_FORCE_OFFSET = {308, 12};
constexpr FieldOffset CANCEL_ORDER_ID_OFFSET = {114, 12};
constexpr FieldOffset EDIT_ORDER_ID_OFFSET = {114, 12};
constexpr FieldOffset EDIT_AMOUNT_OFFSET = {139, 12};
constexpr FieldOffset EDIT_PRICE_OFFSET = {160, 12};
constexpr FieldOffset EDIT_POST_ONLY_OFFSET = {186, 12};
constexpr FieldOffset EDIT_REDUCE_ONLY_OFFSET = {214, 12};

// Integer to string conversion
template <typename T>
FORCE_INLINE int int_to_str(char* buffer, T value) {
  // Handle zero case
  if (value == 0) {
    buffer[0] = '0';
    return 1;
  }

  // Handle negative numbers
  bool negative = false;
  if (value < 0) {
    negative = true;
    value = -value;
  }

  // Convert digits in reverse order
  char temp[32];
  int i = 0;
  while (value > 0) {
    temp[i++] = '0' + (value % 10);
    value /= 10;
  }

  // Add negative sign if needed
  int result_len = i + (negative ? 1 : 0);
  if (negative) {
    buffer[0] = '-';
  }

  // Reverse the digits
  for (int j = 0; j < i; j++) {
    buffer[(negative ? 1 : 0) + j] = temp[i - j - 1];
  }

  return result_len;
}

// Double to string conversion
FORCE_INLINE int double_to_str(char* buffer, double value) {
  // Handle integer part
  int64_t int_part = static_cast<int64_t>(value);
  int len = int_to_str(buffer, int_part);

  // Handle fractional part
  double frac_part = value - int_part;
  if (frac_part > 0.0001 || frac_part < -0.0001) {
    buffer[len++] = '.';

    // Only show one decimal place for simplicity
    // In a real implementation, this would handle proper precision
    int64_t frac_digit = static_cast<int64_t>(frac_part * 10);
    buffer[len++] = '0' + (frac_digit % 10);
  }

  return len;
}

template <typename Schema>
class Writer {
 public:
  explicit Writer(char* buffer) : buffer_(buffer) {}

  // Set methods for field writing
  template <typename Tag, typename T>
  FORCE_INLINE void set(const T& value) {
    write_field<Tag>(value);
  }

  template <typename ParentTag, typename ChildTag, typename T>
  FORCE_INLINE void set(const T& value) {
    write_nested_field<ParentTag, ChildTag>(value);
  }

 private:
  // Get offset for a top-level tag
  template <typename Tag>
  static constexpr FieldOffset get_field_offset() {
    if constexpr (std::is_same_v<Tag, method_place_t>) {
      return METHOD_OFFSET;
    } else if constexpr (std::is_same_v<Tag, request_id_t>) {
      return ID_OFFSET;
    } else {
      // Force compile error for unsupported tags
      static_assert(std::is_same_v<Tag, method_place_t> ||
                        std::is_same_v<Tag, request_id_t>,
                    "Unsupported field tag");
      return {0, 0};
    }
  }

  // Get offset for a nested tag
  template <typename ParentTag, typename ChildTag>
  static constexpr FieldOffset get_nested_offset() {
    if constexpr (std::is_same_v<ParentTag, params_t>) {
      if constexpr (std::is_same_v<ChildTag, access_token_t>) {
        return ACCESS_TOKEN_OFFSET;
      } else if constexpr (std::is_same_v<ChildTag, instrument_t>) {
        return INSTRUMENT_OFFSET;
      } else if constexpr (std::is_same_v<ChildTag, amount_t>) {
        return AMOUNT_OFFSET;
      } else if constexpr (std::is_same_v<ChildTag, label_t>) {
        return LABEL_OFFSET;
      } else if constexpr (std::is_same_v<ChildTag, price_t>) {
        return PRICE_OFFSET;
      } else if constexpr (std::is_same_v<ChildTag, post_only_t>) {
        return POST_ONLY_OFFSET;
      } else if constexpr (std::is_same_v<ChildTag, reject_post_only_t>) {
        return REJECT_POST_ONLY_OFFSET;
      } else if constexpr (std::is_same_v<ChildTag, reduce_only_t>) {
        return REDUCE_ONLY_OFFSET;
      } else if constexpr (std::is_same_v<ChildTag, time_in_force_t>) {
        return TIME_IN_FORCE_OFFSET;
      } else {
        // Force compile error for unsupported tags
        static_assert(!std::is_same_v<ParentTag, params_t>,
                      "Unsupported child tag for params");
        return {0, 0};
      }
    } else {
      // Force compile error for unsupported parent tags
      static_assert(!std::is_same_v<ParentTag, params_t>,
                    "Unsupported parent tag");
      return {0, 0};
    }
  }

  // Write field implementation
  template <typename Tag, typename T>
  FORCE_INLINE void write_field(const T& value) {
    FieldOffset offset = get_field_offset<Tag>();
    write_value(offset, value);
  }

  template <typename ParentTag, typename ChildTag, typename T>
  FORCE_INLINE void write_nested_field(const T& value) {
    FieldOffset offset = get_nested_offset<ParentTag, ChildTag>();
    write_value(offset, value);
  }

  // Type-specific value writers
  FORCE_INLINE void write_value(const FieldOffset& offset, SV value) {
    // Copy string value to the offset
    size_t copy_size = std::min(value.size(), offset.size);
    std::memcpy(buffer_ + offset.offset, value.data(), copy_size);

    // If the value is shorter than the placeholder, pad with spaces
    if (copy_size < offset.size) {
      std::memset(buffer_ + offset.offset + copy_size, ' ',
                  offset.size - copy_size);
    }
  }

  FORCE_INLINE void write_value(const FieldOffset& offset, const char* value) {
    write_value(offset, SV(value));
  }

  FORCE_INLINE void write_value(const FieldOffset& offset, int value) {
    // Convert to string first
    char temp[32];
    int len = int_to_str(temp, value);

    // If it fits within placeholder, right-align with spaces
    if ((size_t)len <= offset.size) {
      std::memset(buffer_ + offset.offset, ' ', offset.size - len);
      std::memcpy(buffer_ + offset.offset + offset.size - len, temp, len);
    } else {
      std::memcpy(buffer_ + offset.offset, temp,
                  std::min((size_t)len, offset.size));
    }
  }

  FORCE_INLINE void write_value(const FieldOffset& offset, uint64_t value) {
    char temp[32];
    int len = int_to_str(temp, value);

    if ((size_t)len <= offset.size) {
      std::memset(buffer_ + offset.offset, ' ', offset.size - len);
      std::memcpy(buffer_ + offset.offset + offset.size - len, temp, len);
    } else {
      std::memcpy(buffer_ + offset.offset, temp,
                  std::min((size_t)len, offset.size));
    }
  }

  FORCE_INLINE void write_value(const FieldOffset& offset, double value) {
    char temp[32];
    int len = double_to_str(temp, value);

    if ((size_t)len <= offset.size) {
      std::memset(buffer_ + offset.offset, ' ', offset.size - len);
      std::memcpy(buffer_ + offset.offset + offset.size - len, temp, len);
    } else {
      std::memcpy(buffer_ + offset.offset, temp,
                  std::min((size_t)len, offset.size));
    }
  }

  FORCE_INLINE void write_value(const FieldOffset& offset, bool value) {
    // Choose the proper string based on boolean value
    const char* str = value ? "true" : "false";
    size_t str_len = value ? 4 : 5;

    // Copy the string
    std::memcpy(buffer_ + offset.offset, str, std::min(str_len, offset.size));

    // If there's remaining space, pad with spaces
    if (str_len < offset.size) {
      std::memset(buffer_ + offset.offset + str_len, ' ',
                  offset.size - str_len);
    }
  }

  char* buffer_;
};

template <typename Schema, typename BufferType>
struct WriteImpl {
  static FORCE_INLINE SV write(BufferType& buffer, auto&& callback) {
    // Default implementation (for place_schema)
    buffer.clear();
    std::memcpy(buffer.data(), TEMPLATE, TEMPLATE_SIZE);
    buffer.set_size(TEMPLATE_SIZE);
    Writer<Schema> writer(buffer.data());
    callback(writer);
    return buffer.view();
  }
};

template <typename BufferType>
class Serializer {
 public:
  explicit Serializer(BufferType& buffer) : buffer_(buffer) {}

  template <typename Schema, typename Callback>
  FORCE_INLINE SV write(Callback&& callback) {
    // Use the helper struct to select the right implementation
    return WriteImpl<Schema, BufferType>::write(
        buffer_, std::forward<Callback>(callback));
  }

 private:
  BufferType& buffer_;
};

constexpr int METHOD_PLACE_SIZE = 12;
constexpr int ACCESS_TKN_SIZE = 400;
constexpr int INSTRUMENT_SIZE = 35;
constexpr int TIF_SIZE = 19;

// Forward declare the schema types
using place_schema = schema::object<
    schema::fixed_key_value<jsonrpc_t>,
    schema::key_value<method_place_t, schema::string<METHOD_PLACE_SIZE>>,
    schema::key_value<request_id_t, schema::number<uint64_t>>,
    schema::key_value<
        params_t,
        schema::object<
            schema::key_value<access_token_t, schema::string<ACCESS_TKN_SIZE>>,
            schema::key_value<instrument_t, schema::string<INSTRUMENT_SIZE>>,
            schema::key_value<amount_t, schema::number<double>>,
            schema::key_value<label_t, schema::number<uint64_t>>,
            schema::key_value<price_t, schema::number<double>>,
            schema::key_value<post_only_t, schema::boolean>,
            schema::key_value<reject_post_only_t, schema::boolean>,
            schema::key_value<reduce_only_t, schema::boolean>,
            schema::key_value<time_in_force_t, schema::string<TIF_SIZE>>>>>;

using cancel_schema = schema::object<
    schema::fixed_key_value<jsonrpc_t>,
    schema::key_value<method_place_t, schema::string<METHOD_PLACE_SIZE>>,
    schema::key_value<request_id_t, schema::number<uint64_t>>,
    schema::key_value<
        params_t,
        schema::object<
            schema::key_value<access_token_t, schema::string<ACCESS_TKN_SIZE>>,
            schema::key_value<order_id_t, schema::string<METHOD_PLACE_SIZE>>>>>;

using edit_schema = schema::object<
    schema::fixed_key_value<jsonrpc_t>,
    schema::key_value<method_place_t, schema::string<METHOD_PLACE_SIZE>>,
    schema::key_value<request_id_t, schema::number<uint64_t>>,
    schema::key_value<
        params_t,
        schema::object<
            schema::key_value<access_token_t, schema::string<ACCESS_TKN_SIZE>>,
            schema::key_value<order_id_t, schema::string<METHOD_PLACE_SIZE>>,
            schema::key_value<amount_t, schema::number<double>>,
            schema::key_value<price_t, schema::number<double>>,
            schema::key_value<post_only_t, schema::boolean>,
            schema::key_value<reduce_only_t, schema::boolean>>>>;

// Specialization for cancel_schema
template <typename BufferType>
struct WriteImpl<cancel_schema, BufferType> {
  static FORCE_INLINE SV write(BufferType& buffer, auto&& callback) {
    buffer.clear();
    std::memcpy(buffer.data(), CANCEL_TEMPLATE, CANCEL_TEMPLATE_SIZE);
    buffer.set_size(CANCEL_TEMPLATE_SIZE);
    Writer<cancel_schema> writer(buffer.data());
    callback(writer);
    return buffer.view();
  }
};

// Specialization for edit_schema
template <typename BufferType>
struct WriteImpl<edit_schema, BufferType> {
  static FORCE_INLINE SV write(BufferType& buffer, auto&& callback) {
    buffer.clear();
    std::memcpy(buffer.data(), EDIT_TEMPLATE, EDIT_TEMPLATE_SIZE);
    buffer.set_size(EDIT_TEMPLATE_SIZE);
    Writer<edit_schema> writer(buffer.data());
    callback(writer);
    return buffer.view();
  }
};

template <>
template <typename ParentTag, typename ChildTag>
constexpr FieldOffset Writer<cancel_schema>::get_nested_offset() {
  if constexpr (std::is_same_v<ParentTag, params_t>) {
    if constexpr (std::is_same_v<ChildTag, access_token_t>) {
      return ACCESS_TOKEN_OFFSET;  // Reuse existing offset
    } else if constexpr (std::is_same_v<ChildTag, order_id_t>) {
      return CANCEL_ORDER_ID_OFFSET;  // Cancel-specific offset
    } else {
      static_assert(!std::is_same_v<ParentTag, params_t>,
                    "Unsupported child tag for params in cancel schema");
      return {0, 0};
    }
  } else {
    static_assert(!std::is_same_v<ParentTag, params_t>,
                  "Unsupported parent tag for cancel schema");
    return {0, 0};
  }
}

template <>
template <typename ParentTag, typename ChildTag>
constexpr FieldOffset Writer<edit_schema>::get_nested_offset() {
  if constexpr (std::is_same_v<ParentTag, params_t>) {
    if constexpr (std::is_same_v<ChildTag, access_token_t>) {
      return ACCESS_TOKEN_OFFSET;  // Reuse existing offset
    } else if constexpr (std::is_same_v<ChildTag, order_id_t>) {
      return EDIT_ORDER_ID_OFFSET;
    } else if constexpr (std::is_same_v<ChildTag, amount_t>) {
      return EDIT_AMOUNT_OFFSET;
    } else if constexpr (std::is_same_v<ChildTag, price_t>) {
      return EDIT_PRICE_OFFSET;
    } else if constexpr (std::is_same_v<ChildTag, post_only_t>) {
      return EDIT_POST_ONLY_OFFSET;
    } else if constexpr (std::is_same_v<ChildTag, reduce_only_t>) {
      return EDIT_REDUCE_ONLY_OFFSET;
    } else {
      static_assert(!std::is_same_v<ParentTag, params_t>,
                    "Unsupported child tag for params in edit schema");
      return {0, 0};
    }
  } else {
    static_assert(!std::is_same_v<ParentTag, params_t>,
                  "Unsupported parent tag for edit schema");
    return {0, 0};
  }
}

static void BM_CancelOrderRequest(benchmark::State& state) {
  StaticBuffer<4096> buffer;
  Serializer<StaticBuffer<4096>> serializer(buffer);

  std::string endpoint = "private/cancel";
  uint64_t request_id = 42;
  std::string access_token = "thisismyreallylongaccesstokenstoredontheheap";
  std::string order_id = "ETH-281234";

  for (auto _ : state) {
    buffer.clear();

    serializer.write<cancel_schema>([&](auto& w) {
      w.template set<method_place_t>(endpoint);
      w.template set<request_id_t>(request_id);
      w.template set<params_t, access_token_t>(access_token);
      w.template set<params_t, order_id_t>(order_id);
    });

    benchmark::DoNotOptimize(buffer.data());
    benchmark::DoNotOptimize(buffer.size());
  }

  state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(buffer.size()));
}

static void BM_EditOrderRequest(benchmark::State& state) {
  StaticBuffer<4096> buffer;
  Serializer<StaticBuffer<4096>> serializer(buffer);

  std::string endpoint = "private/edit";
  uint64_t request_id = 43;
  std::string access_token = "thisismyreallylongaccesstokenstoredontheheap";
  std::string order_id = "ETH-281234";

  for (auto _ : state) {
    buffer.clear();

    serializer.write<edit_schema>([&](auto& w) {
      w.template set<method_place_t>(endpoint);
      w.template set<request_id_t>(request_id);
      w.template set<params_t, access_token_t>(access_token);
      w.template set<params_t, order_id_t>(order_id);
      w.template set<params_t, amount_t>(150.0);
      w.template set<params_t, price_t>(98765.0);
      w.template set<params_t, post_only_t>(true);
      w.template set<params_t, reduce_only_t>(false);
    });

    benchmark::DoNotOptimize(buffer.data());
    benchmark::DoNotOptimize(buffer.size());
  }

  state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(buffer.size()));
}

static void BM_PlaceOrderRequest(benchmark::State& state) {
  StaticBuffer<4096> buffer;
  Serializer<StaticBuffer<4096>> serializer(buffer);

  std::string endpoint = "private/buy";
  uint64_t request_id = 17;
  std::string access_token = "thisismyreallylongaccesstokenstoredontheheap";
  std::string ticker = "BTC-PERPETUAL";
  std::string time_in_force = "immediate_or_cancel";

  for (auto _ : state) {
    buffer.clear();

    serializer.write<place_schema>([&](auto& w) {
      w.template set<method_place_t>(endpoint);
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

    benchmark::DoNotOptimize(buffer.data());
    benchmark::DoNotOptimize(buffer.size());
  }

  state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(buffer.size()));
}

static void BM_ShortStringPayload(benchmark::State& state) {
  StaticBuffer<4096> buffer;
  Serializer<StaticBuffer<4096>> serializer(buffer);

  std::string endpoint = "private/buy";
  std::string access_token = "short_token";
  std::string ticker = "BTC-PERP";
  std::string time_in_force = "ioc";

  for (auto _ : state) {
    buffer.clear();
    serializer.template write<place_schema>([&](auto& w) {
      w.template set<method_place_t>(endpoint);
      w.template set<request_id_t>(17);
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
    benchmark::DoNotOptimize(buffer.data());
  }

  state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(buffer.size()));
}

static void BM_LongStringPayload(benchmark::State& state) {
  StaticBuffer<4096> buffer;
  Serializer<StaticBuffer<4096>> serializer(buffer);

  std::string endpoint = "private/buy";
  std::string access_token =
      "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
      "eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIy"
      "LCJleHAiOjE1MTYyMzkwMjIsImF1ZCI6ImRlcmliaXQuY29tIiwiaXNzIjoiZGVyaWJpdC5j"
      "b20ifQ.SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c";
  std::string ticker = "BTC-PERPETUAL-30SEP22-65000-C";
  std::string time_in_force = "good_til_cancelled_post_only";

  for (auto _ : state) {
    buffer.clear();
    serializer.template write<place_schema>([&](auto& w) {
      w.template set<method_place_t>(endpoint);
      w.template set<request_id_t>(17);
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
    benchmark::DoNotOptimize(buffer.data());
  }

  state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(buffer.size()));
}

static void BM_IntegerValues(benchmark::State& state) {
  StaticBuffer<4096> buffer;
  Serializer<StaticBuffer<4096>> serializer(buffer);

  std::string endpoint = "private/buy";
  uint64_t request_id = 9999999999;  // Large integer

  for (auto _ : state) {
    buffer.clear();
    serializer.template write<place_schema>([&](auto& w) {
      w.template set<method_place_t>(endpoint);
      w.template set<request_id_t>(request_id);
      w.template set<params_t, access_token_t>("token");
      w.template set<params_t, instrument_t>("BTC-PERPETUAL");
      w.template set<params_t, amount_t>(100);    // Integer amount
      w.template set<params_t, label_t>(999999);  // Large label
      w.template set<params_t, price_t>(100000);  // Integer price
      w.template set<params_t, post_only_t>(true);
      w.template set<params_t, reject_post_only_t>(false);
      w.template set<params_t, reduce_only_t>(false);
      w.template set<params_t, time_in_force_t>("ioc");
    });
    benchmark::DoNotOptimize(buffer.data());
  }

  state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(buffer.size()));
}

static void BM_FloatingPointValues(benchmark::State& state) {
  StaticBuffer<4096> buffer;
  Serializer<StaticBuffer<4096>> serializer(buffer);

  for (auto _ : state) {
    buffer.clear();
    serializer.template write<place_schema>([&](auto& w) {
      w.template set<method_place_t>("private/buy");
      w.template set<request_id_t>(17);
      w.template set<params_t, access_token_t>("token");
      w.template set<params_t, instrument_t>("BTC-PERPETUAL");
      w.template set<params_t, amount_t>(0.01234);  // Small fractional amount
      w.template set<params_t, label_t>(23);
      w.template set<params_t, price_t>(
          99993.12345);  // Price with many decimals
      w.template set<params_t, post_only_t>(true);
      w.template set<params_t, reject_post_only_t>(false);
      w.template set<params_t, reduce_only_t>(false);
      w.template set<params_t, time_in_force_t>("ioc");
    });
    benchmark::DoNotOptimize(buffer.data());
  }

  state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(buffer.size()));
}

template <size_t BufferSize>
static void BM_BufferCapacity(benchmark::State& state) {
  // Skip if buffer is too small
  if (BufferSize < TEMPLATE_SIZE) {
    state.SkipWithError("Buffer size too small for template: need at least " +
                        std::to_string(TEMPLATE_SIZE) + " bytes");
    return;
  }

  StaticBuffer<BufferSize> buffer;
  Serializer<StaticBuffer<BufferSize>> serializer(buffer);

  std::string endpoint = "private/buy";
  std::string access_token = "thisismyaccesstoken";

  for (auto _ : state) {
    buffer.clear();
    serializer.template write<place_schema>([&](auto& w) {
      w.template set<method_place_t>(endpoint);
      w.template set<request_id_t>(17);
      w.template set<params_t, access_token_t>(access_token);
      w.template set<params_t, instrument_t>("BTC-PERPETUAL");
      w.template set<params_t, amount_t>(100.0);
      w.template set<params_t, label_t>(23);
      w.template set<params_t, price_t>(99993.0);
      w.template set<params_t, post_only_t>(true);
      w.template set<params_t, reject_post_only_t>(false);
      w.template set<params_t, reduce_only_t>(false);
      w.template set<params_t, time_in_force_t>("ioc");
    });
    benchmark::DoNotOptimize(buffer.data());
  }

  state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(buffer.size()));
}

static void BM_EdgeCaseValues(benchmark::State& state) {
  StaticBuffer<4096> buffer;
  Serializer<StaticBuffer<4096>> serializer(buffer);

  for (auto _ : state) {
    buffer.clear();
    serializer.template write<place_schema>([&](auto& w) {
      w.template set<method_place_t>("");            // Empty method
      w.template set<request_id_t>(0);               // Zero ID
      w.template set<params_t, access_token_t>("");  // Empty token
      w.template set<params_t, instrument_t>("");    // Empty instrument
      w.template set<params_t, amount_t>(0.0);       // Zero amount
      w.template set<params_t, label_t>(0);          // Zero label
      w.template set<params_t, price_t>(0.0);        // Zero price
      w.template set<params_t, post_only_t>(false);
      w.template set<params_t, reject_post_only_t>(false);
      w.template set<params_t, reduce_only_t>(false);
      w.template set<params_t, time_in_force_t>("");  // Empty time in force
    });
    benchmark::DoNotOptimize(buffer.data());
  }

  state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(buffer.size()));
}

static void BM_RepeatedFieldUpdates(benchmark::State& state) {
  StaticBuffer<4096> buffer;
  Serializer<StaticBuffer<4096>> serializer(buffer);

  buffer.clear();
  serializer.template write<place_schema>([&](auto& w) {
    w.template set<method_place_t>("private/buy");
    w.template set<request_id_t>(17);
    w.template set<params_t, access_token_t>("token");
    w.template set<params_t, instrument_t>("BTC-PERPETUAL");
    w.template set<params_t, amount_t>(100.0);
    w.template set<params_t, label_t>(23);
    w.template set<params_t, price_t>(99993.0);
    w.template set<params_t, post_only_t>(true);
    w.template set<params_t, reject_post_only_t>(false);
    w.template set<params_t, reduce_only_t>(false);
    w.template set<params_t, time_in_force_t>("ioc");
  });

  // Benchmark just updating the price field repeatedly
  Writer<place_schema> writer(buffer.data());
  double price = 50000.0;

  for (auto _ : state) {
    // Update just the price field
    writer.template set<params_t, price_t>(price);
    price += 0.5;  // Small increment
    benchmark::DoNotOptimize(buffer.data());
  }

  state.SetBytesProcessed(int64_t(state.iterations()) * sizeof(double));
}

static void BM_MultipleSerialization(benchmark::State& state) {
  // Number of JSON messages to generate in sequence
  const int count = state.range(0);

  std::vector<StaticBuffer<4096>> buffers(count);
  std::vector<Serializer<StaticBuffer<4096>>> serializers;
  for (int i = 0; i < count; i++) {
    serializers.emplace_back(Serializer<StaticBuffer<4096>>(buffers[i]));
  }

  std::string endpoint = "private/buy";
  std::string access_token = "token";

  for (auto _ : state) {
    for (int i = 0; i < count; i++) {
      buffers[i].clear();

      serializers[i].template write<place_schema>([&](auto& w) {
        w.template set<method_place_t>(endpoint);
        w.template set<request_id_t>(i);  // Unique ID per message
        w.template set<params_t, access_token_t>(access_token);
        w.template set<params_t, instrument_t>("BTC-PERPETUAL");
        w.template set<params_t, amount_t>(100.0 + i);  // Unique amount
        w.template set<params_t, label_t>(23 + i);      // Unique label
        w.template set<params_t, price_t>(99993.0);
        w.template set<params_t, post_only_t>(true);
        w.template set<params_t, reject_post_only_t>(false);
        w.template set<params_t, reduce_only_t>(false);
        w.template set<params_t, time_in_force_t>("ioc");
      });

      benchmark::DoNotOptimize(buffers[i].data());
    }
  }

  // Count all bytes serialized across all messages
  state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(count) *
                          int64_t(buffers[0].size()));
  state.SetItemsProcessed(int64_t(state.iterations()) *
                          int64_t(count));  // Count messages as items
}

BENCHMARK(BM_PlaceOrderRequest);
BENCHMARK(BM_CancelOrderRequest);
BENCHMARK(BM_EditOrderRequest);
BENCHMARK(BM_ShortStringPayload);
BENCHMARK(BM_LongStringPayload);
BENCHMARK(BM_IntegerValues);
BENCHMARK(BM_FloatingPointValues);
BENCHMARK(BM_BufferCapacity<512>);
BENCHMARK(BM_BufferCapacity<1024>);
BENCHMARK(BM_BufferCapacity<4096>);
BENCHMARK(BM_BufferCapacity<16384>);
BENCHMARK(BM_EdgeCaseValues);
BENCHMARK(BM_RepeatedFieldUpdates);
BENCHMARK(BM_MultipleSerialization)->Arg(1)->Arg(5)->Arg(10)->Arg(50);

void demonstrate_place_order() {
  std::cout << "=== JSON Serialization Example ===" << std::endl;

  StaticBuffer<4096> buffer;
  Serializer<StaticBuffer<4096>> serializer(buffer);

  std::string endpoint = "private/buy";
  uint64_t request_id = 42;
  std::string access_token = "sample_access_token_123";
  std::string ticker = "BTC-PERPETUAL";
  std::string time_in_force = "immediate_or_cancel";

  auto serialized_json = serializer.write<place_schema>([&](auto& w) {
    w.template set<method_place_t>(endpoint);
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

  std::string json =
      R"({"jsonrpc":"2.0","method":")" + endpoint + R"(","id":)" +
      std::to_string(request_id) + R"(,"params":{"access_token":")" +
      access_token + R"(","instrument_name":")" + ticker + R"(","amount":)" +
      std::to_string(100.0) + R"(,"label":)" + std::to_string(23) +
      R"(,"price":)" + std::to_string(99993.0) + R"(,"post_only":)" +
      std::string(true ? "true" : "false") + R"(,"reject_post_only":)" +
      std::string(false ? "true" : "false") + R"(,"reduce_only":)" +
      std::string(false ? "true" : "false") + R"(,"time_in_force":")" +
      time_in_force + R"("}})";

  std::cout << "Serialized JSON:" << std::endl;
  std::cout << json << std::endl;

  std::cout << "\nSerialization Details:" << std::endl;
  std::cout << "Original Buffer Size: " << serialized_json.size() << " bytes"
            << std::endl;
  std::cout << "Cleaned JSON Size: " << json.size() << " bytes" << std::endl;
  std::cout << "\nTemplate Size: " << TEMPLATE_SIZE << " bytes" << std::endl;
}

void demonstrate_cancel_order() {
  std::cout << "\n=== Cancel Order Serialization Example ===" << std::endl;

  StaticBuffer<4096> buffer;
  Serializer<StaticBuffer<4096>> serializer(buffer);

  std::string endpoint = "private/cancel";
  uint64_t request_id = 42;
  std::string access_token = "sample_access_token_123";
  std::string order_id = "ETH-281234";

  auto serialized_json = serializer.write<cancel_schema>([&](auto& w) {
    w.template set<method_place_t>(endpoint);
    w.template set<request_id_t>(request_id);
    w.template set<params_t, access_token_t>(access_token);
    w.template set<params_t, order_id_t>(order_id);
  });

  std::string json = R"({"jsonrpc":"2.0","method":")" + endpoint +
                     R"(","id":)" + std::to_string(request_id) +
                     R"(,"params":{"access_token":")" + access_token +
                     R"(","order_id":")" + order_id + R"("}})";

  std::cout << "Serialized JSON:" << std::endl;
  std::cout << json << std::endl;

  std::cout << "\nSerialization Details:" << std::endl;
  std::cout << "Original Buffer Size: " << serialized_json.size() << " bytes"
            << std::endl;
  std::cout << "Cleaned JSON Size: " << json.size() << " bytes" << std::endl;
}

void demonstrate_edit_order() {
  std::cout << "\n=== Edit Order Serialization Example ===" << std::endl;

  StaticBuffer<4096> buffer;
  Serializer<StaticBuffer<4096>> serializer(buffer);

  std::string endpoint = "private/edit";
  uint64_t request_id = 43;
  std::string access_token = "sample_access_token_123";
  std::string order_id = "ETH-281234";

  auto serialized_json = serializer.write<edit_schema>([&](auto& w) {
    w.template set<method_place_t>(endpoint);
    w.template set<request_id_t>(request_id);
    w.template set<params_t, access_token_t>(access_token);
    w.template set<params_t, order_id_t>(order_id);
    w.template set<params_t, amount_t>(150.0);
    w.template set<params_t, price_t>(98765.0);
    w.template set<params_t, post_only_t>(true);
    w.template set<params_t, reduce_only_t>(false);
  });

  std::string json =
      R"({"jsonrpc":"2.0","method":")" + endpoint + R"(","id":)" +
      std::to_string(request_id) + R"(,"params":{"access_token":")" +
      access_token + R"(","order_id":")" + order_id + R"(","amount":)" +
      std::to_string(150.0) + R"(,"price":)" + std::to_string(98765.0) +
      R"(,"post_only":)" + std::string(true ? "true" : "false") +
      R"(,"reduce_only":)" + std::string(false ? "true" : "false") + R"("}})";

  std::cout << "Serialized JSON:" << std::endl;
  std::cout << json << std::endl;

  std::cout << "\nSerialization Details:" << std::endl;
  std::cout << "Original Buffer Size: " << serialized_json.size() << " bytes"
            << std::endl;
  std::cout << "Cleaned JSON Size: " << json.size() << " bytes" << std::endl;
}

int main(int argc, char** argv) {
  ::benchmark::Initialize(&argc, argv);

  ::benchmark::RunSpecifiedBenchmarks();

  demonstrate_place_order();
  demonstrate_cancel_order();
  demonstrate_edit_order();

  ::benchmark::Shutdown();
  return 0;
}
