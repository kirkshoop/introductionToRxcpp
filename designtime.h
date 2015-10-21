#pragma once

namespace designtimedef 
{

template<class Payload = void>
struct state
{
    explicit state(Payload* p) : p(p) {}
    Payload& get() {
        return *p;
    }
    Payload& get() const {
        return *p;
    }
private:
    mutable Payload* p;
};
template<>
struct state<void>
{
};

///
/// \brief A subscription represents the scope of an async operation. Holds a set of nested lifetimes. Can be used to make state that is scoped to the subscription. Can call arbitratry functions at the end of the lifetime.
///
struct subscription
{
private:
    struct shared
    {
        ~shared(){
            auto expired = std::move(destructors);
            for (auto& d : expired) {
                d();
            }
        }
        shared() : stopped(false) {cout << "new lifetime" << endl;}
        bool stopped;
        set<subscription> others;
        deque<function<void()>> stoppers;
        deque<function<void()>> destructors;
    };
public:
    subscription() : store(make_shared<shared>()) {}
    explicit subscription(shared_ptr<shared> o) : store(o) {}
    /// \brief used to exit loops or otherwise stop work scoped to this subscription.
    /// \returns bool - if true do not access any state objects.
    bool is_stopped() const {
        return store->stopped;
    }
    /// \brief 
    void insert(const subscription& s) const {
        if (s == *this) {std::abort();}
        // nest
        store->others.insert(s);
        // unnest when child is stopped
        weak_ptr<shared> p = store;
        weak_ptr<shared> c = s.store;
        s.insert([p, c](){
            auto storep = p.lock();
            auto storec = c.lock();
            if (storep && storec) {
                auto that = subscription(storep);
                auto s = subscription(storec);
                that.erase(s);
            }
        });
        if (store->stopped) stop();
    }
    void erase(const subscription& s) const {
        if (s == *this) {std::abort();}
        store->others.erase(s);
    }
    void insert(function<void()> stopper) const {
        store->stoppers.emplace_front(stopper);
        if (store->stopped) stop();
    }
    template<class Payload, class... ArgN>
    state<Payload> make_state(ArgN... argn) const {
        auto p = make_unique<Payload>(argn...);
        auto result = state<Payload>{p.get()};
        store->destructors.emplace_front(
            [d=p.release()]() mutable {
                auto p = d; 
                d = nullptr; 
                delete p;
            });
        return result;
    }
    void stop() const {
        store->stopped = true;
        {
            auto others = std::move(store->others);
            for (auto& o : others) {
                o.stop();
            }
        }
        {
            auto stoppers = std::move(store->stoppers);
            for (auto& s : stoppers) {
                s();
            }
        }
    }
private:
    shared_ptr<shared> store;
    friend bool operator==(const subscription&, const subscription&);
    friend bool operator<(const subscription&, const subscription&);
};
bool operator==(const subscription& lhs, const subscription& rhs) {
    return lhs.store == rhs.store;
}
bool operator!=(const subscription& lhs, const subscription& rhs) {
    return !(lhs == rhs);
}
bool operator<(const subscription& lhs, const subscription& rhs) {
    return lhs.store < rhs.store;
}

template<class DoNow, class DoLater, class Clock>
struct strand
{
    using time_point = typename Clock::time_point;

    DoNow n;
    DoLater l;
    Clock clock;
    subscription lifetime;

