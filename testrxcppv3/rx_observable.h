#pragma once

namespace rx {

template<class Select = defaults>
struct observable;

namespace detail {
    template<class V, class C, class E>
    using bind_t = function<starter_interface<C, E>(subscriber_interface<V, C, E>)>;
}
template<class V, class C, class E>
struct observable<interface<V, C, E>> {
    detail::bind_t<V, C, E> b;
    observable(const observable&) = default;
    template<class Bind>
    observable(const observable<Bind>& o) 
        : b(o.b) {
    } 
    starter_interface<C, E> bind(subscriber_interface<V, C, E> s) const {
        return b(s);
    }
    template<class... TN>
    observable as_interface() const {
        return {*this};
    }
};
template<class V, class C, class E>
using observable_interface = observable<interface<V, C, E>>;

template<class Bind>
struct observable {
    Bind b;
    /// \brief 
    /// \returns starter
    template<class Subscriber>
    auto bind(Subscriber&& s) const {
        static_assert(detail::is_specialization_of<decltype(b(s)), starter>::value, "observable function must return starter!");
        return b(s);
    }
    template<class V, class C = steady_clock, class E = exception_ptr>
    observable_interface<V, C, E> as_interface() const {
        return {*this};
    }
};

template<class Bind>
observable<Bind> make_observable(Bind&& b) {
    return {forward<Bind>(b)};
}

namespace detail {

template<class T>
using for_observable = for_specialization_of_t<T, observable>;

template<class T>
using not_observable = not_specialization_of_t<T, observable>;

}

}