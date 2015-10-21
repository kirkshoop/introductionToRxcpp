#pragma once

namespace rx {

template<class Select = defaults, class... ON>
struct observer;

namespace detail {
    
template<class T>
struct observer_check : public false_type {};

template<class... ON>
struct observer_check<observer<ON...>> : public true_type {};

template<class T>
using for_observer = enable_if_t<observer_check<decay_t<T>>::value>;

template<class T>
using not_observer = enable_if_t<!observer_check<decay_t<T>>::value>;

auto report = [](auto&& e, auto&& f, auto&&... args){
    try{f(args...);} catch(...) {e(current_exception());}
};

auto enforce = [](const subscription& lifetime, auto&& f) {
    return [&](auto&&... args){
        if (!lifetime.is_stopped()) f(args...);
    };
};

auto end = [](const subscription& lifetime, auto&& f, auto&&... cap) {
    return [&](auto&&... args){
        if (!lifetime.is_stopped()) { 
            f(cap..., args...); 
            lifetime.stop();
        }
    };
};

struct noop
{
    template<class V, class CheckV = not_observer<V>>
    void operator()(V&&) const {
    }
    template<class Delegatee, class V>
    void operator()(const Delegatee& d, V&& v) const {
        d.next(std::forward<V>(v));
    }
    inline void operator()() const {
    }
    template<class Delegatee, class Check = for_observer<Delegatee>>
    void operator()(const Delegatee& d) const {
        d.complete();
    }
};
struct ignore
{
    template<class E>
    void operator()(E&&) const {
    }
    template<class Delegatee, class E, class CheckD = for_observer<Delegatee>>
    void operator()(const Delegatee& d, E&& e) const {
        d.error(forward<E>(e));
    }
};
struct pass
{
    template<class Delegatee, class E, class CheckD = for_observer<Delegatee>>
    void operator()(const Delegatee& d, E&& e) const {
        d.error(forward<E>(e));
    }
    template<class Delegatee, class Check = for_observer<Delegatee>>
    void operator()(const Delegatee& d) const {
        d.complete();
    }
};
struct skip
{
    template<class Delegatee, class E, class CheckD = for_observer<Delegatee>>
    void operator()(const Delegatee& , E&& ) const {
    }
    template<class Delegatee, class Check = for_observer<Delegatee>>
    void operator()(const Delegatee& ) const {
    }
};
struct fail
{
    template<class E>
    void operator()(E&&) const {
        info("abort! ");
        std::abort();
    }
    template<class Delegatee, class E, class CheckD = for_observer<Delegatee>>
    void operator()(const Delegatee&, E&&) const {
        info("abort! ");
        std::abort();
    }
};

}

namespace detail{
    template<class V, class E>
    struct abstract_observer
    {
        virtual ~abstract_observer(){}
        virtual void next(const V&) const = 0;
        virtual void error(const E&) const = 0;
        virtual void complete() const = 0;
    };

    template<class V, class E, class... ON>
    struct basic_observer : public abstract_observer<V, E> {
        using value_type = decay_t<V>;
        using errorvalue_type = decay_t<E>;
        basic_observer(const observer<ON...>& o)
            : d(o){
        }
        observer<ON...> d;
        virtual void next(const value_type& v) const {
            d.next(v);
        }
        virtual void error(const errorvalue_type& err) const {
            d.error(err);
        }
        virtual void complete() const {
            d.complete();
        }
    };
}
template<class V, class E>
struct observer<interface<V, E>> {
    using value_type = decay_t<V>;
    using errorvalue_type = decay_t<E>;
    observer(const observer& o) = default;
    template<class... ON>
    observer(const observer<ON...>& o)
        : lifetime(o.lifetime)
        , d(make_shared<detail::basic_observer<V, E, ON...>>(o)) {
    }
    subscription lifetime;
    shared_ptr<detail::abstract_observer<value_type, errorvalue_type>> d;
    void next(const value_type& v) const {
        d->next(v);
    }
    void error(const errorvalue_type& err) const {
        d->error(err);
    }
    void complete() const {
        d->complete();
    }
    template<class... TN>
    observer as_interface() const {
        return {*this};
    }
};
template<class V, class E>
using observer_interface = observer<interface<V, E>>;


template<class Next, class Error, class Complete>
struct observer<Next, Error, Complete> {
    subscription lifetime;
    mutable Next n;
    mutable Error e;
    mutable Complete c;
    template<class V>
    void next(V&& v) const {
        using namespace detail;
        report(end(lifetime, e), enforce(lifetime, n), std::forward<V>(v));
    }
    template<class E>
    void error(E&& err) const {
        using namespace detail;
        report(fail{}, end(lifetime, e), std::forward<E>(err));
    }
    void complete() const {
        using namespace detail;
        report(fail{}, end(lifetime, c));
    }
    template<class V, class E = exception_ptr>
    observer_interface<V, E> as_interface() const {
        using observer_t = detail::basic_observer<V, E, Next, Error, Complete>;
        return {lifetime, make_shared<observer_t>(*this)};
    }
};
template<class Delegatee, class Next, class Error, class Complete>
struct observer<Delegatee, Next, Error, Complete> {
    Delegatee d;
    subscription lifetime;
    mutable Next n;
    mutable Error e;
    mutable Complete c;
    template<class V>
    void next(V&& v) const {
        using namespace detail;
        report(end(lifetime, e, d), enforce(lifetime, n), d, std::forward<V>(v));
    }
    template<class E>
    void error(E&& err) const {
        using namespace detail;
        report(fail{}, end(lifetime, e), d, std::forward<E>(err));
    }
    void complete() const {
        using namespace detail;
        report(fail{}, end(lifetime, c), d);
    }
    template<class V, class E = exception_ptr>
    observer_interface<V, E> as_interface() const {
        using observer_t = detail::basic_observer<V, E, Delegatee, Next, Error, Complete>;
        return {lifetime, make_shared<observer_t>(*this)};
    }
};

template<class Next = detail::noop, class Error = detail::fail, class Complete = detail::noop, 
    class CheckN = detail::not_observer<Next>>
auto make_observer(subscription lifetime, Next&& n = Next{}, Error&& e = Error{}, Complete&& c = Complete{}) {
    return observer<decay_t<Next>, decay_t<Error>, decay_t<Complete>>{
        lifetime,
        forward<Next>(n), 
        forward<Error>(e), 
        forward<Complete>(c)
    };
}

template<class Delegatee, class Next = detail::noop, class Error = detail::pass, class Complete = detail::pass, 
    class CheckD = detail::for_observer<Delegatee>>
auto make_observer(Delegatee&& d, subscription lifetime, Next&& n = Next{}, Error&& e = Error{}, Complete&& c = Complete{}) {
    return observer<decay_t<Delegatee>, decay_t<Next>, decay_t<Error>, decay_t<Complete>>{
        forward<Delegatee>(d), 
        lifetime,
        forward<Next>(n), 
        forward<Error>(e), 
        forward<Complete>(c)
    };
}

}