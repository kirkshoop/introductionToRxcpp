#pragma once

namespace rx {

template<class Clock = steady_clock, class Error = exception_ptr>
struct run_loop {
    using clock_type = decay_t<Clock>;
    using error_type = decay_t<Error>;
    using lock_type = mutex;
    using guard_type = unique_lock<lock_type>;
    using observer_type = observer_interface<detail::re_defer_at_t<clock_type>, error_type>;
    using item_type = observe_at<clock_type, observer_type>;
    using queue_type = observe_at_queue<clock_type, observer_type>;

    struct guarded_loop {
        ~guarded_loop() {
            info(to_string(reinterpret_cast<ptrdiff_t>(this)) + " - run_loop: guarded_loop destroy");
        }
        lock_type lock;
        condition_variable wake;
        queue_type deferred;
    };

    subscription lifetime;
    state<guarded_loop> loop;
    
    explicit run_loop(subscription l) 
        : lifetime(l)
        , loop(make_state<guarded_loop>(lifetime)) {
        auto& guarded = this->loop.get();
        lifetime.insert([&guarded](){
            info(to_string(reinterpret_cast<ptrdiff_t>(addressof(guarded))) + " - run_loop: stop notify_all");
            //guard_type guard(guarded.lock);
            guarded.wake.notify_all();
        });
    }
    ~run_loop(){
        info(to_string(reinterpret_cast<ptrdiff_t>(addressof(loop.get()))) + " - run_loop: destroy");
    }
    
    bool is_ready(guard_type& guard) const {
        if (!guard.owns_lock()) { 
            info(to_string(reinterpret_cast<ptrdiff_t>(addressof(loop.get()))) + " - run_loop: is_ready caller must own lock!");
            abort(); 
        }
        auto& deferred = loop.get().deferred;
        return !deferred.empty() && deferred.top().when <= clock_type::now();
    }

    bool wait(guard_type& guard) const {
        if (!guard.owns_lock()) { 
            info(to_string(reinterpret_cast<ptrdiff_t>(addressof(loop.get()))) + " - run_loop: wait caller must own lock!");
            abort(); 
        }
        auto& deferred = loop.get().deferred;
        info(to_string(reinterpret_cast<ptrdiff_t>(addressof(loop.get()))) + " - run_loop: wait");
        if (!loop.lifetime.is_stopped()) {
            if (!deferred.empty()) {
                info(to_string(reinterpret_cast<ptrdiff_t>(addressof(loop.get()))) + " - run_loop: wait_until top when");
                loop.get().wake.wait_until(guard, deferred.top().when, [&](){
                    bool r = is_ready(guard) || loop.lifetime.is_stopped();
                    info(to_string(reinterpret_cast<ptrdiff_t>(addressof(loop.get()))) + " - run_loop: wait wakeup is_ready - " + to_string(r));
                    return r;
                });
            } else {
                info(to_string(reinterpret_cast<ptrdiff_t>(addressof(loop.get()))) + " - run_loop: wait for notify");
                loop.get().wake.wait(guard, [&](){
                    bool r = !deferred.empty() || loop.lifetime.is_stopped();
                    info(to_string(reinterpret_cast<ptrdiff_t>(addressof(loop.get()))) + " - run_loop: wait wakeup is_ready - " + to_string(r));
                    return r;
                });
            }
        }
        info(to_string(reinterpret_cast<ptrdiff_t>(addressof(loop.get()))) + " - run_loop: wake");
        return !loop.lifetime.is_stopped();
    }

    void call(guard_type& guard, item_type& next) const {
        if (guard.owns_lock()) { 
            info(to_string(reinterpret_cast<ptrdiff_t>(addressof(loop.get()))) + " - run_loop: call caller must not own lock!");
            abort(); 
        }
        info("run_loop: call");
        auto& deferred = loop.get().deferred;
        bool complete = true;
        next.what.next([&](time_point<clock_type> at){
            unique_lock<guard_type> nestedguard(guard);
            info(to_string(reinterpret_cast<ptrdiff_t>(addressof(loop.get()))) + " - run_loop: call self");
            if (lifetime.is_stopped() || next.what.lifetime.is_stopped()) return;
            info(to_string(reinterpret_cast<ptrdiff_t>(addressof(loop.get()))) + " - run_loop: call push self");
            next.when = at;
            deferred.push(next);
            complete = false;
        });
        if (complete) {
            info(to_string(reinterpret_cast<ptrdiff_t>(addressof(loop.get()))) + " - run_loop: call complete");
            next.what.complete();
        }
    }

    void step(guard_type& guard, typename clock_type::duration d) const {
        if (!guard.owns_lock()) { 
            info(to_string(reinterpret_cast<ptrdiff_t>(addressof(loop.get()))) + " - run_loop: step caller must own lock!");
            abort(); 
        }
        auto& deferred = loop.get().deferred;
        auto stop = clock_type::now() + d;
        while (!loop.lifetime.is_stopped() && is_ready(guard) && clock_type::now() < stop) {
            info(to_string(reinterpret_cast<ptrdiff_t>(addressof(loop.get()))) + " - run_loop: step");

            auto next = move(deferred.top());
            deferred.pop();
            
            guard.unlock();
            call(guard, next);
            guard.lock();
        }
    }
    
    void run() const {
        guard_type guard(loop.get().lock);
        info(to_string(reinterpret_cast<ptrdiff_t>(addressof(loop.get()))) + " - run_loop: run");
        while (wait(guard)) {
            step(guard, 3600s);
        }
        info(to_string(reinterpret_cast<ptrdiff_t>(addressof(loop.get()))) + " - run_loop: exit");
    }

    struct strand {
        subscription lifetime;
        state<guarded_loop> loop;
        
        template<class... OON>
        void operator()(time_point<clock_type> at, observer<OON...> out) const {
            guard_type guard(loop.get().lock);
            lifetime.insert(out.lifetime);
            loop.get().deferred.push(item_type{at, out});
            info(to_string(reinterpret_cast<ptrdiff_t>(addressof(loop.get()))) + " - run_loop: defer_at notify_all");
            loop.get().wake.notify_all();
        }
    };

    auto make() const {
        return [loop = this->loop](subscription lifetime) {
            loop.lifetime.insert(lifetime);
            return make_strand<clock_type>(lifetime, strand{lifetime, loop}, detail::now<clock_type>{});
        };
    }
};

}