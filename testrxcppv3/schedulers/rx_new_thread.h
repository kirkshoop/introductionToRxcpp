#pragma once

namespace rx {

struct threadjoin
{
    thread worker;
    function<void()> notify;
    template<class W, class N>
    threadjoin(W&& w, N&& n) 
        : worker(forward<W>(w))
        , notify(forward<N>(n)) {
        worker.detach();
    }
    ~threadjoin(){
        info("threadjoin: destroy notify");
        notify();
    }
    
};

template<class Strand>
struct new_thread {
    using strand_type = decay_t<Strand>;

    subscription lifetime;
    strand_type strand;
    state<threadjoin> worker;
    
    new_thread(strand_type&& s, state<threadjoin>&& t) 
        : lifetime(s.lifetime)
        , strand(move(s))
        , worker(move(t)) {
    }
    
    template<class At, class... ON>
    void operator()(At at, observer<ON...> out) const {
        strand.defer_at(at, out);
    }
};

template<class Clock = steady_clock, class Error = exception_ptr>
struct make_new_thread {
    auto operator()(subscription lifetime) const {
        info("new_thread: create");
        run_loop<Clock, Error> loop(subscription{});
        auto strand = loop.make()(lifetime);
        auto t = make_state<threadjoin>(lifetime, [=](){
                info("new_thread: loop run enter");
                loop.run();
                info("new_thread: loop run exit");
            }, [l = loop.lifetime](){
                info("new_thread: loop stop enter");
                l.stop();
                info("new_thread: loop stop exit");
            });
        return make_strand<Clock>(lifetime, new_thread<decltype(strand)>(move(strand), move(t)), detail::now<Clock>{});
    }
};

}