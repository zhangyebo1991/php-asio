/**
 * php-asio/future.cpp
 *
 * @author CismonX<admin@cismon.net>
 */

#include "future.hpp"
#include "generator.hpp"
#include "timer.hpp"
#include "signal.hpp"
#include "resolver.hpp"
#include "socket.hpp"
#include "acceptor.hpp"
#include "stream_descriptor.hpp"

namespace asio
{
    future* future::add(
        void* io_object
#ifdef ENABLE_COROUTINE
        , zend_object*& obj)
    {
        obj = p3::alloc_object<future>(class_entry,
            [io_object](future* ptr) {
                new(ptr) future(io_object);
            });
        GC_ADDREF(obj);
        return p3::to_object<future>(obj);
#else
        ) {
        return new future(io_object);
#endif // ENABLE_COROUTINE
    }

    template <typename T>
    void future::on_resolve(const ASYNC_CALLBACK(T)&& callback)
    {
        callback_ = new ASYNC_CALLBACK(T)(std::move(callback));
    }

    template <typename V, typename T>
    void future::resolve(const boost::system::error_code& ec, T arg)
    {
        auto callback = static_cast<ASYNC_CALLBACK(T)*>(callback_);
        send_ = (*callback)(ec, arg);
#ifdef ENABLE_COROUTINE
        if (yield_) {
            last_error_ = static_cast<int64_t>(ec.value());
            generator_send(reinterpret_cast<zend_generator*>(Z_OBJ_P(generator_)), send_);
            coroutine(generator_);
        }
#endif // ENABLE_COROUTINE
        zval_ptr_dtor(send_);
        efree(send_);
        delete callback;
        auto io_object = static_cast<V*>(io_object_);
        if (io_object->handler_count_dec() == 0)
            PHP_ASIO_OBJ_DTOR(io_object);
#ifdef ENABLE_STRAND
        if (strand_ && strand_->handler_count_dec() == 0)
            PHP_ASIO_OBJ_DTOR(strand_);
#endif // ENABLE_STRAND
#ifdef ENABLE_COROUTINE
        PHP_ASIO_OBJ_DTOR(this);
#else
        delete this;
#endif // ENABLE_COROUTINE
    }

#ifdef ENABLE_STRAND
    zval* future::handle_strand(zval* callable)
    {
        if (callable && Z_TYPE_P(callable) == IS_OBJECT &&
            instanceof_function(Z_OBJCE_P(callable), wrapped_handler::class_entry)) {
            const auto wrapped_hander = p3::to_object<wrapped_handler>(callable);
            strand_ = wrapped_hander->strand_;
            return wrapped_hander->callback_;
        }
        return callable;
    }
#endif // ENABLE_STRAND

#ifdef ENABLE_COROUTINE
    void future::coroutine(zval* value)
    {
        if (Z_TYPE_P(value) == IS_OBJECT && instanceof_function(Z_OBJCE_P(value), zend_ce_generator)) {
            const auto generator = reinterpret_cast<zend_generator*>(Z_OBJ_P(value));
            if (generator_valid(generator)) {
                const auto ret = generator_current(generator);
                if (ret && instanceof_function(Z_OBJCE_P(ret), class_entry)) {
                    const auto future = p3::to_object<asio::future>(ret);
                    future->generator_ = value;
                    future->yield_ = true;
                    return;
                }
                PHP_ASIO_ERROR(E_WARNING, "Invalid yield value. Future expected.");
            }
        }
        zval_ptr_dtor(value);
        efree(value);
    }

    P3_METHOD(future, lastError)
    {
        RETVAL_LONG(last_error_)
    }

    thread_local int64_t future::last_error_ = 0;

    PHP_ASIO_CE_DEFINE(future);
#endif // ENABLE_COROUTINE

    template void future::on_resolve(const ASYNC_CALLBACK(int)&&);
    template void future::on_resolve(const ASYNC_CALLBACK(size_t)&&);
    template void future::on_resolve(const ASYNC_CALLBACK(tcp::resolver::iterator)&&);
    template void future::on_resolve(const ASYNC_CALLBACK(udp::resolver::iterator)&&);

    template void future::resolve<timer>(const boost::system::error_code&, int);
    template void future::resolve<signal>(const boost::system::error_code&, int);
    template void future::resolve<resolver<tcp>>(
        const boost::system::error_code&, tcp::resolver::iterator);
    template void future::resolve<resolver<udp>>(
        const boost::system::error_code&, udp::resolver::iterator);
    template void future::resolve<socket<tcp>>(const boost::system::error_code&, int);
    template void future::resolve<socket<tcp>>(const boost::system::error_code&, size_t);
    template void future::resolve<socket<unix>>(const boost::system::error_code&, int);
    template void future::resolve<socket<unix>>(const boost::system::error_code&, size_t);
    template void future::resolve<socket<udp>>(const boost::system::error_code&, size_t);
    template void future::resolve<socket<udg>>(const boost::system::error_code&, size_t);
    template void future::resolve<acceptor<tcp>>(const boost::system::error_code&, int);
    template void future::resolve<acceptor<unix>>(const boost::system::error_code&, int);
    template void future::resolve<stream_descriptor>(const boost::system::error_code&, size_t);
}
