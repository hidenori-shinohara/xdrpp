// -*- C++ -*-

//! \file msgsock.h Send and receive delimited messages over
//! non-blocking sockets.

#ifndef _XDRPP_MSGSOCK_H_INCLUDED_
#define _XDRPP_MSGSOCK_H_INCLUDED_ 1

#include <deque>
#include <xdrpp/message.h>
#include <xdrpp/pollset.h>

namespace xdr {

/** \brief Send and receive a series of delimited messages on a stream
 * socket.
 *
 * The format is simple:  A 4-byte length (in little-endian format)
 * followed by that many bytes.  The implementation is optimized for
 * having many sockets each receiving a small number of messages, as
 * opposed to receiving many messages over the same socket.
 *
 * Currently this calls read once or twice per message to get the
 * exact length before allocating buffer space and reading the message
 * body (possibly including the next message length).  This could be
 * fixed to read at least a little bit more data speculatively and
 * reduce the number of system calls.
 */
class msg_sock {
public:
  static constexpr std::size_t default_maxmsglen = 0x100000;
  using rcb_t = std::function<void(msg_ptr)>;

  template<typename T> msg_sock(pollset &ps, sock_t s, T &&rcb,
				size_t maxmsglen = default_maxmsglen)
    : ps_(ps), s_(s), maxmsglen_(maxmsglen), rcb_(std::forward<T>(rcb)) {
    init();
  }
  msg_sock(pollset &ps, sock_t s) : msg_sock(ps, s, nullptr) {}
  ~msg_sock();
  msg_sock &operator=(msg_sock &&) = delete;

  template<typename T> void setrcb(T &&rcb) {
    rcb_ = std::forward<T>(rcb);
    initcb();
  }

  size_t wsize() const { return wsize_; }
  void putmsg(msg_ptr &b);
  void putmsg(msg_ptr &&b) { putmsg(b); }
  //! Returns pointer to a \c bool that becomes \c true once the
  //! msg_sock has been deleted.
  std::shared_ptr<const bool> destroyed_ptr() const { return destroyed_; }
  pollset &get_pollset() { return ps_; }
  //! Returns the socket, but do not do IO on it.  This is just for
  //! calling things like \c getpeername.
  sock_t get_sock() const { return s_; }

private:
  pollset &ps_;
  const sock_t s_;
  const size_t maxmsglen_;
  std::shared_ptr<bool> destroyed_{std::make_shared<bool>(false)};

  rcb_t rcb_;
  uint32_t nextlen_;
  msg_ptr rdmsg_;
  size_t rdpos_ {0};

  std::deque<msg_ptr> wqueue_;
  size_t wsize_ {0};
  size_t wstart_ {0};
  bool wfail_ {false};

  static constexpr bool eagain(int err) {
    return err == EAGAIN || err == EWOULDBLOCK || err == EINTR;
  }
  char *nextlenp() { return reinterpret_cast<char *>(&nextlen_); }
  uint32_t nextlen() const { return swap32le(nextlen_); }

  void init();
  void initcb();
  void input();
  void pop_wbytes(size_t n);
  void output(bool cbset);
};


class rpc_sock {
  uint32_t xid_{0};
  std::unordered_map<uint32_t, msg_sock::rcb_t> calls_;

  void abort_all_calls();
  void recv_msg(msg_ptr b);
  void recv_call(msg_ptr);
public:
  std::unique_ptr<msg_sock> ms_;
  using rcb_t = msg_sock::rcb_t;
  rcb_t servcb_;

  template<typename T>
  rpc_sock(pollset &ps, sock_t s, T &&t,
	   size_t maxmsglen = msg_sock::default_maxmsglen)
    : ms_(new msg_sock(ps, s,
		       std::bind(&rpc_sock::recv_msg, this,
				 std::placeholders::_1),
		       maxmsglen)),
      servcb_(std::forward<T>(t)) {}
  rpc_sock(pollset &ps, sock_t s) : rpc_sock(ps, s, rcb_t(nullptr)) {}
  ~rpc_sock() { abort_all_calls(); }
  template<typename T> void set_servcb(T &&scb) {
    servcb_ = std::forward<T>(scb);
  }

  uint32_t get_xid() {
    while (calls_.find(++xid_) == calls_.end())
      ;
    return xid_;
  }

  void send_call(msg_ptr &b, rcb_t cb);
  void send_call(msg_ptr &&b, rcb_t cb) { send_call(b, cb); }
  void send_reply(msg_ptr &&b) { ms_->putmsg(std::move(b)); }
};

//! Functor wrapper around \c rpc_sock::send_reply.  Mostly useful because
//! std::function implementations avoid memory allocation with \c
//! operator() but not when other methods are passed to \c std::bind.
struct rpc_sock_reply_t {
  rpc_sock *ms_;
  constexpr rpc_sock_reply_t(rpc_sock *ms) : ms_(ms) {}
  void operator()(msg_ptr b) const { ms_->send_reply(std::move(b)); }
};


} // namespace xdr 

#endif // !_XDRPP_MSGSOCK_H_INCLUDED_
