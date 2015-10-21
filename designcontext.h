#pragma once

#include <sstream>

namespace designcontextdef {

namespace shapes {

template<class Payload = void>
struct state;

struct subscription
{
    bool is_stopped() const;
    void insert(const subscription& s) const;
    void erase(const subscription& s) const;
    void insert(function<void()> stopper) const;
    state<> make_state(subscription) const;
    template<class Payload, class... ArgN>
    state<Payload> make_state(subscription, ArgN... argn) const;
    void stop() const;
};

template<class Payload>
struct state {
    subscription lifetime;
    Payload& get();
};

struct starter {
    template<class Context>
    subscription start(Context);
};

struct receiver {
    template<class T>
    void next(T);
    template<class E>
    void error(E);
    void complete();
};

struct subscriber {
    template<class Context>
    receiver create(Context);
};

struct sender {
    starter bind(subscriber);
};

struct lifter {
    subscriber lift(subscriber);
};

struct adaptor {
    sender adapt(sender);
};

struct terminator {
    starter terminate(sender);
};

}

using std::decay_t;

template<class Payload = void>
struct state;

///
/// \brief A subscription represents the scope of an async operation. 
/// Holds a set of nested lifetimes. 
/// Can be used to make state that is scoped to the subscription. 
/// Can call arbitratry functions at the end of the lifetime.
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
        shared() : stopped(false) {info("new lifetime");}
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
    state<Payload> make_state(ArgN... argn) const;
    state<> make_state() const;
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

template<class Payload>
struct state
{
    subscription lifetime;
    state(subscription l, Payload* p) : lifetime(l), p(p) {}
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
    subscription lifetime;
};

template<class Payload, class... ArgN>
state<Payload> subscription::make_state(ArgN... argn) const {
    auto p = make_unique<Payload>(argn...);
    auto result = state<Payload>{*this, p.get()};
    store->destructors.emplace_front(
        [d=p.release()]() mutable {
            auto p = d; 
            d = nullptr; 
            delete p;
        });
    return result;
}
state<> subscription::make_state() const {
    auto result = state<>{*this};
    return result;
}

namespace detail {

template<class T>
struct state_check : public false_type {};

template<class Payload>
struct state_check<state<Payload>> : public true_type {};

template<class T>
using for_state = enable_if_t<state_check<std::decay_t<T>>::value>;

template<class T>
using not_state = enable_if_t<!state_check<std::decay_t<T>>::value>;


template<class T>
struct subscription_check : public false_type {};

template<>
struct subscription_check<subscription> : public true_type {};

template<class T>
using for_subscription = enable_if_t<subscription_check<std::decay_t<T>>::value>;

template<class T>
using not_subscription = enable_if_t<!subscription_check<std::decay_t<T>>::value>;

}


template<class Start>
struct starter {
    Start s;
    template<class Context>
    subscription start(Context&& c) const {
        return s(forward<Context>(c));
    }
};
template<class Start>
starter<Start> make_starter(Start&& s) {
    return {forward<Start>(s)};
}

namespace detail {

template<class T>
struct starter_check : public false_type {};

template<class Start>
struct starter_check<starter<Start>> : public true_type {};

template<class T>
using for_starter = enable_if_t<starter_check<decay_t<T>>::value>;

template<class T>
using not_starter = enable_if_t<!starter_check<decay_t<T>>::value>;

}

template<class Next, class Error, class Complete, class Delegatee = void>
struct receiver;

namespace detail {
    
template<class T>
struct receiver_check : public false_type {};

template<class Next, class Error, class Complete, class Delegatee>
struct receiver_check<receiver<Next, Error, Complete, Delegatee>> : public true_type {};

template<class T>
using for_receiver = enable_if_t<receiver_check<decay_t<T>>::value>;

template<class T>
using not_receiver = enable_if_t<!receiver_check<decay_t<T>>::value>;

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
    template<class V, class CheckV = not_receiver<V>>
    void operator()(V&&) const {
    }
    template<class Delegatee, class V>
    void operator()(const Delegatee& d, V&& v) const {
        d.next(std::forward<V>(v));
    }
    inline void operator()() const {
    }
    template<class Delegatee, class Check = for_receiver<Delegatee>>
    void operator()(const Delegatee& d) const {
        d.complete();
    }
};
struct ignore
{
    template<class E>
    void operator()(E&&) const {
    }
    template<class Delegatee, class E, class CheckD = for_receiver<Delegatee>>
    void operator()(const Delegatee& d, E&& e) const {
        d.error(forward<E>(e));
    }
};
struct fail
{
    template<class E>
    void operator()(E&&) const {
        info("abort! ");
        std::abort();
    }
    template<class Delegatee, class E, class CheckD = for_receiver<Delegatee>>
    void operator()(const Delegatee&, E&&) const {
        info("abort! ");
        std::abort();
    }
};

}

