#pragma once

namespace rx {


template<class Select = defaults>
struct starter;

namespace detail {
    template<class C, class E>
    using start_t = function<subscription(context_interface<C, E>)>;
}
template<class C, class E>
struct starter<interface<C, E>> {
    detail::start_t<C, E> s;
    starter(const starter&) = default;
    template<class Start>
    starter(const starter<Start>& s)
        : s(s.s) {
    }
    subscription start(context_interface<C, E> ctx) const {
        return s(move(ctx));
    }
    template<class... TN>
    starter as_interface() const {
        return {*this};
    }
};
template<class C, class E>
using starter_interface = starter<interface<C, E>>;

template<class Start>
struct starter {
    Start s;
    template<class... CN>
    subscription start(context<CN...> ctx) const {
        return s(ctx);
    }
    template<class C = steady_clock, class E = exception_ptr>
    starter_interface<C, E> as_interface() const {
        return {*this};
    }
};
template<class Start>
starter<Start> make_starter(Start&& s) {
    return {forward<Start>(s)};
}

namespace detail {

template<class T>
struct starter_check : public false_type {};

template<class Select>
struct starter_check<starter<Select>> : public true_type {};

template<class T>
using for_starter = enable_if_t<starter_check<decay_t<T>>::value>;

template<class T>
using not_starter = enable_if_t<!starter_check<decay_t<T>>::value>;

}

}