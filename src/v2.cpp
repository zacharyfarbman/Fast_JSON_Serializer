
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <deque>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

class Buffer {
 public:
  explicit Buffer(size_t capacity)
      : data_(std::make_unique<char[]>(capacity)),
        capacity_(capacity),
        size_(0) {}

  void reserve(size_t new_capacity) {
    if (new_capacity <= capacity_) {
      return;
    }

    auto new_data = std::make_unique<char[]>(new_capacity);
    std::memcpy(new_data.get(), data_.get(), size_);
    data_ = std::move(new_data);
    capacity_ = new_capacity;
  }

  void append(const char* str, size_t len) {
    if (size_ + len > capacity_) {
      reserve((size_ + len) * 2);
    }
    std::memcpy(data_.get() + size_, str, len);
    size_ += len;
  }

  void append(char c) {
    if (size_ + 1 > capacity_) {
      reserve(capacity_ * 2);
    }
    data_[size_++] = c;
  }

  void append(const char* str) { append(str, strlen(str)); }
  void append(std::string_view sv) { append(sv.data(), sv.size()); }

  void reset() { size_ = 0; }

  char* current() { return data_.get() + size_; }
  size_t remaining() const { return capacity_ - size_; }
  const char* data() const { return data_.get(); }
  size_t size() const { return size_; }
  std::string_view view() const { return std::string_view(data_.get(), size_); }

 private:
  std::unique_ptr<char[]> data_;
  size_t capacity_;
  size_t size_;
};

template <typename BufferType>
class DeribitJsonRpc {
 public:
  explicit DeribitJsonRpc(BufferType& buffer)
      : buffer_(buffer), first_field_(true) {}

  void begin_object() {
    buffer_.append('{');
    first_field_ = true;
  }

  void end_object() { buffer_.append('}'); }

  void begin_array() {
    buffer_.append('[');
    first_field_ = true;
  }

  void end_array() { buffer_.append(']'); }

  void append_escaped_string(std::string_view value) {
    buffer_.append('"');
    for (char c : value) {
      switch (c) {
        case '"':
          buffer_.append("\\\"", 2);
          break;
        case '\\':
          buffer_.append("\\\\", 2);
          break;
        case '\n':
          buffer_.append("\\n", 2);
          break;
        case '\r':
          buffer_.append("\\r", 2);
          break;
        case '\t':
          buffer_.append("\\t", 2);
          break;
        default:
          buffer_.append(c);
      }
    }
    buffer_.append('"');
  }

  void serialize(const char* key, std::string_view value) {
    write_key(key);
    append_escaped_string(value);
  }

  void serialize(const char* key, const std::string& value) {
    serialize(key, std::string_view(value));
  }

  void serialize(const char* key, const char* value) {
    write_key(key);
    append_escaped_string(std::string_view(value));
  }

  void serialize(const char* key, double value) {
    write_key(key);
    char buf[MAX_INT_CHARS];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
    if (ec == std::errc()) {
      buffer_.append(buf, ptr - buf);
    }
  }

  void serialize(const char* key, int64_t value) {
    write_key(key);
    char buf[MAX_INT_CHARS];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
    if (ec == std::errc()) {
      buffer_.append(buf, ptr - buf);
    }
  }

  void serialize(const char* key, int value) {
    serialize(key, static_cast<int64_t>(value));
  }

  void serialize(const char* key, bool value) {
    write_key(key);
    if (value) {
      buffer_.append("true", 4);
    } else {
      buffer_.append("false", 5);
    }
  }

  void serialize_null(const char* key) {
    write_key(key);
    buffer_.append("null", 4);
  }

  void begin_json_rpc(const char* method, int id) {
    begin_object();
    serialize("jsonrpc", "2.0");
    serialize("method", method);
    serialize("id", id);
    write_key("params");
    begin_object();
  }

  void end_json_rpc() {
    end_object();  // end params
    end_object();  // end rpc object
  }

 private:
  BufferType& buffer_;
  bool first_field_;
  static constexpr size_t MAX_INT_CHARS = 32;

  void write_key(const char* key) {
    if (!first_field_) {
      buffer_.append(',');
    } else {
      first_field_ = false;
    }
    buffer_.append('"');
    buffer_.append(key);
    buffer_.append('"');
    buffer_.append(':');
  }
};