template<class Next, class Error, class Complete>
struct receiver<Next, Error, Complete, void> {
    subscription lifetime;
    Next n;
    Error e;
    Complete c;
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
};
template<class Delegatee, class Next, class Error, class Complete>
struct receiver {
    Delegatee d;
    subscription lifetime;
    Next n;
    Error e;
    Complete c;
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
};

template<class Next = detail::noop, class Error = detail::fail, class Complete = detail::noop, 
    class CheckN = detail::not_receiver<Next>>
auto make_receiver(subscription lifetime, Next&& n = Next{}, Error&& e = Error{}, Complete&& c = Complete{}) {
    return receiver<decay_t<Next>, decay_t<Error>, decay_t<Complete>, void>{
        lifetime,
        forward<Next>(n), 
        forward<Error>(e), 
        forward<Complete>(c)
    };
}

template<class Delegatee, class Next = detail::noop, class Error = detail::fail, class Complete = detail::noop, 
    class CheckD = detail::for_receiver<Delegatee>>
auto make_receiver(Delegatee&& d, subscription lifetime, Next&& n = Next{}, Error&& e = Error{}, Complete&& c = Complete{}) {
    return receiver<decay_t<Delegatee>, decay_t<Next>, decay_t<Error>, decay_t<Complete>>{
        forward<Delegatee>(d), 
        lifetime,
        forward<Next>(n), 
        forward<Error>(e), 
        forward<Complete>(c)
    };
}

template<class Create>
struct subscriber {
    Create c;
    /// \brief returns receiver
    template<class Context>
    auto create(Context& ctx) const {
        return c(ctx);
    }
};

template<class Create>
subscriber<Create> make_subscriber(Create&& c) {
    return {forward<Create>(c)};
}

auto make_subscriber() {
    return make_subscriber([](auto ctx){return make_receiver(ctx.lifetime);});
}

namespace detail {

template<class T>
struct subscriber_check : public false_type {};

template<class Create>
struct subscriber_check<subscriber<Create>> : public true_type {};

template<class T>
using for_subscriber = enable_if_t<subscriber_check<decay_t<T>>::value>;

template<class T>
using not_subscriber = enable_if_t<!subscriber_check<decay_t<T>>::value>;

}

template<class Bind>
struct sender {
    Bind b;
    /// \brief returns starter
    template<class Subscriber>
    auto bind(Subscriber&& s) const {
        return b(s);
    }
};

template<class Bind>
sender<Bind> make_sender(Bind&& b) {
    return {forward<Bind>(b)};
}

namespace detail {

template<class T>
struct sender_check : public false_type {};

template<class Bind>
struct sender_check<sender<Bind>> : public true_type {};

template<class T>
using for_sender = enable_if_t<sender_check<decay_t<T>>::value>;

template<class T>
using not_sender = enable_if_t<!sender_check<decay_t<T>>::value>;

}


template<class Lift>
struct lifter {
    Lift l;
    /// \brief returns subscriber    
    template<class Subscriber>
    auto lift(Subscriber&& s) const {
        return l(forward<Subscriber>(s));
    }
};

template<class Lift>
lifter<Lift> make_lifter(Lift&& l) {
    return {forward<Lift>(l)};
}

namespace detail {

template<class T>
struct lifter_check : public false_type {};

template<class Lift>
struct lifter_check<lifter<Lift>> : public true_type {};

template<class T>
using for_lifter = enable_if_t<lifter_check<decay_t<T>>::value>;

template<class T>
using not_lifter = enable_if_t<!lifter_check<decay_t<T>>::value>;

}


