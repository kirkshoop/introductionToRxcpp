#pragma once

namespace rx {

template<class T>
using payload_t = typename decay_t<T>::payload_type;

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
    using lock_type = mutex;
    using guard_type = unique_lock<lock_type>;
    struct finish
    {
        finish() 
            : stopped(false)
            , joined(false) {
        }
        lock_type lock;
        set<subscription> others;
        atomic<bool> stopped;
        mutex joinlock;
        atomic<bool> joined;
        condition_variable joinwake;
    };
    struct shared
    {
        ~shared(){
            info(to_string(reinterpret_cast<ptrdiff_t>(this)) + " - subscription: destroy - " + to_string(destructors.size()));
            {
                auto expired = move(destructors);
                for (auto& d : expired) {
                    info(to_string(reinterpret_cast<ptrdiff_t>(this)) + " - subscription: destroy destructor");
                    d();
                    info(to_string(reinterpret_cast<ptrdiff_t>(this)) + " - subscription: destroy destructor exit");
                }
            }
            info(to_string(reinterpret_cast<ptrdiff_t>(this)) + " - end lifetime");
        }
        explicit shared(const shared_ptr<finish>& ) 
            : defer([](function<void()> target){target();}) {
            info(to_string(reinterpret_cast<ptrdiff_t>(this)) + " - new lifetime");
        }
        bool search_scopes(shared_ptr<shared> other) {
            for(auto& check : scopes) {
                auto scope = check.lock();
                if (scope == other) {
                    return true;
                }
                if (scope && scope->search_scopes(other)) {
                    return true;
                }
            }
            return false;
        }
        function<void(function<void()>)> defer;
        list<function<void()>> stoppers;
        list<function<void()>> destructors;
        list<weak_ptr<shared>> scopes;
    };
public:
    subscription() : signal(make_shared<finish>()), store(make_shared<shared>(signal)) {}
    subscription(shared_ptr<shared> st, shared_ptr<finish> s) : signal(s), store(st) {}
    /// \brief used to exit loops or otherwise stop work scoped to this subscription.
    /// \returns bool - if true do not access any state objects.
    bool is_stopped() const {
        return !store || signal->stopped;
    }
    /// \brief 
    void insert(const subscription& s) const {
        guard_type guard(signal->lock);
        if (is_stopped()) {
            s.stop();
            return;
        }
        if (s == *this) {
            info(to_string(reinterpret_cast<ptrdiff_t>(store.get())) + " - subscription: inserting self!");
            std::abort();
        }

        if (store->search_scopes(s.store)) {
            info(to_string(reinterpret_cast<ptrdiff_t>(store.get())) + " - subscription: inserting loop in lifetime!");
            std::abort();
        }

        // nest
        signal->others.insert(s);

        weak_ptr<shared> p = store;
        s.store->scopes.push_front(p);

        // unnest when child is stopped
        weak_ptr<shared> c = s.store;
        s.insert([p, c, ps = signal, cs = s.signal](){
            auto storep = p.lock();
            auto storec = c.lock();
            if (storep && storec) {
                //info(to_string(reinterpret_cast<ptrdiff_t>(storep.get())) + " - subscription: erase nested - " + to_string(reinterpret_cast<ptrdiff_t>(storec.get())));
                auto that = subscription(storep, ps);
                auto s = subscription(storec, cs);
                that.erase(s);
            } else {
                info("subscription: erase nested (store missing!)");
            }
        });
    }
    /// \brief 
    void erase(const subscription& s) const {
        guard_type guard(signal->lock);
        if (is_stopped()) {
            return;
        }
        if (s == *this) {
            info("subscription: erasing self!");
            std::abort();
        }
        signal->others.erase(s);
    }
    /// \brief 
    void insert(function<void()> stopper) const {
        guard_type guard(signal->lock);

        if (is_stopped()) {
            guard.unlock();
            stopper();
            return;
        }

        store->stoppers.emplace_front(stopper);
    }
    /// \brief 
    template<class Payload, class... ArgN>
    state<Payload> make_state(ArgN&&... argn) const;
    /// \brief 
    state<> make_state() const;
    /// \brief 
    state<> copy_state(const state<>&) const;
    /// \brief 
    template<class Payload>
    state<Payload> copy_state(const state<Payload>&) const;
    /// \brief 
    void bind_defer(function<void(function<void()>)> d) {
        guard_type guard(signal->lock);
        if (is_stopped()) {
            return;
        }
        store->defer = d;
    }
    /// \brief 
    void stop() const {
        guard_type guard(signal->lock);
        if (is_stopped()) {
            return;
        }

        auto st = move(store);
        store = nullptr;

        signal->stopped = true;
        info(to_string(reinterpret_cast<ptrdiff_t>(st.get())) + " - subscription: stopped set to true");

        auto si = signal;

        guard.unlock();

        st->defer([=](){
            info(to_string(reinterpret_cast<ptrdiff_t>(st.get())) + " - subscription: stop");

            {
                guard_type guard(si->lock);
                auto expired = st->stoppers;
                guard.unlock();
                for (auto s : expired) {
                    info(to_string(reinterpret_cast<ptrdiff_t>(st.get())) + " - subscription: stop stopper");
                    s();
                    info(to_string(reinterpret_cast<ptrdiff_t>(st.get())) + " - subscription: stop stopper exit");
                }
            }
            {
                guard_type guard(si->lock);
                auto expired = si->others;
                guard.unlock();
                for (auto o : expired) {
                    info(to_string(reinterpret_cast<ptrdiff_t>(st.get())) + " - subscription: stop other");
                    o.stop();
                    info(to_string(reinterpret_cast<ptrdiff_t>(st.get())) + " - subscription: stop other exit");
                }
            }
            {
                guard_type guard(si->lock);
                st->defer = [](function<void()> target){target();};
                st->stoppers.clear();
            }

            info(to_string(reinterpret_cast<ptrdiff_t>(st.get())) + " - subscription: notify_all");
            {
                unique_lock<mutex> guard(si->joinlock);
                si->joined = true;
            }
            si->joinwake.notify_all();
            info(to_string(reinterpret_cast<ptrdiff_t>(st.get())) + " - subscription: stopped");
        });
    }
    /// \brief
    void join() const {
        info(to_string(reinterpret_cast<ptrdiff_t>(store.get())) + " - subscription: join");
        {
            unique_lock<mutex> guard(signal->lock);
            auto expired = signal->others;
            guard.unlock();
            for (auto o : expired) {
                info(to_string(reinterpret_cast<ptrdiff_t>(store.get())) + " - subscription: join other");
                o.join();
                info(to_string(reinterpret_cast<ptrdiff_t>(store.get())) + " - subscription: join other exit");
            }
        }
        {
            unique_lock<mutex> guard(signal->joinlock);
            signal->joinwake.wait(guard, [s = this->signal](){return !!s->joined;});
        }
        info(to_string(reinterpret_cast<ptrdiff_t>(store.get())) + " - subscription: joined " + (signal->joined ? "true" : "false"));
    }
    shared_ptr<finish> signal;
    mutable shared_ptr<shared> store;
private:
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

namespace detail {

template<class T>
using for_subscription = for_same_t<T, subscription>;

template<class T>
using not_subscription = not_same_t<T, subscription>;

}

template<>
struct state<void>
{
    using payload_type = void;
    subscription lifetime;
    explicit state(subscription lifetime) 
        : lifetime(lifetime) {
    }
    state(const state&) = default;
    template<class Payload>
    state(const state<Payload>& o)
        : lifetime(o.lifetime) {
    }
};

template<class Payload>
struct state
{
    using payload_type = decay_t<Payload>;
    subscription lifetime;
    state(subscription l, Payload* p) : lifetime(l), p(p) {}
    Payload& get() {
        return *p;
    }
    Payload& get() const {
        return *p;
    }
    explicit operator state<>(){
        return {lifetime};
    }
private:
    mutable Payload* p;
};

class lifetime_error : public logic_error {
public:
  explicit lifetime_error (const string& what_arg) : logic_error(what_arg) {}
  explicit lifetime_error (const char* what_arg) : logic_error(what_arg) {}
};

template<class Payload, class... ArgN>
state<Payload> subscription::make_state(ArgN&&... argn) const {
    guard_type guard(signal->lock);
    auto size = store->destructors.size();
    info(to_string(reinterpret_cast<ptrdiff_t>(store.get())) + " - subscription: make_state - " + to_string(size) + " " + typeid(Payload).name());
    if (is_stopped()) {
        throw lifetime_error("subscription is stopped!");
    }
    auto p = make_unique<Payload>(forward<ArgN>(argn)...);
    auto result = state<Payload>{*this, p.get()};
    store->destructors.emplace_front(
        [d=p.release(), s=store.get(), size]() mutable {
            info(to_string(reinterpret_cast<ptrdiff_t>(s)) + " - subscription: destroy make_state - " + to_string(size) + " " + typeid(Payload).name());
            auto p = d; 
            d = nullptr; 
            delete p;
            info(to_string(reinterpret_cast<ptrdiff_t>(s)) + " - subscription: destroy make_state exit - " + to_string(size) + " " + typeid(Payload).name());
        });
    return result;
}
state<> subscription::make_state() const {
    info(to_string(reinterpret_cast<ptrdiff_t>(store.get())) + " - subscription: make_state");
    if (is_stopped()) {
        throw lifetime_error("subscription is stopped!");
    }
    auto result = state<>{*this};
    return result;
}

state<> subscription::copy_state(const state<>&) const{
    if (is_stopped()) {
        throw lifetime_error("subscription is stopped!");
    }
    return make_state();
}

template<class Payload>
state<Payload> subscription::copy_state(const state<Payload>& o) const{
    if (is_stopped()) {
        throw lifetime_error("subscription is stopped!");
    }
    return make_state<Payload>(o.get());
}

template<class Payload, class... ArgN>
state<Payload> make_state(subscription lifetime, ArgN... argn) {
    return lifetime.template make_state<Payload>(forward<ArgN>(argn)...);
}
inline state<> make_state(subscription lifetime) {
    return lifetime.make_state();
}

state<> copy_state(subscription lifetime, const state<>&) {
    return lifetime.make_state();
}

template<class Payload>
state<Payload> copy_state(subscription lifetime, const state<Payload>& o) {
    return lifetime.template make_state<Payload>(o.get());
}

namespace detail {

template<class T>
using for_state = for_specialization_of_t<T, state>;

template<class T>
using not_state = not_specialization_of_t<T, state>;

}

}