/*
 Small, safe and fast printf-like formatting library for C++
 Author: Victor Zverovich
 */

#ifndef FORMAT_H_
#define FORMAT_H_

#include <cstddef>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>

namespace format {

class FormatError : public std::runtime_error {
 public:
  FormatError(const std::string &message) : std::runtime_error(message) {}
};

class ArgFormatter;

template <typename Callback>
class ArgFormatterWithCallback;

// A buffer with the first SIZE elements stored in the object itself.
template <typename T, std::size_t SIZE>
class Buffer {
 private:
  std::size_t size_;
  std::size_t capacity_;
  T *ptr_;
  T data_[SIZE];

  void Grow(std::size_t size) {
    capacity_ = std::max(size, capacity_ + capacity_ / 2);
    T *p = new T[capacity_];
    std::copy(ptr_, ptr_ + size_, p);
    if (ptr_ != data_)
      delete [] ptr_;
    ptr_ = p;
  }

  // Do not implement!
  Buffer(const Buffer &);
  void operator=(const Buffer &);

 public:
  Buffer() : size_(0), capacity_(SIZE), ptr_(data_) {}
  ~Buffer() {
    if (ptr_ != data_) delete [] ptr_;
  }

  std::size_t size() const { return size_; }
  std::size_t capacity() const { return capacity_; }

  void resize(std::size_t size) {
    if (size > capacity_)
      Grow(size);
    size_ = size;
  }

  void reserve(std::size_t capacity) {
    if (capacity < capacity_)
      Grow(capacity - capacity_);
  }

  void push_back(const T &value) {
    if (size_ == capacity_)
      Grow(size_ + 1);
    ptr_[size_++] = value;
  }

  void append(const T *begin, const T *end) {
    std::ptrdiff_t size = end - begin;
    if (size_ + size > capacity_)
      Grow(size);
    std::copy(begin, end, ptr_ + size_);
    size_ += size;
  }

  T &operator[](std::size_t index) { return ptr_[index]; }
  const T &operator[](std::size_t index) const { return ptr_[index]; }

  const T &back() const { return ptr_[size_ - 1]; }
  T &back() { return ptr_[size_ - 1]; }

  void clear() { size_ = 0; }

  void Take(Buffer &other) {
    if (ptr_ != data_)
      delete [] ptr_;
    size_ = other.size_;
    if (other.ptr_ != data_) {
      ptr_ = other.ptr_;
      other.ptr_ = 0;
    } else {
      ptr_ = data_;
      std::copy(other.ptr_, other.ptr_ + size_, data_);
    }
  }
};

// A sprintf-like formatter that automatically allocates enough storage to
// fit all the output.
class Formatter {
 private:
  Buffer<char, 500> buffer_;  // Output buffer.

  enum Type {
    // Numeric types should go first.
    INT, UINT, LONG, ULONG, DOUBLE, LONG_DOUBLE,
    LAST_NUMERIC_TYPE = LONG_DOUBLE,
    CHAR, STRING, WSTRING, POINTER, CUSTOM
  };

  typedef void (Formatter::*FormatFunc)(const void *arg, int width);

 public:
  // A format argument.
  struct Arg {
    Type type;
    union {
      int int_value;
      unsigned uint_value;
      double double_value;
      long long_value;
      unsigned long ulong_value;
      long double long_double_value;
      const void *pointer_value;
      struct {
        const char *string_value;
        std::size_t size;
      };
      struct {
        const void *custom_value;
        FormatFunc format;
      };
    };

    Arg() {}
    explicit Arg(int value) : type(INT), int_value(value) {}
    explicit Arg(unsigned value) : type(UINT), uint_value(value) {}
    explicit Arg(long value) : type(LONG), long_value(value) {}
    explicit Arg(unsigned long value) : type(ULONG), ulong_value(value) {}
    explicit Arg(double value) : type(DOUBLE), double_value(value) {}
    explicit Arg(long double value)
    : type(LONG_DOUBLE), long_double_value(value) {}
    explicit Arg(char value) : type(CHAR), int_value(value) {}
    explicit Arg(const char *value, std::size_t size = 0)
    : type(STRING), string_value(value), size(size) {}
    explicit Arg(const void *value) : type(POINTER), pointer_value(value) {}
    explicit Arg(const void *value, FormatFunc f)
    : type(CUSTOM), custom_value(value), format(f) {}
  };

  Buffer<Arg, 10> args_;  // Format arguments.

  const char *format_;  // Format string.

  friend class ArgFormatter;

  void Add(const Arg &arg) {
    args_.push_back(arg);
  }

  // Formats an integer.
  template <typename T>
  void FormatInt(T value, unsigned flags, int width, char type);

  // Formats a floating point number (double or long double).
  template <typename T>
  void FormatDouble(
      T value, unsigned flags, int width, int precision, char type);

  // Formats an argument of a custom type, such as a user-defined class.
  template <typename T>
  void FormatCustomArg(const void *arg, int width);

  void Format();

  void Take(Formatter &f) {
    buffer_.Take(f.buffer_);
    args_.Take(f.args_);
  }

 public:
  Formatter() : format_(0) {}

  ArgFormatter operator()(const char *format);

  template <typename Callback>
  ArgFormatterWithCallback<Callback> FormatWithCallback(const char *format);

  std::size_t size() const { return buffer_.size(); }

