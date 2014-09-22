// -*- C++ -*-

/** \file types.h Type definitions for xdrc compiler output. */

#ifndef _XDRC_TYPES_H_HEADER_INCLUDED_
#define _XDRC_TYPES_H_HEADER_INCLUDED_ 1

#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace xdr {

using std::uint32_t;

//! Poor man's version of C++14 enable_if_t.
#define ENABLE_IF(expr) typename std::enable_if<expr>::type

//! Generic class of XDR unmarshaling errors.
struct xdr_runtime_error : std::runtime_error {
  using std::runtime_error::runtime_error;
};

//! Attempt to exceed the bounds of a variable-length array or string.
struct xdr_overflow : xdr_runtime_error {
  using xdr_runtime_error::xdr_runtime_error;
};

//! Attempt to set invalid value for a union discriminant.
struct xdr_bad_value : xdr_runtime_error {
  using xdr_runtime_error::xdr_runtime_error;
};

//! Attempt to access wrong field of a union.
struct xdr_wrong_union : std::logic_error {
  using std::logic_error::logic_error;
};

//! This is used to apply an archive to a field.  It is designed as a
//! template class that can be specialized to various archive formats,
//! since some formats may want the fied name and others not.  Other
//! uses include translating types to supertypes, e.g., so an archive
//! can handle \c std::string instead of \c xdr::xstring.
template<typename Archive> struct archive_adapter {
  template<typename T> static void apply(Archive &ar, const char *, T &&t) {
    ar(std::forward<T>(t));
  }
};

//! By default, this function simply applies \c ar (which must be a
//! function object) to \c t.  However, it does so via the \c
//! xdr::archive_adapter template class, which can be specialized to
//! capture the field name as well.
template<typename Archive, typename T> inline void
archive(Archive &ar, const char *name, T &&t)
{
  archive_adapter<Archive>::apply(ar, name, std::forward<T>(t));
}

//! Metadata for all marshalable XDR types.
template<typename T> struct xdr_traits {
  //! \c T is a valid XDR type that can be serialized.
  static constexpr bool valid = false;
  //! \c T is an \c xstring, \c opaque_array, or \c opaque_vec.
  static constexpr bool is_bytes = false;
  //! \c T is an XDR struct or union (traits have load/save).
  static constexpr bool is_class = false;
  //! \c T is an XDR enum or bool (traits have enum_name).
  static constexpr bool is_enum = false;
  //! \c T is an xdr::pointer, xdr::xarray, or xdr::xvector (with load/save).
  static constexpr bool is_container = false;
  //! \c T is one of [u]int{32,64}_t, float, or double
  static constexpr bool is_numeric = false;
  //! \c T is a xdr::xvector, xdr::pointer, xdr::xstring, or xdr::opaque_vec
  static constexpr bool is_variable_size = false;
};

//! Default xdr_traits values for actual XDR types.
struct xdr_traits_base {
  static constexpr bool valid = true;
  static constexpr bool is_bytes = false;
  static constexpr bool is_class = false;
  static constexpr bool is_enum = false;
  static constexpr bool is_container = false;
  static constexpr bool is_numeric = false;
  static constexpr bool is_variable_size = false;
};

#define XDR_NUMERIC(type, size)					\
template<> struct xdr_traits<type> : xdr_traits_base {		\
  static constexpr bool is_numeric = true;			\
  static constexpr size_t serial_size(type) { return size; }	\
}
XDR_NUMERIC(std::int32_t, 4);
XDR_NUMERIC(std::uint32_t, 4);
XDR_NUMERIC(std::int64_t, 8);
XDR_NUMERIC(std::uint64_t, 8);
XDR_NUMERIC(float, 4);
XDR_NUMERIC(double, 8);
#undef XDR_NUMERIC

template<> struct xdr_traits<bool> : xdr_traits_base {
  static constexpr bool is_enum = true;
  static constexpr size_t serial_size(uint32_t) { return 4; }
  static constexpr const char *enum_name(uint32_t b) {
    return b == 0 ? "FALSE" : b == 1 ? "TRUE" : nullptr;
  }
};

static constexpr uint32_t XDR_MAX_LEN = 0xffffffff;

