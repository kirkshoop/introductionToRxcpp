#pragma once

namespace rx {

template<class Select = defaults>
struct subscriber;

namespace detail {
    template<class V, class C, class E>
    using create_t = function<observer_interface<V, E>(context_interface<C, E>)>;
}
template<class V, class C, class E>
struct subscriber<interface<V, C, E>> {
    detail::create_t<V, C, E> c;
    subscriber(const subscriber&) = default;
    template<class Create>
    subscriber(const subscriber<Create>& o)
        : c(o.c) {
    }
    observer_interface<V, E> create(context_interface<C, E> ctx) const {
        return c(ctx);
    }
    template<class... TN>
    subscriber as_interface() const {
        return {*this};
    }
};
template<class V, class C, class E>
using subscriber_interface = subscriber<interface<V, C, E>>;

template<class Create>
struct subscriber {
    Create c;
    /// \brief returns observer
    template<class... CN>
    auto create(context<CN...> ctx) const {
        static_assert(detail::is_specialization_of<decltype(c(ctx)), observer>::value, "subscriber function must return observer!");
        return c(ctx);
    }
    template<class V, class C = steady_clock, class E = exception_ptr>
    subscriber_interface<V, C, E> as_interface() const {
        return {*this};
    }
};

template<class Create>
subscriber<Create> make_subscriber(Create&& c) {
    return {forward<Create>(c)};
}

auto make_subscriber() {
    return make_subscriber([](auto ctx){return make_observer(ctx.lifetime);});
}

namespace detail {

template<class T>
using for_subscriber = for_specialization_of_t<T, subscriber>;

template<class T>
using not_subscriber = not_specialization_of_t<T, subscriber>;

}

}