  const char *data() const { return &buffer_[0]; }
  const char *c_str() const { return &buffer_[0]; }
};

class ArgFormatter {
 private:
  friend class Formatter;

  // This method is private to disallow formatting of arbitrary pointers.
  // If you want to output a pointer cast it to void*. Do not implement!
  template <typename T>
  ArgFormatter &operator<<(const T *value);

  // This method is private to disallow formatting of wide characters.
  // If you want to output a wide character cast it to integer type.
  // Do not implement!
  ArgFormatter &operator<<(wchar_t value);

 protected:
  mutable Formatter *formatter_;

  ArgFormatter(const ArgFormatter& other) : formatter_(other.formatter_) {
    other.formatter_ = 0;
  }

  ArgFormatter& operator=(const ArgFormatter& other) {
    formatter_ = other.formatter_;
    other.formatter_ = 0;
    return *this;
  }

  Formatter *FinishFormatting() const {
    Formatter *f = formatter_;
    if (f) {
      formatter_ = 0;
      f->Format();
    }
    return f;
  }

 public:
  explicit ArgFormatter(Formatter &f) : formatter_(&f) {}
  ~ArgFormatter();

  friend const char *c_str(const ArgFormatter &af) {
    return af.FinishFormatting()->c_str();
  }

  friend std::string str(const ArgFormatter &af) {
    return af.FinishFormatting()->c_str();
  }

  ArgFormatter &operator<<(int value) {
    formatter_->Add(Formatter::Arg(value));
    return *this;
  }

  ArgFormatter &operator<<(unsigned value) {
    formatter_->Add(Formatter::Arg(value));
    return *this;
  }

  ArgFormatter &operator<<(long value) {
    formatter_->Add(Formatter::Arg(value));
    return *this;
  }

  ArgFormatter &operator<<(unsigned long value) {
    formatter_->Add(Formatter::Arg(value));
    return *this;
  }

  ArgFormatter &operator<<(double value) {
    formatter_->Add(Formatter::Arg(value));
    return *this;
  }

  ArgFormatter &operator<<(long double value) {
    formatter_->Add(Formatter::Arg(value));
    return *this;
  }

  ArgFormatter &operator<<(char value) {
    formatter_->Add(Formatter::Arg(value));
    return *this;
  }

  ArgFormatter &operator<<(const char *value) {
    formatter_->Add(Formatter::Arg(value));
    return *this;
  }

  ArgFormatter &operator<<(const std::string &value) {
    formatter_->Add(Formatter::Arg(value.c_str(), value.size()));
    return *this;
  }

  ArgFormatter &operator<<(const void *value) {
    formatter_->Add(Formatter::Arg(value));
    return *this;
  }

  template <typename T>
  ArgFormatter &operator<<(T *value) {
    const T *const_value = value;
    return *this << const_value;
  }

  // If T is a pointer type, say "U*", AddPtrConst<T>::Value will be
  // "const U*". This additional const ensures that operator<<(const void *)
  // and not this method is called both for "const void*" and "void*".
  template <typename T>
  ArgFormatter &operator<<(const T &value) {
    formatter_->Add(Formatter::Arg(&value, &Formatter::FormatCustomArg<T>));
    return *this;
  }
};

template <typename Callback>
class ArgFormatterWithCallback : public ArgFormatter {
 public:
  explicit ArgFormatterWithCallback(Formatter &f) : ArgFormatter(f) {}

  ~ArgFormatterWithCallback() {
    if (!formatter_) return;
    Callback callback;
    callback(*formatter_);
  }
};

template <typename T>
void Formatter::FormatCustomArg(const void *arg, int width) {
  const T &value = *static_cast<const T*>(arg);
  std::ostringstream os;
  os << value;
  std::string str(os.str());
  std::size_t offset = buffer_.size();
  buffer_.resize(offset + std::max<std::size_t>(width, str.size()));
  char *out = &buffer_[offset];
  std::copy(str.begin(), str.end(), out);
  if (width > str.size())
    std::fill_n(out + str.size(), width - str.size(), ' ');
}

inline ArgFormatter Formatter::operator()(const char *format) {
  format_ = format;
  args_.clear();
  return ArgFormatter(*this);
}

template <typename Callback>
ArgFormatterWithCallback<Callback>
Formatter::FormatWithCallback(const char *format) {
  format_ = format;
  args_.clear();
  return ArgFormatterWithCallback<Callback>(*this);
}

class FullFormat : public ArgFormatter {
 private:
  mutable Formatter formatter_;

  // Do not implement.
  FullFormat& operator=(const FullFormat&);

 public:
  explicit FullFormat(const char *format) : ArgFormatter(formatter_) {
    ArgFormatter::operator=(formatter_(format));
  }

  FullFormat(const FullFormat& other) : ArgFormatter(other) {
    formatter_.Take(other.formatter_);
  }

  ~FullFormat() {
    FinishFormatting();
  }
};

inline FullFormat Format(const char *format) { return FullFormat(format); }

class Print : public ArgFormatter {
 private:
  Formatter formatter_;

  // Do not implement.
  Print(const Print&);
  Print& operator=(const Print&);

 public:
  explicit Print(const char *format) : ArgFormatter(formatter_) {
    ArgFormatter::operator=(formatter_(format));
  }

  ~Print() {
    FinishFormatting();
    std::fwrite(formatter_.data(), 1, formatter_.size(), stdout);
  }
};
}

namespace fmt = format;

#endif  // FORMAT_H_