template<class Adapt>
struct adaptor {
    Adapt a;
    /// \brief returns sender
    template<class Sender>
    auto adapt(Sender&& s) const {
        return a(forward<Sender>(s));
    }
};

template<class Adapt>
adaptor<Adapt> make_adaptor(Adapt&& l) {
    return {forward<Adapt>(l)};
}

namespace detail {

template<class T>
struct adaptor_check : public false_type {};

template<class Adapt>
struct adaptor_check<adaptor<Adapt>> : public true_type {};

template<class T>
using for_adaptor = enable_if_t<adaptor_check<decay_t<T>>::value>;

template<class T>
using not_adaptor = enable_if_t<!adaptor_check<decay_t<T>>::value>;

}

template<class Terminate>
struct terminator {
    Terminate t;
    /// \brief returns sender
    template<class Sender>
    auto terminate(Sender&& s) const {
        return t(forward<Sender>(s));
    }
};

template<class Terminate>
terminator<Terminate> make_terminator(Terminate&& t) {
    return {forward<Terminate>(t)};
}

namespace detail {

template<class T>
struct terminator_check : public false_type {};

template<class Adapt>
struct terminator_check<terminator<Adapt>> : public true_type {};

template<class T>
using for_terminator = enable_if_t<terminator_check<decay_t<T>>::value>;

template<class T>
using not_terminator = enable_if_t<!terminator_check<decay_t<T>>::value>;

}



inline state<> start(subscription lifetime = subscription{}) {
    return lifetime.make_state();
}

template<class Payload>
auto start(subscription lifetime = subscription{}) {
    return lifetime.template make_state<Payload>();
}

template<class Payload, class... ArgN>
auto start(subscription lifetime, ArgN&&... an) {
    return lifetime.template make_state<Payload>(forward<ArgN>(an)...);
}


const auto ints = [](auto first, auto last){
    info("new ints");
    return make_sender([=](auto scrb){
        info("ints bound to subscriber");
        return make_starter([=](auto ctx) {
            auto r = scrb.create(ctx);
            info("ints started");
            for(auto i = first;!ctx.lifetime.is_stopped(); ++i){
                r.next(i);
                if (i == last) break;
            }
            r.complete();
            return ctx.lifetime;
        });
    });
};

const auto copy_if = [](auto pred){
    info("new copy_if");
    return make_lifter([=](auto scbr){
        info("copy_if bound to subscriber");
        return make_subscriber([=](auto& ctx){
            info("copy_if bound to context");
            auto r = scbr.create(ctx);
            return make_receiver(r, ctx.lifetime, [=](auto& r, auto v){
                if (pred(v)) r.next(v);
            });
        });
    });
};

const auto transform = [](auto f){
    info("new transform");
    return make_lifter([=](auto scbr){
        info("transform bound to subscriber");
        return make_subscriber([=](auto& ctx){
            info("transform bound to context");
            auto r = scbr.create(ctx);
            return make_receiver(r, ctx.lifetime, [=](auto& r, auto v){
                r.next(f(v));
            });
        });
    });
};

const auto last_or_default = [](auto def){
        info("new last_or_default");
    return make_lifter([=](auto scbr){
        info("last_or_default bound to subscriber");
        return make_subscriber([=](auto& ctx){
            info("last_or_default bound to context");
            auto r = scbr.create(ctx);
            auto last = ctx.lifetime.template make_state<std::decay_t<decltype(def)>>(def);
            return make_receiver(r, ctx.lifetime, 
                [last](auto& r, auto v){
                    last.get() = v;
                },
                detail::ignore{},
                [last](auto& r){
                    r.next(last.get());
                    r.complete();
                });
        });
    });
};

const auto take = [](int n){
    info("new take");
    return make_adaptor([=](auto source){
        info("take bound to source");
        return make_sender([=](auto scrb){
            info("take bound to subscriber");
            return source.bind(
                make_subscriber([=](auto ctx){
                    info("take bound to context");
                    auto r = scrb.create(ctx);
                    auto remaining = ctx.lifetime.template make_state<int>(n);
                    return make_receiver(r, ctx.lifetime, 
                    [remaining](auto& r, auto v){
                        if (remaining.get()-- == 0) {
                            r.complete();
                            return;
                        }
                        r.next(v);
                    });
                }));
        });
    });
};

