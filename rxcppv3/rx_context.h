#pragma once

namespace rx {


template<class Select = defaults, class... TN>
struct context;

namespace detail {
    template<class C, class E>
    struct abstract_context : public abstract_strand<C, E>
    {
        virtual ~abstract_context(){}
    };

    template<class C, class E, class MakeStrand>
    struct basic_context : public abstract_context<C, E> {
        using clock_type = decay_t<C>;
        using errorvalue_type = decay_t<E>;
        basic_context(context<void, MakeStrand, C> o)
            : d(o){
        }
        template<class Defaults>
        basic_context(context<Defaults> o)
            : d(o.lifetime, o.m){
        }
        context<void, MakeStrand, C> d;
        virtual time_point_t<clock_type> now() const {
            return d.now();
        }
        virtual void defer_at(time_point_t<clock_type> at, observer_interface<re_defer_at_t<C>, E> out) const {
            d.defer_at(at, out);
        }
    };
    
    template<class C, class E>
    using make_strand_t = function<strand_interface<C, E>(subscription)>;

    template<class Clock = steady_clock>
    struct make_immediate {
        auto operator()(subscription lifetime) const {
            return make_strand<Clock>(lifetime, detail::immediate<Clock>{lifetime}, detail::now<Clock>{});
        }
    };

}

#if !RX_SLOW
template<class Clock>
auto make_shared_make_strand(const detail::make_immediate<Clock>& make) {
    return make;
}
#endif

template<class C, class E>
struct context<interface<C, E>> {
    using payload_type = void;
    using clock_type = decay_t<C>;
    using errorvalue_type = decay_t<E>;
    context(const context& o) = default;
    context(context&& o) = default;
    context(const context<void, void, clock_type>& o)
        : lifetime(o.lifetime)
        , d(make_shared<detail::basic_context<C, E, void>>(o))
        , m([m = o.m](subscription lifetime){
            return m(lifetime);
        }) {
    }
    context(context<void, void, clock_type>&& o)
        : lifetime(o.lifetime)
        , d(make_shared<detail::basic_context<C, E, void>>(o))
        , m([m = o.m](subscription lifetime){
            return m(lifetime);
        }) {
    }
    template<class... CN>
    context(const context<CN...>& o)
        : lifetime(o.lifetime)
        , d(make_shared<detail::basic_context<C, E, decay_t<decltype(o.m)>>>(o))
        , m([m = o.m](subscription lifetime){
            return m(lifetime);
        }) {
    }
    template<class... CN>
    context(context<CN...>&& o)
        : lifetime(o.lifetime)
        , d(make_shared<detail::basic_context<C, E, decay_t<decltype(o.m)>>>(o))
        , m([m = o.m](subscription lifetime){
            return m(lifetime);
        }) {
    }

    subscription lifetime;
    shared_ptr<detail::abstract_context<clock_type, errorvalue_type>> d;
    detail::make_strand_t<clock_type, E> m;
    time_point_t<clock_type> now() const {
        return d->now();
    }
    void defer_at(time_point_t<clock_type> at, observer_interface<detail::re_defer_at_t<clock_type>, E> out) const {
        d->defer_at(at, out);
    }
    template<class... TN>
    context as_interface() const {
        return {*this};
    }
};
template<class C, class E>
using context_interface = context<interface<C, E>>;

template<>
struct context<defaults> {
    using payload_type = void;
    using Clock = steady_clock;
    using clock_type = decay_t<Clock>;
    using make_strand_type = detail::make_immediate<Clock>;
    using strand_type = decay_t<decltype(declval<make_strand_type>()(declval<subscription>()))>;
    subscription lifetime;
    make_strand_type m;
private:
    struct State {
        explicit State(strand_type&& s) : s(s) {}
        explicit State(const strand_type& s) : s(s) {}
        strand_type s;
    };
    state<State> s;    
public:

    explicit context(subscription lifetime) 
        : lifetime(lifetime)
        , m()
        , s(make_state<State>(lifetime, m(subscription{}))) {
        lifetime.insert(s.get().s.lifetime);
#if !RX_DEFER_IMMEDIATE
        lifetime.bind_defer([s = s.get().s](function<void()> target){
            if (s.lifetime.is_stopped()) abort();
            defer(s, make_observer(subscription{}, [target](auto& ){
                return target();
            }));
        });
#endif
    }
    context(subscription lifetime, make_strand_type m, strand_type strand) 
        : lifetime(lifetime)
        , m(m)
        , s(make_state<State>(lifetime, strand)) {
        lifetime.insert(s.get().s.lifetime);
#if !RX_DEFER_IMMEDIATE
        lifetime.bind_defer([s = s.get().s](function<void()> target){
            if (s.lifetime.is_stopped()) abort();
            defer(s, make_observer(subscription{}, [target](auto& ){
                return target();
            }));
        });
#endif
    }
    time_point_t<clock_type> now() const {
        return s.get().s.now();
    }
    template<class... ON>
    void defer_at(time_point_t<clock_type> at, observer<ON...> out) const {
        s.get().s.defer_at(at, out);
    }
    template<class E = exception_ptr>
    context_interface<Clock, E> as_interface() const {
        using context_t = detail::basic_context<Clock, E, make_strand_type>;
        return {lifetime, make_shared<context_t>(*this)};
    }
};

template<class Clock>
struct context<void, void, Clock> {
    using payload_type = void;
    using clock_type = decay_t<Clock>;
    using make_strand_type = detail::make_immediate<Clock>;
    using strand_type = decay_t<decltype(declval<make_strand_type>()(declval<subscription>()))>;
    subscription lifetime;
    make_strand_type m;
private:
    struct State {
        explicit State(strand_type&& s) : s(s) {}
        explicit State(const strand_type& s) : s(s) {}
        strand_type s;
    };
    state<State> s;    
public:

    explicit context(subscription lifetime) 
        : lifetime(lifetime)
        , m()
        , s(make_state<State>(lifetime, m(subscription{}))) {
        lifetime.insert(s.get().s.lifetime);
#if !RX_DEFER_IMMEDIATE
        lifetime.bind_defer([s = s.get().s](function<void()> target){
            if (s.lifetime.is_stopped()) abort();
            defer(s, make_observer(subscription{}, [target](auto& ){
                return target();
            }));
        });
#endif
    }
    context(subscription lifetime, make_strand_type m, strand_type s) 
        : lifetime(lifetime)
        , m(m)
        , s(make_state<State>(lifetime, s)) {
        lifetime.insert(s.get().s.lifetime);
#if !RX_DEFER_IMMEDIATE
        lifetime.bind_defer([s = s.get().s](function<void()> target){
            if (s.lifetime.is_stopped()) abort();
            defer(s, make_observer(subscription{}, [target](auto& ){
                return target();
            }));
        });
#endif
    }
    time_point_t<clock_type> now() const {
        return s.get().s.now();
    }
    template<class... ON>
    void defer_at(time_point_t<clock_type> at, observer<ON...> out) const {
        s.get().s.defer_at(at, out);
    }
    template<class E = exception_ptr>
    context_interface<clock_type, E> as_interface() const {
        using context_t = detail::basic_context<clock_type, E, make_strand_type>;
        return {lifetime, make_shared<context_t>(*this)};
    }
};

template<class MakeStrand, class Clock>
struct context<void, MakeStrand, Clock> {
    using payload_type = void;
    using clock_type = decay_t<Clock>;
    using make_strand_type = decay_t<MakeStrand>;
    using strand_type = decay_t<decltype(declval<make_strand_type>()(declval<subscription>()))>;
    subscription lifetime;
    MakeStrand m;

private:
    struct State {
        explicit State(strand_type&& s) : s(s) {}
        explicit State(const strand_type& s) : s(s) {}
        strand_type s;
    };
    state<State> s;  

public:
    context(subscription lifetime, make_strand_type m) 
        : lifetime(lifetime)
        , m(m)
        , s(make_state<State>(lifetime, m(subscription{}))) {
        lifetime.insert(s.get().s.lifetime);
#if !RX_DEFER_IMMEDIATE
        lifetime.bind_defer([s = s.get().s](function<void()> target){
            if (s.lifetime.is_stopped()) abort();
            defer(s, make_observer(subscription{}, [target](auto& ){
                return target();
            }));
        });
#endif
    }
    context(subscription lifetime, make_strand_type m, strand_type s) 
        : lifetime(lifetime)
        , m(m)
        , s(make_state<State>(lifetime, s)) {
        lifetime.insert(this->s.get().s.lifetime);
#if !RX_DEFER_IMMEDIATE
        lifetime.bind_defer([s = this->s.get().s](function<void()> target){
            if (s.lifetime.is_stopped()) abort();
            defer(s, make_observer(subscription{}, [target](auto& ){
                return target();
            }));
        });
#endif
    }
    time_point_t<clock_type> now() const {
        return s.get().s.now();
    }
    template<class... ON>
    void defer_at(time_point_t<clock_type> at, observer<ON...> out) const {
        s.get().s.defer_at(at, out);
    }
    template<class E = exception_ptr>
    context_interface<Clock, E> as_interface() const {
        using context_t = detail::basic_context<Clock, E, make_strand_type>;
        return {lifetime, make_shared<context_t>(*this)};
    }  
};

template<class Payload, class MakeStrand, class Clock>
struct context<Payload, MakeStrand, Clock> {
    using payload_type = decay_t<Payload>;
    using clock_type = decay_t<Clock>;
    using make_strand_type = decay_t<MakeStrand>;
    using strand_type = decay_t<decltype(declval<make_strand_type>()(declval<subscription>()))>;
    subscription lifetime;
    MakeStrand m;
    context(subscription lifetime, payload_type p, make_strand_type m) 
        : lifetime(lifetime)
        , m(m)
        , s(make_state<State>(lifetime, m(subscription{}), move(p))) {
        lifetime.insert(s.get().s.lifetime);
#if !RX_DEFER_IMMEDIATE
        lifetime.bind_defer([s = s.get().s](function<void()> target){
            if (s.lifetime.is_stopped()) abort();
            defer(s, make_observer(subscription{}, [target](auto& ){
                return target();
            }));
        });
#endif
    }
    time_point_t<clock_type> now() const {
        return s.get().s.now();
    }
    template<class... ON>
    void defer_at(time_point_t<clock_type> at, observer<ON...> out) const {
        s.get().s.defer_at(at, out);
    }
    template<class E = exception_ptr>
    context_interface<clock_type, E> as_interface() const {
        using context_t = detail::basic_context<clock_type, E, make_strand_type>;
        return {lifetime, make_shared<context_t>(*this)};
    }
    payload_type& get(){
        return s.get().p;
    }
    const payload_type& get() const {
        return s.get().p;
    }
    operator context<void, make_strand_type, clock_type> () const {
        return context<void, make_strand_type, clock_type>(lifetime, m, s.get().s);
    }
private:
    using Strand = decay_t<decltype(declval<MakeStrand>()(declval<subscription>()))>;
    struct State {
        State(strand_type&& s, payload_type&& p) : s(s), p(p) {}
        State(const strand_type& s, const payload_type& p) : s(s), p(p) {}
        strand_type s;
        payload_type p;
    };
    state<State> s;    
};


template<class... CN, class... ON>
subscription defer(context<CN...> s, observer<ON...> out) {
    s.defer_at(s.now(), out);
    return out.lifetime;
}
template<class... CN, class... ON>
subscription defer_at(context<CN...> s, clock_time_point_t<context<CN...>> at, observer<ON...> out) {
    s.defer_at(at, out);
    return out.lifetime;
}
template<class... CN, class... ON>
subscription defer_after(context<CN...> s, clock_duration_t<context<CN...>> delay, observer<ON...> out) {
    s.defer_at(s.now() + delay, out);
    return out.lifetime;
}
template<class... CN, class... ON>
subscription defer_periodic(context<CN...> s, clock_time_point_t<context<CN...>> initial, clock_duration_t<context<CN...>> period, observer<ON...> out) {
    auto lifetime = subscription{};
    auto state = make_state<pair<long, clock_time_point_t<context<CN...>>>>(lifetime, make_pair(0, initial));
    s.defer_at(initial, make_observer(
        out,
        out.lifetime, 
        [state, period](const observer<ON...>& out, auto& self) mutable {
            auto& s = state.get();
            out.next(s.first++);
            s.second += period;
            self(s.second);
        }, detail::pass{}, detail::skip{}));
    return out.lifetime;
}

inline auto make_context(subscription lifetime) {
    auto c = context<>{
        lifetime
    };
    return c;
}

template<class Payload, class... AN>
auto make_context(subscription lifetime, AN&&... an) {
    auto c = context<Payload, detail::make_immediate<steady_clock>, steady_clock>{
        lifetime,
        Payload(forward<AN>(an)...),
        detail::make_immediate<steady_clock>{}
    };
    return c;
}

template<class Payload, class Clock, class... AN>
auto make_context(subscription lifetime, AN&&... an) {
    auto c = context<Payload, detail::make_immediate<Clock>, Clock>{
        lifetime,
        Payload(forward<AN>(an)...),
        detail::make_immediate<Clock>{}
    };
    return c;
}

template<class Payload, class Clock, class MakeStrand, class... AN>
auto make_context(subscription lifetime, MakeStrand&& m, AN&&... an) {
    auto c = context<Payload, decay_t<MakeStrand>, Clock>{
        lifetime,
        Payload(forward<AN>(an)...),
        forward<MakeStrand>(m)
    };
    return c;
}

template<class Clock, class MakeStrand, class C = void_t<typename Clock::time_point>>
auto make_context(subscription lifetime, MakeStrand&& m) {
    auto c = context<void, decay_t<MakeStrand>, Clock>{
        lifetime,
        forward<MakeStrand>(m)
    };
    return c;
}

template<class MakeStrand, class C = detail::for_strand<decltype(declval<MakeStrand>()(declval<subscription>()))>>
auto make_context(subscription lifetime, MakeStrand&& m) {
    auto c = context<void, decay_t<MakeStrand>, steady_clock>{
        lifetime,
        forward<MakeStrand>(m)
    };
    return c;
}

inline auto copy_context(subscription lifetime, const context<>&) {
    return make_context(lifetime);
}

template<class Payload, class MakeStrand, class Clock>
auto copy_context(subscription lifetime, const context<Payload, MakeStrand, Clock>& o) {
    return make_context<Payload, Clock>(lifetime, o.m, o.get());
}

template<class MakeStrand, class Clock>
auto copy_context(subscription lifetime, const context<void, MakeStrand, Clock>& o) {
    return make_context<Clock>(lifetime, o.m);
}

template<class Clock>
auto copy_context(subscription lifetime, const context<void, void, Clock>& o) {
    return make_context<Clock>(lifetime, o.m);
}

template<class NewMakeStrand, class... CN>
auto copy_context(subscription lifetime, NewMakeStrand&& makeStrand, const context<CN...>& ) {
    return make_context<clock_t<context<CN...>>>(lifetime, forward<NewMakeStrand>(makeStrand));
}

template<class C, class E>
auto copy_context(subscription lifetime, const context_interface<C, E>& o) {
    auto c = context_interface<C, E>{
        context<void, detail::make_strand_t<C, E>, C> {
            lifetime,
            o.m
        }
    };
    return c;
}

namespace detail {

template<class T>
using for_context = for_specialization_of_t<T, context>;

template<class T>
using not_context = not_specialization_of_t<T, context>;

}

}