namespace deribit {
namespace fields {
constexpr const char INSTRUMENT_NAME[] = "instrument_name";
constexpr const char AMOUNT[] = "amount";
constexpr const char PRICE[] = "price";
constexpr const char TYPE[] = "type";
constexpr const char LABEL[] = "label";
constexpr const char ORDER_ID[] = "order_id";
constexpr const char REDUCE_ONLY[] = "reduce_only";
constexpr const char POST_ONLY[] = "post_only";
constexpr const char TIME_IN_FORCE[] = "time_in_force";
constexpr const char MAX_SHOW[] = "max_show";
}  // namespace fields
namespace methods {
constexpr const char PRIVATE_BUY[] = "private/buy";
constexpr const char PRIVATE_SELL[] = "private/sell";
constexpr const char PRIVATE_EDIT[] = "private/edit";
constexpr const char PRIVATE_CANCEL[] = "private/cancel";
constexpr const char PRIVATE_GET_POSITIONS[] = "private/get_positions";
}  // namespace methods
namespace order_types {
constexpr const char LIMIT[] = "limit";
constexpr const char MARKET[] = "market";
constexpr const char STOP_LIMIT[] = "stop_limit";
constexpr const char STOP_MARKET[] = "stop_market";
}  // namespace order_types
namespace time_in_force {
constexpr const char GTC[] = "good_til_cancelled";
constexpr const char IOC[] = "immediate_or_cancel";
constexpr const char FOK[] = "fill_or_kill";
}  // namespace time_in_force
}  // namespace deribit

struct DeribitOrderRequest {
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

struct DeribitEditRequest {
  std::string order_id;
  double amount;
  double price;
  bool post_only;
  double max_show;
};

struct DeribitCancelRequest {
  std::string order_id;
};

// Schema pattern for field serialization
template <typename T, const char* Name, typename Type, Type T::* Member>
struct Field {
  static constexpr const char* name = Name;

  template <typename U>
  static auto get(const U& obj)
      -> decltype(static_cast<const T&>(obj).*Member) {
    return static_cast<const T&>(obj).*Member;
  }
};

// Schema pattern
template <typename... Fields>
struct Schema {
  template <typename T, typename BufferType>
  static void serialize(const T& obj, DeribitJsonRpc<BufferType>& serializer) {
    serialize_fields<0>(obj, serializer);
  }

  template <size_t I, typename T, typename BufferType>
  static void serialize_fields(const T& obj,
                               DeribitJsonRpc<BufferType>& serializer) {
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
class DeribitClient {
 public:
  DeribitClient() : buffer_(8192), request_id_(1) {}

  // create buy order JSON-RPC
  std::string_view create_buy_request(const DeribitOrderRequest& req) {
    buffer_.reset();
    DeribitJsonRpc<Buffer> rpc(buffer_);

    rpc.begin_json_rpc(deribit::methods::PRIVATE_BUY, request_id_++);
    BuySellSchema::serialize(req, rpc);
    rpc.end_json_rpc();

    return buffer_.view();
  }

  // create sell order JSON-RPC
  std::string_view create_sell_request(const DeribitOrderRequest& req) {
    buffer_.reset();
    DeribitJsonRpc<Buffer> rpc(buffer_);

    rpc.begin_json_rpc(deribit::methods::PRIVATE_SELL, request_id_++);
    BuySellSchema::serialize(req, rpc);
    rpc.end_json_rpc();

    return buffer_.view();
  }

  // create edit order JSON-RPC
  std::string_view create_edit_request(const DeribitEditRequest& req) {
    buffer_.reset();
    DeribitJsonRpc<Buffer> rpc(buffer_);

    rpc.begin_json_rpc(deribit::methods::PRIVATE_EDIT, request_id_++);
    EditSchema::serialize(req, rpc);
    rpc.end_json_rpc();

    return buffer_.view();
  }

  // create cancel order JSON-RPC
  std::string_view create_cancel_request(const DeribitCancelRequest& req) {
    buffer_.reset();
    DeribitJsonRpc<Buffer> rpc(buffer_);

    rpc.begin_json_rpc(deribit::methods::PRIVATE_CANCEL, request_id_++);
    CancelSchema::serialize(req, rpc);
    rpc.end_json_rpc();

    return buffer_.view();
  }

  std::string_view create_get_positions_request() {
    buffer_.reset();
    DeribitJsonRpc<Buffer> rpc(buffer_);

    rpc.begin_json_rpc(deribit::methods::PRIVATE_GET_POSITIONS, request_id_++);
    rpc.end_json_rpc();

    return buffer_.view();
  }

 private:
  Buffer buffer_;
  int request_id_;
};

int main() { return 0; }