const auto merge = [](){
    info("new merge");
    return make_adaptor([=](auto source){
        info("merge bound to source");
        return make_sender([=](auto scrb){
            info("merge bound to subscriber");
            return source.bind(
                make_subscriber([=](auto ctx){
                    info("merge bound to context");
                    
                    using payload_t = decay_t<decltype(ctx.get())>;

                    auto pending = ctx.lifetime.template make_state<set<subscription>>();
                    pending.get().insert(ctx.lifetime);

                    subscription destlifetime;
                    destlifetime.insert([pending](){
                        while (!pending.get().empty()) {
                            (*pending.get().begin()).stop();
                        }
                        info("merge-output stopped");
                    });
                    auto destctx = destlifetime.template make_state<payload_t>(ctx.get());
                    auto r = scrb.create(destctx);

                    ctx.lifetime.insert([pending, r, l = ctx.lifetime](){
                        pending.get().erase(l);
                        if (pending.get().empty()){
                            r.complete();
                        }
                        info("merge-input stopped");
                    });

                    return make_receiver(r, destlifetime, 
                        [pending, destctx](auto r, auto v){
                            v.bind(
                                make_subscriber([=](auto ctx){
                                    info("merge-nested bound to context");
                                    pending.get().insert(ctx.lifetime);
                                    ctx.lifetime.insert([pending, r, l = ctx.lifetime](){
                                        pending.get().erase(l);
                                        if (pending.get().empty()){
                                            r.complete();
                                        }
                                        info("merge-nested stopped");
                                    });
                                    return make_receiver(r, ctx.lifetime, 
                                        [pending](auto& r, auto v){
                                            r.next(v);
                                        },
                                        [](auto& r, auto e){
                                            r.error(e);
                                        },
                                        [](auto& r){
                                            // not complete until all pending streams have stopped
                                        });
                                })) | 
                            start<payload_t>(subscription{}, destctx.get());
                        },
                        [](auto& r, auto e){
                            r.error(e);
                        },
                        [](auto& r){
                            // not complete until all pending streams have stopped
                        });
                }));
        });
    });
};

template<class F>
auto transform_merge(F&& f) {
    return transform(forward<F>(f)) | merge();
};

const auto printto = [](auto& output){
    info("new printto");
    return make_subscriber([&](auto& ctx) {
        info("printto bound to context");
        auto values = ctx.lifetime.template make_state<int>(0);
        return make_receiver(
            ctx.lifetime,
            [&, values](auto v) {
                ++values.get();
                output << v << endl;
            },
            [&](exception_ptr ep){
                output << what(ep) << endl;
            },
            [&, values](){
                output << values.get() << " values received - done!" << endl;
            });
    });
};

const auto ignore = [](){
    info("new ignore");
    return make_subscriber([&](auto& ctx) {
        info("ignore bound to context");
        return make_receiver(ctx.lifetime);
    });
};


/// \brief chain operator overload for
/// Subscriber = Lifter | Subscriber
/// \param lifter
/// \param subscriber
/// \returns subscriber
template<class Lifter, class Subscriber,
    class CheckL = detail::for_lifter<Lifter>,
    class CheckScbr = detail::for_subscriber<Subscriber>>
auto operator|(Lifter&& l, Subscriber&& scbr) {
    return l.lift(forward<Subscriber>(scbr));
}

/// \brief chain operator overload for
/// Lifter = Lifter | Lifter
/// \param lifter
/// \param lifter
/// \returns Lifter
template<class LifterL, class LifterR,
    class CheckLl = detail::for_lifter<LifterL>,
    class CheckLr = detail::for_lifter<LifterR>, 
    class _5 = void>
auto operator|(LifterL lhs, LifterR rhs) {
    return make_lifter([lhs = move(lhs), rhs = move(rhs)](auto&& scbr){
        lhs.lift(rhs.lift(forward<decltype(scbr)>(scbr)));
    });
}