    template<class Dest>
    void operator()(Dest d) {
        if (lifetime != d.lifetime){
            lifetime.insert(d.lifetime);
        }
        n(clock, d);
    }
    template<class Dest>
    void operator()(time_point when, Dest d) {
        if (lifetime != d.lifetime){
            lifetime.insert(d.lifetime);
        }
        l(clock, when, d);
    }
};

template<class Create, class Clock>
struct scheduler 
{
    Create c;
    Clock clock;
    auto operator()(subscription s = subscription{}) {
        return c(clock, s);
    }
};

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


template<class Next, class Error, class Complete, class State, class Dest>
struct receiver;

template<class T>
struct receiver_check : public false_type {};

template<class Next, class Error, class Complete, class State, class Dest>
struct receiver_check<receiver<Next, Error, Complete, State, Dest>> : public true_type {};

template<class T>
using for_receiver = enable_if_t<receiver_check<std::decay_t<T>>::value>;

template<class T>
using not_receiver = enable_if_t<!receiver_check<std::decay_t<T>>::value>;

template<class T>
struct subscription_check : public false_type {};

template<>
struct subscription_check<subscription> : public true_type {};

template<class T>
using for_subscription = enable_if_t<subscription_check<std::decay_t<T>>::value>;

template<class T>
using not_subscription = enable_if_t<!subscription_check<std::decay_t<T>>::value>;

struct noop
{
    // next
    template<class V, class CheckR = not_receiver<V>, class CheckS = not_subscription<V>, class unique = void>
    void operator()(V&&) const {
    }
    // complete
    inline void operator()() const {
    }
    // lifetime next
    template<class V, class Check = not_receiver<V>>
    void operator()(const subscription& s, V&&) const {
    }
    // lifetime complete
    inline void operator()(const subscription& s) const {
    }
    // delegating next
    template<class Dest, class V, class CheckD = for_receiver<Dest>, class CheckV = not_receiver<V>>
    void operator()(Dest&& d, V&& v) const {
        d(std::forward<V>(v));
    }
    // delegating complete
    template<class Dest, class Check = for_receiver<Dest>>
    void operator()(Dest&& d) const {
        d();
    }
};
struct ignore
{
    inline void operator()(exception_ptr) const {
    }
    inline void operator()(const subscription&, exception_ptr) const {
    }
    template<class Dest, class CheckD = for_receiver<Dest>>
    void operator()(Dest&& d, exception_ptr ep) const {
        d(ep);
    }
    template<class Dest, class Payload, 
        class CheckD = for_receiver<Dest>, 
        class CheckP = not_receiver<Payload>>
    void operator()(Dest&& d, Payload&, exception_ptr ep) const {
        d(ep);
    }
};
struct fail
{
    template<class Payload, class CheckP = not_receiver<Payload>>
    void operator()(Payload&, exception_ptr ep) const {
        cout << "abort! " << what(ep) << endl << flush;
        std::abort();
    }
    inline void operator()(const subscription&, exception_ptr ep) const {
        cout << "abort! " << what(ep) << endl << flush;
        std::abort();
    }
    inline void operator()(exception_ptr ep) const {
        cout << "abort! " << what(ep) << endl << flush;
        std::abort();
    }
};

//stateless
template<class Next, class Error, class Complete>
struct receiver<Next, Error, Complete, state<>, void>
{
    Next n;
    Error e;
    Complete c;
    template<class V, class Strand, class Check = enable_if_t<!is_same<std::decay_t<V>, exception_ptr>::value>>
    void operator()(const subscription& lifetime, const Strand& strand, V&& v) const {
        report(end(lifetime, e, lifetime), enforce(lifetime, n), lifetime, std::forward<V>(v));
    }
    template<class Strand>
    void operator()(const subscription& lifetime, const Strand& strand, exception_ptr ep) const {
        report(fail{}, end(lifetime, e), lifetime, ep);
    }
    template<class Strand>
    void operator()(const subscription& lifetime, const Strand& strand) const {
        report(fail{}, end(lifetime, c), lifetime);
    }
};
//stateful
template<class Next, class Error, class Complete, class Payload>
struct receiver<state<Payload>, Next, Error, Complete, void>
{
    mutable state<Payload> s;
    Next n;
    Error e;
    Complete c;
    template<class V, class Strand, class Check = enable_if_t<!is_same<std::decay_t<V>, exception_ptr>::value>>
    void operator()(const subscription& lifetime, const Strand& strand, V&& v) const {
        report(end(lifetime, e, lifetime, s.get()), enforce(lifetime, n), lifetime, s.get(), std::forward<V>(v));
    }
    template<class Strand>
    void operator()(const subscription& lifetime, const Strand& strand, exception_ptr ep) const {
        report(fail{}, end(lifetime, e), lifetime, s.get(), ep);
    }
    template<class Strand>
    void operator()(const subscription& lifetime, const Strand& strand) const {
        report(fail{}, end(lifetime, c), lifetime, s.get());
    }
};
// stateless delegating
template<class DNext, class DError, class DComplete, class DState, class DDest, class Next, class Error, class Complete>
struct receiver<receiver<DNext, DError, DComplete, DState, DDest>, Next, Error, Complete, state<>>
{
    using Dest = receiver<DNext, DError, DComplete, DState, DDest>;
    Dest d;
    Next n;
    Error e;
    Complete c;
    template<class V, class Strand, class Check = enable_if_t<!is_same<std::decay_t<V>, exception_ptr>::value>>
    void operator()(const subscription& lifetime, const Strand& strand, V&& v) const {
        report(end(lifetime, e, d), enforce(lifetime, n), d, std::forward<V>(v));
    }
    template<class Strand>
    void operator()(const subscription& lifetime, const Strand& strand, exception_ptr ep) const {
        report(fail{}, end(lifetime, e), d, ep);
    }
    template<class Strand>
    void operator()(const subscription& lifetime, const Strand& strand) const {
        report(fail{}, end(lifetime, c), d);
    }
};
// stateful delegating
template<class DNext, class DError, class DComplete, class DState, class DDest, class Next, class Error, class Complete, class Payload>
struct receiver<receiver<DNext, DError, DComplete, DState, DDest>, state<Payload>, Next, Error, Complete>
{
    using Dest = receiver<DNext, DError, DComplete, DState, DDest>;
    Dest d;
    mutable state<Payload> s;
    Next n;
    Error e;
    Complete c;
    template<class V, class Strand, class Check = enable_if_t<!is_same<std::decay_t<V>, exception_ptr>::value>>
    void operator()(const subscription& lifetime, const Strand& strand, V&& v) const {
        report(end(lifetime, e, d, s.get()), enforce(lifetime, n), d, s.get(), std::forward<V>(v));
    }
    template<class Strand>
    void operator()(const subscription& lifetime, const Strand& strand, exception_ptr ep) const {
        report(fail{}, end(lifetime, e), d, s.get(), ep);
    }
    template<class Strand>
    void operator()(const subscription& lifetime, const Strand& strand) const {
        report(fail{}, end(lifetime, c), d, s.get());
    }
};


//stateless
template<class Next = noop, class Error = fail, class Complete = noop,
class CheckN = not_receiver<Next>,
class CheckE = not_receiver<Error>,
class CheckC = not_receiver<Complete>>
auto make_receiver(Next n = Next{}, Error e = Error{}, Complete c = Complete{}) {
    return receiver<std::decay_t<Next>, std::decay_t<Error>, std::decay_t<Complete>, state<>, void>{n, e, c};
}
//stateful
template<class Payload, class Next = noop, class Error = fail, class Complete = noop,
class CheckN = not_receiver<Next>,
class CheckE = not_receiver<Error>,
class CheckC = not_receiver<Complete>>
auto make_receiver(state<Payload> s, Next n = Next{}, Error e = Error{}, Complete c = Complete{}) {
    return receiver<state<Payload>, std::decay_t<Next>, std::decay_t<Error>, std::decay_t<Complete>, void>{s, n, e, c};
}
// stateless delegating
template<class Dest, class Next = noop, class Error = ignore, class Complete = noop,
class CheckD = for_receiver<Dest>,
class CheckN = not_receiver<Next>,
class CheckE = not_receiver<Error>,
class CheckC = not_receiver<Complete>,
class unique = void>
auto make_receiver(Dest d, Next n = Next{}, Error e = Error{}, Complete c = Complete{}) {
    return receiver<std::decay_t<Dest>, std::decay_t<Next>, std::decay_t<Error>, std::decay_t<Complete>, state<>>{d, n, e, c};
}
// stateful delegating
template<class Dest, class Payload, class Next = noop, class Error = ignore, class Complete = noop,
class CheckD = for_receiver<Dest>,
class CheckN = not_receiver<Next>,
class CheckE = not_receiver<Error>,
class CheckC = not_receiver<Complete>>
auto make_receiver(Dest d, state<Payload> s, Next n = Next{}, Error e = Error{}, Complete c = Complete{}) {
    return receiver<std::decay_t<Dest>, state<Payload>, std::decay_t<Next>, std::decay_t<Error>, std::decay_t<Complete>>{d, s, n, e, c};
}

template<class Subscribe>
struct sender
{
    Subscribe subscribe;
    template<class Dest>
    subscription operator()(Dest&& dest) const {
        return subscribe(std::forward<Dest>(dest));
    }
};

template<class T>
struct sender_check : public false_type {};

template<class Subscribe>
struct sender_check<sender<Subscribe>> : public true_type {};

template<class T>
using for_sender = enable_if_t<sender_check<std::decay_t<T>>::value>;

template<class T>
using not_sender = enable_if_t<!sender_check<std::decay_t<T>>::value>;

template<class Subscribe, class CheckS = not_sender<Subscribe>>
auto make_sender(Subscribe s) {
    return sender<std::decay_t<Subscribe>>{s};
}

auto immediate = make_scheduler<system_clock>([](){
    return make_strand<system_clock>(
        [](auto& dest){dest()},
        [](system_clock::time_point when, auto& dest){});
});

template<class Start>
struct starter
{
    Start start;
    template<class Strand>
    subscription operator()(subscribe lifetime = subscription{}, Strand strand = immediate()) const {
        return subscribe(std::forward<Dest>(dest));
    }
};

template<class T>
struct sender_check : public false_type {};

template<class Subscribe>
struct sender_check<sender<Subscribe>> : public true_type {};

template<class T>
using for_sender = enable_if_t<sender_check<std::decay_t<T>>::value>;

template<class T>
using not_sender = enable_if_t<!sender_check<std::decay_t<T>>::value>;

template<class Subscribe, class CheckS = not_sender<Subscribe>>
auto make_sender(Subscribe s) {
    return sender<std::decay_t<Subscribe>>{s};
}


template<class Lift>
struct lifter
{
    Lift lift;
    template<class Dest>
    auto operator()(Dest&& dest) const {
        return lift(std::forward<Dest>(dest));
    }
};

template<class T>
struct lifter_check : public false_type {};

template<class Lift>
struct lifter_check<lifter<Lift>> : public true_type {};

template<class T>
using for_lifter = enable_if_t<lifter_check<std::decay_t<T>>::value>;

template<class T>
using not_lifter = enable_if_t<!lifter_check<std::decay_t<T>>::value>;

template<class Lift, class CheckS = not_lifter<Lift>>
auto make_lifter(Lift l) {
    return lifter<std::decay_t<Lift>>{l};
}

const auto ints = [](auto first, auto last){
    cout << "new ints" << endl;
    return make_sender([=](auto dest){
        cout << "ints bound to dest" << endl;
        return [=](subscription lifetime, auto strand){
            for(auto i = first;i != last && !lifetime.is_stopped(); ++i){
                dest(lifetime, strand, i);
            }
            dest(lifetime, strand);
            return lifetime;
        }
    });
};
/*
const auto async_ints = [](auto first, auto last){
    cout << "new async_ints" << endl;
    return make_sender([=](auto dest){
        cout << "async_ints bound to dest" << endl;
        auto store = dest.lifetime.template make_state<std::decay_t<decltype(first)>>(first);
        auto sched = jsthread.create_coordinator().get_scheduler().create_worker();
        auto tick = [store, dest, sched, last](const schedulable& sb){
            if (dest.lifetime.is_stopped()) {return;}
            auto& current = store.get();
            if (current == last) {
                dest(current); 
            } else {
                dest(current++);
            }
            if (current != last) {
                sb.schedule();
                return;
            }
            dest();
        };
        sched.schedule(tick);
        return dest.lifetime;
    });
};
*/
const auto copy_if = [](auto pred){
    cout << "new copy_if" << endl;
    return make_lifter([=](auto dest){
        cout << "copy_if bound to dest" << endl;
        return make_receiver(dest, [=](auto& d, subscription lifetime, auto strand, auto v){
            if (pred(v)) d(lifetime, strand, v);
        });
    });
};
/*
const auto last_or_default = [](auto def){
        cout << "new last_or_default" << endl;
    return make_lifter([=](auto dest){
        cout << "last_or_default bound to dest" << endl;
        auto last = dest.lifetime.template make_state<std::decay_t<decltype(def)>>(def);
        return make_receiver(dest, last, 
            [](auto& d, auto& l, auto v){
                l = v;
            },
            [](auto& d, auto& l, exception_ptr ep) {
                d(ep);
            },
            [](auto& d, auto& l){
                d(l);
                d();
            });
    });
};
const auto take = [](int n){
    cout << "new take" << endl;
    return [=](auto source){
        return make_sender([=](auto dest){
            cout << "take bound to dest" << endl;
            auto remaining = dest.lifetime.template make_state<int>(n);
            return source(make_receiver(dest, remaining, 
                [](auto& d, auto& r, auto v){
                    if (r-- == 0) {
                        d();
                        return;
                    }
                    d(v);
                }));
        });
    };
};
*/
const auto printto = [](auto& output){
    cout << "new printto" << endl;
    subscription lifetime;
    auto values = lifetime.template make_state<int>(0);
    return make_receiver(
        lifetime,
        values,
        [&](auto& c, subscription lifetime, auto strand, auto v) {
            ++c;
            output << v << endl;
        },
        [&](auto& c, subscription lifetime, auto strand, exception_ptr ep){
            output << what(ep) << endl;
        },
        [&](auto& c, subscription lifetime, auto strand){
            output << c << " values received - done!" << endl;
        });
};

/// \brief chain operator overload for
/// subscription = sender | receiver
/// \param sender
/// \param receiver
/// \returns subscription
template<class SenderV, class ReceiverV, 
    class CheckS = for_sender<SenderV>, 
    class CheckR = for_receiver<ReceiverV>>
subscription operator|(SenderV sv, ReceiverV rv) {
    return sv(rv);
}

/// \brief chain operator overload for
/// sender = sender | lifter
/// \param sender
/// \param lifter
/// \returns sender
template<class Sender, class Lifter, 
    class CheckS = for_sender<Sender>, 
    class CheckL = for_lifter<Lifter>, 
    class _5 = void>
auto operator|(Sender s, Lifter l) {
    return make_sender([=](auto dest){
        return s(l(dest));
    });
}

/// \brief chain operator overload for
/// receiver = lifter | receiver
/// \param lifter
/// \param receiver
/// \returns receiver
template<class Lifter, class Receiver, 
    class CheckL = for_lifter<Lifter>, 
    class CheckR = for_receiver<Receiver>, 
    class _5 = void, 
    class _6 = void>
auto operator|(Lifter l, Receiver r) {
    return l(r);
}

/// \brief chain operator overload for
/// lifter = lifter | lifter
/// \param lifter
/// \param lifter
/// \returns lifter
template<class LifterL, class LifterR, 
    class CheckL = for_lifter<LifterL>, 
    class CheckR = for_lifter<LifterR>,
    class _5 = void, 
    class _6 = void, 
    class _7 = void>
auto operator|(LifterL ll, LifterR lr){
    return make_lifter([=](auto dest){
        return ll(lr(dest));
    });
}

/// \brief chain operator overload for both
/// sender = sender | algorithm
/// and
/// subscription = sender | subscriber
/// \param sender
/// \param algorithm
/// \param subscriber
/// \returns sender
/// \returns subscription
template<class SenderV, class Algorithm, 
    class CheckV = for_sender<SenderV>, 
    class CheckS = not_sender<Algorithm>, 
    class CheckL = not_lifter<Algorithm>, 
    class CheckR = not_receiver<Algorithm>,
    class _7 = void, 
    class _8 = void>
auto operator|(SenderV sv, Algorithm al){
    return al(sv);
}

/// \brief chain operator overload for
/// subscriber = lifter | algorithm
/// \param lifter
/// \param algorithm
/// \returns subscriber
template<class Lifter, class Algorithm, 
    class CheckL = for_lifter<Lifter>, 
    class CheckAS = not_sender<Algorithm>, 
    class CheckAL = not_lifter<Algorithm>, 
    class CheckAR = not_receiver<Algorithm>,
    class _7 = void, 
    class _8 = void, 
    class _9 = void>
auto operator|(Lifter l, Algorithm al){
    return [=](auto source){
        return make_sender([=](auto dest){
            return al(source)(l(dest));
        });
    };
}

/// \brief chain operator overload for
/// receiver = algorithm | receiver
/// \param algorithm
/// \param receiver
/// \returns receiver
template<class Algorithm, class Receiver, 
    class CheckAS = not_sender<Algorithm>, 
    class CheckAL = not_lifter<Algorithm>, 
    class CheckAR = not_receiver<Algorithm>,
    class CheckL = for_receiver<Receiver>, 
    class _7 = void, 
    class _8 = void, 
    class _9 = void, 
    class _10 = void>
auto operator|(Algorithm al, Receiver r){
    return [=](auto source){
        return al(source)(r);
    };
}

void testoperator(){
    {
        // sender = sender | lifter
    auto even$ = ints(0, 1) | copy_if(even);
        // subscription = sender | receiver
    subscription lifetime = even$ | printto(cout);
    }
    {
        // lifter = lifter | lifter
    auto lasteven = copy_if(even) | last_or_default(42);
        // receiver = lifter | receiver
    auto printlasteven = lasteven | printto(cout);
        // subscription = sender | receiver
    auto lifetime = ints(0, 1) | printlasteven;
    }

    {
        // algorithm = lifter | algorithm
    auto take3even = copy_if(even) | take(3);
        // subscriber = algorithm | receiver
    auto print3even = take3even | printto(cout);
        // sender = sender | algorithm
    auto source = async_ints(0, 1) | take(3);
        // subscription = sender | subscriber
    auto lifetime = source | print3even;
    }
}

}

extern"C" void EMSCRIPTEN_KEEPALIVE designtime(int first, int last, int count){
    using namespace designtimedef;
    auto takeandprint = take(count) | printto(cout);
    auto lifetime1 = async_ints(first, last) | designtimedef::copy_if(even) | takeandprint;
    lifetime1.insert([](){cout << "stopped1" << endl;});
    lifetime1.template make_state<destruction>();

    auto lifetime2 = async_ints(first, last * 2) | designtimedef::copy_if(even) | takeandprint;
    lifetime2.insert([](){cout << "stopped2" << endl;});
    lifetime2.template make_state<destruction>();
}

