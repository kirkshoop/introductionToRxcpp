#pragma once

namespace rx {


template<class Select = defaults, class... TN>
struct strand;

namespace detail {
    struct shared_strand_construct_t {};

    template<class C>
    using re_defer_at_t = function<void(time_point_t<C>)>;

    template<class C, class E>
    struct abstract_strand
    {
        virtual ~abstract_strand(){}
        virtual time_point_t<C> now() const = 0;
        virtual void defer_at(time_point_t<C>, observer_interface<re_defer_at_t<C>, E>) const = 0;
    };

    template<class C, class E, class Execute, class Now>
    struct basic_strand : public abstract_strand<C, E> {
        using clock_type = decay_t<C>;
        using errorvalue_type = decay_t<E>;
        basic_strand(const strand<Execute, Now, C>& o)
            : d(o){
        }
        strand<Execute, Now, C> d;
        virtual clock_time_point_t<basic_strand> now() const {
            return d.now();
        }
        virtual void defer_at(clock_time_point_t<basic_strand> at, observer_interface<re_defer_at_t<C>, E> out) const {
            d.defer_at(at, out);
        }
    };

    template<class Clock>
    struct immediate {
        subscription lifetime;
        template<class... ON>
        void operator()(time_point_t<Clock> at, observer<ON...> out) const {
            auto next = at;
            bool stop = false;
            info("immediate::defer_at");
            while (!stop && !lifetime.is_stopped() && !out.lifetime.is_stopped()) {
                info("immediate::defer_at sleep_until");
                this_thread::sleep_until(next);
                stop = true;
                info("immediate::defer_at next");
                out.next([&](typename Clock::time_point at){
                    info("immediate::defer_at self");
                    stop = false;
                    next = at;
                });
            }
            info("immediate::defer_at complete");
            out.complete();
        }
    };

    template<class Clock>
    struct now {
        time_point_t<Clock> operator()() const {
            return Clock::now();
        }
    };
}

template<class C, class E>
struct strand<interface<C, E>> {
    using clock_type = decay_t<C>;
    using errorvalue_type = decay_t<E>;
    strand(const strand& o) = default;
    template<class Execute, class Now>
    strand(const strand<Execute, Now, C>& o)
        : lifetime(o.lifetime)
        , d(make_shared<detail::basic_strand<C, E, Execute, Now>>(o)) {
    }
    subscription lifetime;
    shared_ptr<detail::abstract_strand<clock_type, errorvalue_type>> d;
    time_point_t<clock_type> now() const {
        return d->now();
    }
    void defer_at(time_point_t<clock_type> at, observer_interface<detail::re_defer_at_t<C>, E> out) const {
        d->defer_at(at, out);
    }
    template<class... TN>
    strand as_interface() const {
        return {*this};
    }
};
template<class C, class E>
using strand_interface = strand<interface<C, E>>;

template<class Execute, class Now, class Clock>
struct strand<Execute, Now, Clock> {
    using clock_type = decay_t<Clock>;
    subscription lifetime;
    Execute e;
    Now n;
    time_point_t<clock_type> now() const {
        return n();
    }
    template<class... ON>
    void defer_at(time_point_t<clock_type> at, observer<ON...> out) const {
        e(at, out);
    }
    template<class E = exception_ptr>
    strand_interface<Clock, E> as_interface() const {
        using strand_t = detail::basic_strand<Clock, E, Execute, Now>;
        return {lifetime, make_shared<strand_t>(*this)};
    }
};

template<class Clock = steady_clock, class Execute = detail::immediate<Clock>, class Now = detail::now<Clock>>
auto make_strand(subscription lifetime, Execute&& e = Execute{}, Now&& n = Now{}) {
    return strand<decay_t<Execute>, decay_t<Now>, Clock>{
        lifetime,
        forward<Execute>(e), 
        forward<Now>(n)
    };
}

template<class Strand>
struct shared_strand {
    using strand_type = decay_t<Strand>;
    template<class F>
    explicit shared_strand(F&& f, detail::shared_strand_construct_t&&) : st(forward<F>(f)) {}
    strand_type st;
    ~shared_strand() {
        info("shared_strand: destroy stop");
        st.lifetime.stop();
    }
};

template<class Strand>
struct shared_strand_maker {
    using strand_type = decay_t<Strand>;
    shared_ptr<shared_strand<strand_type>> ss;
    auto operator()(subscription lifetime) const {
        ss->st.lifetime.insert(lifetime);
        return make_strand<clock_t<strand_type>>(lifetime, 
            [ss = this->ss, lifetime](auto at, auto o){
                lifetime.insert(o.lifetime);
                ss->st.defer_at(at, o);
            },
            [ss = this->ss](){return ss->st.now();});
    }
};


template<class Strand>
auto make_shared_strand_maker(Strand&& s) -> shared_strand_maker<decay_t<Strand>> {
    using strand_type = decay_t<Strand>;
    return shared_strand_maker<strand_type>{make_shared<shared_strand<strand_type>>(forward<Strand>(s), detail::shared_strand_construct_t{})};
}

template<class MakeStrand>
auto make_shared_make_strand(MakeStrand make) {
    auto strand = make(subscription{});
    return make_shared_strand_maker(strand);
}

namespace detail {

template<class T>
using for_strand = for_specialization_of_t<T, strand>;

template<class T>
using not_strand = not_specialization_of_t<T, strand>;

}

template<class... SN, class... ON>
subscription defer(strand<SN...> s, observer<ON...> out) {
    s.defer_at(s.now(), out);
    return out.lifetime;
}
template<class... SN, class... ON>
subscription defer_at(strand<SN...> s, clock_time_point_t<strand<SN...>> at, observer<ON...> out) {
    s.defer_at(at, out);
    return out.lifetime;
}
template<class... SN, class... ON>
subscription defer_after(strand<SN...> s, clock_duration_t<strand<SN...>> delay, observer<ON...> out) {
    s.defer_at(s.now() + delay, out);
    return out.lifetime;
}
template<class... SN, class... ON>
subscription defer_periodic(strand<SN...> s, clock_time_point_t<strand<SN...>> initial, clock_duration_t<strand<SN...>> period, observer<ON...> out) {
    auto lifetime = subscription{};
    auto state = make_state<pair<long, clock_time_point_t<strand<SN...>>>>(lifetime, make_pair(0, initial));
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

}