template<typename T, bool variable> struct xdr_container_base
  : xdr_traits_base {
  static constexpr bool is_container = true;
  static constexpr bool is_variable_size = variable;
  template<typename Archive> static void save(Archive &a, const T &t) {
    if (variable)
      archive(a, nullptr, uint32_t(t.size()));
    for (const typename T::value_type &o : t)
      archive(a, nullptr, o);
  }
  template<typename Archive> static void load(Archive &a, T &t) {
    uint32_t n;
    if (variable) {
      archive(a, nullptr, n);
      t.check_size(n);
      if (t.size() > n)
	t.resize(n);
    }
    else
      n = t.size();
    for (uint32_t i = 0; i < n; ++i)
      archive(a, nullptr, t.extend_at(i));
  }
};


//! XDR arrays are implemented using std::array as a supertype.
template<typename T, uint32_t N> struct xarray
  : std::array<T, size_t(N)> {
  using array = std::array<T, size_t(N)>;
  using array::array;
  static void validate() {}
  static void check_size(uint32_t i) {
    if (i != N)
      throw xdr_overflow("invalid size in xdr::xarray");
  }
  static void resize(uint32_t i) {
    if (i != N)
      throw xdr_overflow("invalid resize in xdr::xarray");
  }
  T &extend_at(uint32_t i) {
    if (i >= N)
      throw xdr_overflow("attempt to access invalid position in xdr::xarray");
    return (*this)[i];
  }
};

template<typename T, uint32_t N> struct xdr_traits<xarray<T,N>>
  : xdr_container_base<xarray<T,N>, false> {};

//! XDR \c opaque is represented as std::uint8_t;
template<uint32_t N = XDR_MAX_LEN> using opaque_array = xarray<std::uint8_t,N>;
template<uint32_t N> struct xdr_traits<opaque_array<N>> : xdr_traits_base {
  static constexpr bool is_bytes = true;
};


//! A vector with a maximum size (returned by xvector::max_size()).
//! Note that you can exceed the size, but an error will happen when
//! marshaling or unmarshaling the data structure.
template<typename T, uint32_t N = XDR_MAX_LEN>
struct xvector : std::vector<T> {
  using vector = std::vector<T>;
  using vector::vector;

  //! Return the maximum size allowed by the type.
  static constexpr uint32_t max_size() { return N; }

  //! Check whether a size is in bounds
  static void check_size(size_t n) {
    if (n > max_size())
      throw xdr_overflow("xvector overflow");
  }

  void append(const T *elems, std::size_t n) {
    check_size(this->size() + n);
    this->insert(this->end(), elems, elems + n);
  }
  T &extend_at(uint32_t i) {
    if (i >= N)
      throw xdr_overflow("attempt to access invalid position in xdr::xvector");
    if (i == this->size())
      this->emplace_back();
    return (*this)[i];
  }
  void resize(uint32_t n) {
    check_size(n);
    vector::resize(n);
  }
};

template<typename T, uint32_t N> struct xdr_traits<xvector<T,N>>
  : xdr_container_base<xvector<T,N>, true> {};

//! Variable-length opaque data is just a vector of std::uint8_t.
template<uint32_t N = XDR_MAX_LEN> using opaque_vec = xvector<std::uint8_t, N>;
template<uint32_t N> struct xdr_traits<opaque_vec<N>> : xdr_traits_base {
  static constexpr bool is_bytes = true;
  static constexpr bool is_variable_size = true;
};


//! A string with a maximum length (returned by xstring::max_size()).
//! Note that you can exceed the size, but an error will happen when
//! marshaling or unmarshaling the data structure.
template<uint32_t N = XDR_MAX_LEN> struct xstring : std::string {
  using string = std::string;

  //! Return the maximum size allowed by the type.
  static constexpr uint32_t max_size() { return N; }

  //! Check whether a size is in bounds
  static void check_size(size_t n) {
    if (n > max_size())
      throw xdr_overflow("xvector overflow");
  }

  //! Check that the string length is not greater than the maximum
  //! size.  \throws std::out_of_range and clears the contents of the
  //! string if it is too long.
  void validate() const { check_size(size()); }

  xstring() = default;
  xstring(const xstring &) = default;
  xstring(xstring &&) = default;
  xstring &operator=(const xstring &) = default;
  xstring &operator=(xstring &&) = default;

  template<typename...Args> xstring(Args&&...args)
    : string(std::forward<Args>(args)...) { validate(); }

  using string::data;
  char *data() { return &(*this)[0]; } // protobufs does this, so probably ok

//! \hideinitializer
#define ASSIGN_LIKE(method)					\
  template<typename...Args> xstring &method(Args&&...args) {	\
    string::method(std::forward<Args>(args)...);		\
    validate();							\
    return *this;						\
  }
  ASSIGN_LIKE(operator=)
  ASSIGN_LIKE(operator+=)
  ASSIGN_LIKE(append)
  ASSIGN_LIKE(push_back)
  ASSIGN_LIKE(assign)
  ASSIGN_LIKE(insert)
  ASSIGN_LIKE(replace)
  ASSIGN_LIKE(swap)
#undef ASSIGN_LIKE
};

