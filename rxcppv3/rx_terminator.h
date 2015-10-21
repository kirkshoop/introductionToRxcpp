#pragma once

namespace rx {


template<class Select = defaults>
struct terminator;

namespace detail {
    template<class V, class C, class E>
    using terminate_t = function<starter_interface<C, E>(const observable_interface<V, C, E>&)>;
}
template<class V, class C, class E>
struct terminator<interface<V, C, E>> {
    detail::terminate_t<V, C, E> t;
    terminator(const terminator&) = default;
    template<class Terminate>
    terminator(const terminator<Terminate>& t) 
        : t(t.t) {
    }
    starter_interface<C, E> terminate(const observable_interface<V, C, E>& ovr) const {
        return t(ovr);
    }
    template<class... TN>
    terminator as_interface() const {
        return {*this};
    }
};
template<class V, class C, class E>
using terminator_interface = terminator<interface<V, C, E>>;

template<class Terminate>
struct terminator {
    Terminate t;
    /// \brief returns starter
    template<class... ON>
    auto terminate(observable<ON...> o) const {
        static_assert(detail::is_specialization_of<decltype(t(o)), starter>::value, "terminator function must return starter!");
        return t(o);
    }
    template<class V, class C = steady_clock, class E = exception_ptr>
    terminator_interface<V, C, E> as_interface() const {
        return {*this};
    }
};

template<class Terminate>
terminator<Terminate> make_terminator(Terminate&& t) {
    return {forward<Terminate>(t)};
}

namespace detail {

template<class T>
using for_terminator = for_specialization_of_t<T, terminator>;

template<class T>
using not_terminator = not_specialization_of_t<T, terminator>;

}

}