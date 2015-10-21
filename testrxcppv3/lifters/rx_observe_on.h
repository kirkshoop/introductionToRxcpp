#pragma once

namespace rx {

template<class MakeStrand>
auto observe_on(MakeStrand makeStrand){
    info("new observe_on");
    return make_lifter([=](auto scbr){
        info("observe_on bound to subscriber");
        return make_subscriber([=](auto ctx){
            info("observe_on bound to context");
            subscription lifetime;
            ctx.lifetime.insert(lifetime);
            auto outcontext = copy_context(ctx.lifetime, makeStrand, ctx);
            auto r = scbr.create(outcontext);
            return make_observer(r, lifetime, 
                [=](auto& r, auto v){
                    auto next = make_observer(r, subscription{}, [=](auto& r, auto& ){
                        r.next(v);
                    }, detail::pass{}, detail::skip{});
                    defer(outcontext, next);
                },
                [=](auto& r, auto e){
                    auto error = make_observer(r, subscription{}, [=](auto& r, auto& ){
                        r.error(e);
                    }, detail::pass{}, detail::skip{});
                    defer(outcontext, error);
                },
                [=](auto& r){
                    auto complete = make_observer(r, subscription{}, [=](auto& r, auto& ){
                        r.complete();
                    }, detail::pass{}, detail::skip{});
                    defer(outcontext, complete);
                });
        });
    });
}

#if !RX_SLOW
template<class Clock>
auto observe_on(const detail::make_immediate<Clock>&){
    info("new observe_on");
    return make_lifter([=](auto scbr){
        info("observe_on bound to subscriber");
        return scbr;
    });
}
#endif

}