#pragma once

namespace rx {

inline string what(exception_ptr ep) {
    try {rethrow_exception(ep);}
    catch (const exception& ex) {
        return ex.what();
    }
    return string();
}

struct virtual_clock
{
    using rep = std::int64_t;
    using period = std::milli;
    using duration = std::chrono::duration<rep, period>;
    using time_point = std::chrono::time_point<virtual_clock>;

    static bool is_steady() {
        return false;
    }
    time_point now() const {
        return rightnow;
    }
    void now(time_point at) {
        rightnow = at;
    }
    time_point rightnow;
};

struct lifetime_record
{
    time_point<virtual_clock> start;
    time_point<virtual_clock> stop;
};

bool operator==(const lifetime_record& l, const lifetime_record& r){
    return l.start == r.start && l.stop == r.stop;
}

bool operator!=(const lifetime_record& l, const lifetime_record& r){
    return !(l == r);
}

template<class... OSN>
basic_ostream<OSN...>& operator<<(basic_ostream<OSN...>& os, const lifetime_record& l){
    os << "{" << (l.start - time_point<virtual_clock>{}).count() << ", " << (l.stop - time_point<virtual_clock>{}).count() << "}";
    return os;
}

template<class T, class E>
struct marble_record
{
    using clock_type = virtual_clock;
    using error_type = decay_t<E>;
    using time_point = typename clock_type::time_point;
    using duration = typename clock_type::duration;
    using observer_type = observer_interface<T, error_type>;
    using emitter_type = function<void(observer_type o)>;

    time_point at;
    emitter_type emitter;

    template<class Strand>
    void defer(time_point origin, Strand& s, observer_type r) {
        auto emit = emitter;
        auto lifted = make_observer(
            subscription{}, 
            [=](auto& ) mutable {
                emit(r);
            });
        defer_at(s, origin + (at - time_point{}), lifted);
    }
};

template<class T, class E>
bool operator==(const marble_record<T, E>& l, const marble_record<T, E>& r){
    if (l.at != r.at) return false;
    bool result = false;
    l.emitter(make_observer(subscription{}, 
        [&](auto vl){
            r.emitter(make_observer(subscription{}, [&](auto vr){
                result = vl == vr;
            }, detail::ignore{}));
        },
        [&](auto el){
            r.emitter(make_observer(subscription{}, detail::ignore{}, [&](auto er){
                result = what(el) == what(er);
            }));
        },
        [&](){
            r.emitter(make_observer(subscription{}, detail::ignore{}, detail::ignore{}, [&](){
                result = true;
            }));
        }));
    return result;
}

template<class T, class E>
bool operator!=(const marble_record<T, E>& l, const marble_record<T, E>& r){
    return !(l == r);
}

template<class T, class E, class... OSN>
basic_ostream<OSN...>& operator<<(basic_ostream<OSN...>& os, const marble_record<T, E>& m){
    auto countms = (m.at - time_point<virtual_clock>{}).count();
    m.emitter(make_observer(subscription{}, 
        [&](auto v){
            os << "next@" << countms << "{" << v << "}";
        },
        [&](auto e){
            os << "error@" << countms << "{" << what(e) << "}";
        },
        [&](){
            os << "complete@" << countms << "{}";
        }));
    return os;
}

template<class T, class E>
bool operator==(const std::vector<marble_record<T, E>>& l, const std::vector<marble_record<T, E>>& r){
    return l.size() == r.size() && std::equal(l.begin(), l.end(), r.begin());
}

template<class T, class E>
bool operator!=(const std::vector<marble_record<T, E>>& l, const std::vector<marble_record<T, E>>& r){
    return !(l == r);
}

template<class T, class E, class... OSN>
basic_ostream<OSN...>& operator<<(basic_ostream<OSN...>& os, const std::vector<marble_record<T, E>>& mv){
    os << "[";
    for(auto& m : mv) {
        os << m << ", ";
    }
    os << "]";
    return os;
}

template<class Error = exception_ptr>
struct test_loop;

template<class T, class Error = exception_ptr>
struct recorded
{
    using clock_type = virtual_clock;
    using error_type = decay_t<Error>;
    using time_point = typename clock_type::time_point;
    using duration = typename clock_type::duration;
    using observer_type = observer_interface<T, error_type>;
    using marble_record = rx::marble_record<T, Error>;