template<uint32_t N> struct xdr_traits<xstring<N>> : xdr_traits_base {
  static constexpr bool is_bytes = true;
  static constexpr bool is_variable_size = true;
};


//! Optional data (represented with pointer notation in XDR source).
template<typename T> struct pointer : std::unique_ptr<T> {
  using value_type = T;
  using std::unique_ptr<T>::unique_ptr;
  using std::unique_ptr<T>::get;
  static void check_size(uint32_t n) {
    if (n > 1)
      throw xdr_overflow("xdr::pointer size must be 0 or 1");
  }
  uint32_t size() const { return *this ? 1 : 0; }
  T *begin() { return get(); }
  const T *begin() const { return get(); }
  T *end() { return begin() + size(); }
  const T *end() const { return begin() + size(); }
  T &extend_at(uint32_t i) {
    if (i != 0)
      throw xdr_overflow("attempt to access position > 0 in xdr::pointer");
    if (!size())
      this->reset(new T);
    return **this;
  }
  void resize(uint32_t n) {
    if (n == size())
      return;
    switch(n) {
    case 0:
      this->reset();
      break;
    case 1:
      this->reset(new T);
      break;
    default:
      throw xdr_overflow("xdr::pointer::resize: valid sizes are 0 and 1");
    }
  }
  T &activate() {
    if (!*this)
      this->reset(new T);
    return *this->get();
  }
};

template<typename T> struct xdr_traits<pointer<T>>
  : xdr_container_base<pointer<T>, true> {};


struct case_constructor_t {
  constexpr case_constructor_t() {}
  void operator()() const {}
  template<typename T> void operator()(T *) const {}
  template<typename T, typename F> void operator()(T *t, F T::*f) const {
    new (static_cast<void *>(std::addressof(t->*f))) F{};
  }
};
constexpr case_constructor_t case_constructor;

struct case_destroyer_t {
  constexpr case_destroyer_t() {}
  void operator()() const {}
  template<typename T> void operator()(T *) const {}
  template<typename T, typename F> void operator()(T *t, F T::*f) const {
    (t->*f).~F();
  }
};
constexpr case_destroyer_t case_destroyer;

struct case_construct_from {
  void *dest_;
  constexpr case_construct_from(void *dest) : dest_(dest) {}
  void operator()() const {}
  template<typename T> void operator()(T &&) const {}
  template<typename T, typename F> void operator()(const T &t, F T::*f) const {
    new (static_cast<void *>(&(static_cast<T*>(dest_)->*f))) F{t.*f};
  }
  template<typename T, typename F> void operator()(T &&t, F T::*f) const {
    new (static_cast<void *>(&(static_cast<T*>(dest_)->*f))) F{std::move(t.*f)};
  }
};

struct case_assign_from {
  void *dest_;
  constexpr case_assign_from(void *dest) : dest_(dest) {}
  void operator()() const {}
  template<typename T> void operator()(T &&) const {}
  template<typename T, typename F> void operator()(const T &t, F T::*f) const {
    static_cast<T*>(dest_)->*f = t.*f;
  }
  template<typename T, typename F> void operator()(T &&t, F T::*f) const {
    static_cast<T*>(dest_)->*f = std::move(t.*f);
  }
};

template<typename Archive> struct case_save {
  Archive &ar_;
  const char *name_;
  constexpr case_save(Archive &ar, const char *name) : ar_(ar), name_(name) {}
  void operator()() const {}
  template<typename T> void operator()(const T *) const {}
  template<typename T, typename F> void operator()(const T *t, F T::*f) const {
    archive(ar_, name_, t->*f);
  }
};

template<typename Archive> struct case_load {
  Archive &ar_;
  const char *name_;
  constexpr case_load(Archive &ar, const char *name) : ar_(ar), name_(name) {}
  void operator()() const {}
  template<typename T> void operator()(T *) const {}
  template<typename T, typename F> void operator()(T *t, F T::*f) const {
    archive(ar_, name_, t->*f);
  }
};

}

#endif // !_XDRC_TYPES_H_HEADER_INCLUDED_