/// \brief chain operator overload for
/// Sender = Sender | Lifter
/// \param sender
/// \param lifter
/// \returns sender
template<class Sender, class Lifter,
    class CheckS = detail::for_sender<Sender>,
    class CheckL = detail::for_lifter<Lifter>, 
    class _5 = void, 
    class _6 = void>
auto operator|(Sender&& s, Lifter&& l) {
    return make_sender([=](auto&& scrb){
        return s.bind(l.lift(forward<decltype(scrb)>(scrb)));
    });
}


/// \brief chain operator overload for
/// Starter = Sender | Subscriber
/// \param sender
/// \param subscriber
/// \returns starter
template<class Sender, class Subscriber,
    class CheckS = detail::for_sender<Sender>,
    class CheckScbr = detail::for_subscriber<Subscriber>, 
    class _5 = void, 
    class _6 = void, 
    class _7 = void>
auto operator|(Sender&& s, Subscriber&& scbr) {
    return s.bind(forward<Subscriber>(scbr));
}

/// \brief chain operator overload for
/// subscription = Starter | State
/// \param starter
/// \param state
/// \returns subscription
template<class Starter, class State,
    class CheckS = detail::for_starter<Starter>,
    class CheckSt = detail::for_state<State>, 
    class _5 = void, 
    class _6 = void, 
    class _7 = void, 
    class _8 = void>
subscription operator|(Starter&& s, State&& st) {
    return s.start(forward<State>(st));
}

/// \brief chain operator overload for
/// Adaptor = Adaptor | Adaptor
/// \param adaptor
/// \param adaptor
/// \returns adaptor
template<class AdaptorL, class AdaptorR,
    class CheckAL = detail::for_adaptor<AdaptorL>,
    class CheckAR = detail::for_adaptor<AdaptorR>, 
    class _5 = void, 
    class _6 = void, 
    class _7 = void, 
    class _8 = void, 
    class _9 = void>
auto operator|(AdaptorL&& lhs, AdaptorR&& rhs) {
    return make_adaptor([=](auto source){
        return rhs.adapt(lhs.adapt(source));
    });
}

/// \brief chain operator overload for
/// Adaptor = Adaptor | Lifter
/// \param adaptor
/// \param lifter
/// \returns adaptor
template<class Adapter, class Lifter,
    class CheckA = detail::for_adaptor<Adapter>,
    class CheckL = detail::for_lifter<Lifter>, 
    class _5 = void, 
    class _6 = void, 
    class _7 = void, 
    class _8 = void, 
    class _9 = void, 
    class _10 = void>
auto operator|(Adapter&& a, Lifter&& l) {
    return make_adaptor([=](auto source){
        auto s = a.adapt(source);
        return make_sender([=](auto&& scrb){
            return s.bind(l.lift(forward<decltype(scrb)>(scrb)));
        });
    });
}

/// \brief chain operator overload for
/// Adaptor = Lifter | Adaptor
/// \param lifter
/// \param adaptor
/// \returns adaptor
template<class Lifter, class Adapter,
    class CheckL = detail::for_lifter<Lifter>, 
    class CheckA = detail::for_adaptor<Adapter>,
    class _5 = void, 
    class _6 = void, 
    class _7 = void, 
    class _8 = void, 
    class _9 = void, 
    class _10 = void, 
    class _11 = void>
auto operator|(Lifter&& l, Adapter&& a) {
    return make_adaptor([=](auto source){
        return a.adapt(make_sender([=](auto&& scrb){
            return source.bind(l.lift(forward<decltype(scrb)>(scrb)));
        }));
    });
}

/// \brief chain operator overload for
/// Sender = Sender | Adaptor
/// \param sender
/// \param adaptor
/// \returns sender
template<class Sender, class Adaptor,
    class CheckS = detail::for_sender<Sender>,
    class CheckA = detail::for_adaptor<Adaptor>, 
    class _5 = void, 
    class _6 = void, 
    class _7 = void, 
    class _8 = void, 
    class _9 = void, 
    class _10 = void, 
    class _11 = void, 
    class _12 = void>
auto operator|(Sender&& s, Adaptor&& a) {
    return a.adapt(forward<Sender>(s));
}