    time_point origin() const {
        return time_point{};
    }

    lifetime_record lifespan(time_point start = time_point{} + duration{200}, time_point stop = time_point{} + duration{1000}) const {
        return lifetime_record{start, stop};
    }

    marble_record next(time_point at, T v) const {
        return marble_record{at, [v](observer_type o){
            o.next(v);
        }};
    }
    marble_record error(time_point at, error_type e) const {
        return marble_record{at, [e](observer_type o){
            o.error(e);
        }};
    }
    marble_record complete(time_point at) const {
        return marble_record{at, [](observer_type o){
            o.complete();
        }};
    }

    std::vector<marble_record> expected(std::vector<marble_record> marbles) const {
        return marbles;
    }

    auto hot(std::vector<marble_record> marbles) const {
        info("new hot");
        return make_observable([=](auto scrb){
            info("hot bound to subscriber");
            return make_starter([=](auto ctx) {
                info("hot bound to context");
                auto r = scrb.create(ctx);
                for(auto m : marbles){
                    m.defer(ctx.get().origin, ctx, r);
                }
                info("hot started");
                return ctx.lifetime;
            });
        });
    };

    auto cold(std::vector<marble_record> marbles) const {
        info("new cold");
        return make_observable([=](auto scrb){
            info("cold bound to subscriber");
            return make_starter([=](auto ctx) {
                info("cold bound to context");
                auto r = scrb.create(ctx);
                auto origin = ctx.now();
                for(auto m : marbles){
                    m.defer(origin, ctx, r);
                }
                info("cold started");
                return ctx.lifetime;
            });
        });
    };

    struct test_result
    {
        time_point origin;
        mutable lifetime_record lifespan;
        mutable map<string, vector<marble_record>> marbles;
    };

    auto record(string key) const {
        info("new record");
        return make_lifter([=](auto scbr){
            info("record bound to subscriber");
            return make_subscriber([=](auto ctx){
                info("record bound to context lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(ctx.lifetime.store.get())));
                auto marbles = addressof(ctx.get().marbles[key]);
                auto r = scbr.create(ctx);
                info("record observer lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(r.lifetime.store.get())));
                return make_observer(r, r.lifetime, 
                [=](auto& r, auto v) {
                    marbles->push_back(marble_record{ctx.now(), [v](observer_type o){
                        o.next(v);
                    }});
                    r.next(v);
                },
                [=](auto& r, error_type e){
                    marbles->push_back(marble_record{ctx.now(), [e](observer_type o){
                        o.error(e);
                    }});
                    r.error(e);
                },
                [=](auto& r){
                    marbles->push_back(marble_record{ctx.now(), [](observer_type o){
                        o.complete();
                    }});
                    r.complete();
                });
            });
        });
    };

    template<class... TN>
    auto test(test_loop<TN...>& tl, lifetime_record l = lifetime_record{}) {
        return make_terminator([=](auto source){

            auto test = source.bind(
                make_subscriber([=](auto ctx){
                    info("run bound to context lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(ctx.lifetime.store.get())));
                    return make_observer(ctx.lifetime, detail::ignore{});
                })
            );

            auto ctx = make_context<test_result, virtual_clock>(subscription{}, tl.make());

            ctx.get().origin = ctx.now();
            ctx.get().lifespan.start = ctx.now();
            ctx.lifetime.insert([=](){
                ctx.get().lifespan.stop = ctx.now();
                info("run stopped " + to_string((ctx.get().lifespan.start - time_point{}).count()) + 
                    "-" + to_string((ctx.get().lifespan.stop - time_point{}).count()));
            });

            auto liftedStart = make_observer(
                subscription{}, 
                [=](auto& ) mutable {
                    ctx.get().lifespan.start = ctx.now();
                    test.start(ctx);
                });
            defer_at(ctx, ctx.get().origin + (l.start - time_point{}), liftedStart);
            
            auto liftedStop = make_observer(
                subscription{}, 
                [=](auto& ) mutable {
                    ctx.lifetime.stop();
                });
            defer_at(ctx, ctx.get().origin + (l.stop - time_point{}), liftedStop);

            return ctx;
        });
    }

};

template<class Error>
struct test_loop {
    using clock_type = virtual_clock;
    using error_type = decay_t<Error>;
    using observer_type = observer_interface<detail::re_defer_at_t<clock_type>, error_type>;
    using item_type = observe_at<clock_type, observer_type>;
    using queue_type = observe_at_queue<clock_type, observer_type>;

    struct state_type {
        ~state_type() {
            info(to_string(reinterpret_cast<ptrdiff_t>(this)) + " - test_loop: state_type destroy");
        }
        virtual_clock clock;
        queue_type deferred;
    };

    subscription lifetime;
    state<state_type> state;
    
    explicit test_loop(subscription l = subscription{}, clock_type c = clock_type{}) 
        : lifetime(l)
        , state(make_state<state_type>(lifetime, state_type{c, queue_type{}})) {
    }
    ~test_loop(){
        info(to_string(reinterpret_cast<ptrdiff_t>(addressof(state.get()))) + " - test_loop: destroy");
    }

    void call(item_type& next) const {
        info("test_loop: call");

        state.get().clock.now(next.when);

        auto& deferred = state.get().deferred;
        bool complete = true;
        next.what.next([&](time_point<clock_type> at){
            info(to_string(reinterpret_cast<ptrdiff_t>(addressof(state.get()))) + " - test_loop: call self");
            if (lifetime.is_stopped() || next.what.lifetime.is_stopped()) return;
            info(to_string(reinterpret_cast<ptrdiff_t>(addressof(state.get()))) + " - test_loop: call push self");
            next.when = at;
            deferred.push(next);
            complete = false;
        });
        if (complete) {
            info(to_string(reinterpret_cast<ptrdiff_t>(addressof(state.get()))) + " - test_loop: call complete");
            next.what.complete();
        }
    }

    void step(typename clock_type::duration d) const {
        auto& deferred = state.get().deferred;
        auto stop = state.get().clock.now() + d;
        while (!state.lifetime.is_stopped() && !deferred.empty() && state.get().clock.now() < stop) {
            info(to_string(reinterpret_cast<ptrdiff_t>(addressof(state.get()))) + " - test_loop: step");

            auto next = move(deferred.top());
            deferred.pop();
            
            call(next);
        }
    }
    
    void run() const {
        info(to_string(reinterpret_cast<ptrdiff_t>(addressof(state.get()))) + " - test_loop: run");
        while (!state.get().deferred.empty()) {
            step(3600s);
            break;
        }
        info(to_string(reinterpret_cast<ptrdiff_t>(addressof(state.get()))) + " - test_loop: exit");
    }

    struct strand {
        subscription lifetime;
        rx::state<state_type> state;
        
        template<class... OON>
        void operator()(time_point<clock_type> at, observer<OON...> out) const {
            lifetime.insert(out.lifetime);
            state.get().deferred.push(item_type{at, out});
            info(to_string(reinterpret_cast<ptrdiff_t>(addressof(state.get()))) + " - test_loop: defer_at notify_all");
        }
    };
    struct now
    {
        rx::state<state_type> state;
        time_point_t<virtual_clock> operator()() const {
            return state.get().clock.now();
        }
    };

    auto make() const {
        return [state = this->state](subscription lifetime) {
            state.lifetime.insert(lifetime);
            return make_strand<clock_type>(lifetime, strand{lifetime, state}, now{state});
        };
    }
};

}