/// \brief chain operator overload for
/// Terminator = Adaptor | Subscriber
/// \param adaptor
/// \param subscriber
/// \returns terminator
template<class Adapter, class Subscriber,
    class CheckA = detail::for_adaptor<Adapter>,
    class CheckS = detail::for_subscriber<Subscriber>, 
    class _5 = void, 
    class _6 = void, 
    class _7 = void, 
    class _8 = void, 
    class _9 = void, 
    class _10 = void, 
    class _11 = void, 
    class _12 = void, 
    class _13 = void>
auto operator|(Adapter&& a, Subscriber&& scrb) {
    return make_terminator([=](auto source){
        return a.adapt(source).bind(scrb);
    });
}

/// \brief chain operator overload for
/// starter = Sender | Terminator
/// \param sender
/// \param terminator
/// \returns starter
template<class Sender, class Terminator,
    class CheckS = detail::for_sender<Sender>,
    class CheckA = detail::for_terminator<Terminator>, 
    class _5 = void, 
    class _6 = void, 
    class _7 = void, 
    class _8 = void, 
    class _9 = void, 
    class _10 = void, 
    class _11 = void, 
    class _12 = void, 
    class _13 = void, 
    class _14 = void>
auto operator|(Sender&& s, Terminator&& t) {
    return t.terminate(forward<Sender>(s));
}

}

extern"C" {
    void designcontext(int, int);
}

void designcontext(int first, int last){
    using namespace std::chrono;

    using namespace designcontextdef;
    using designcontextdef::copy_if;
    using designcontextdef::transform;
    using designcontextdef::merge;

    auto lastof3even = copy_if(even) | 
        take(50000000) |
        last_or_default(42);
 
{
    auto lifetime = ints(0, 2) | 
        transform_merge([=](int){
            return ints(first, last * 100) |
                lastof3even;
        }) |
        printto(cout) |
        start<destruction>();

    lifetime.insert([](){info("caller stopped");});
}

{
 cout << "for" << endl;
 auto t0 = high_resolution_clock::now();
    for(auto i = first; i < last; ++i) {
        auto lifetime = ints(0, 0) |
            transform([](int i) {
                return to_string(i);
            }) |
            transform([](const string& s) {
                int i = '0' - s[0];
                return i;
            }) |
        make_subscriber() |
        start<destruction>();

        lifetime.insert([](){info("caller stopped");});
    }
 auto t1 = high_resolution_clock::now();
 auto d = duration_cast<milliseconds>(t1-t0).count() * 1.0;
 auto sc = last - first;
 cout << d / sc << " ms per subscription\n"; 
 auto s = d / 1000.0;
 cout << sc / s << " subscriptions per second\n"; 
}


{
 cout << "transform | merge" << endl;
 auto t0 = high_resolution_clock::now();

    auto lifetime = ints(first, last) | 
        transform([=](int){
            return ints(0, 0) |
                transform([](int i) {
                    return to_string(i);
                }) |
                transform([](const string& s) {
                    int i = '0' - s[0];
                    return i;
                });
//                lastof3even;
        }) |
        merge() |
//        printto(cout) |
        make_subscriber() |
        start<destruction>();

    lifetime.insert([](){info("caller stopped");});

 auto t1 = high_resolution_clock::now();
 auto d = duration_cast<milliseconds>(t1-t0).count() * 1.0;
 auto sc = last - first;
 cout << d / sc << " ms per subscription\n"; 
 auto s = d / 1000.0;
 cout << sc / s << " subscriptions per second\n"; 
}

{
 cout << "transform_merge" << endl;
 auto t0 = high_resolution_clock::now();

    auto lifetime = ints(first, last) | 
        transform_merge([=](int){
            return ints(0, 0) |
                transform([](int i) {
                    return to_string(i);
                }) |
                transform([](const string& s) {
                    int i = '0' - s[0];
                    return i;
                });
//                lastof3even;
        }) |
//        printto(cout) |
        make_subscriber() |
        start<destruction>();

    lifetime.insert([](){info("caller stopped");});

 auto t1 = high_resolution_clock::now();
 auto d = duration_cast<milliseconds>(t1-t0).count() * 1.0;
 auto sc = last - first;
 cout << d / sc << " ms per subscription\n"; 
 auto s = d / 1000.0;
 cout << sc / s << " subscriptions per second\n"; 
}

}
