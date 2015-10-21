#pragma once

namespace rx {

const auto delay = [](auto makeStrand, auto delay){
    info("new delay");
    return make_lifter([=](auto scbr){
        info("delay bound to subscriber");
        return make_subscriber([=](auto ctx){
            info("delay bound to context");
            subscription lifetime;
            ctx.lifetime.insert(lifetime);
            auto outcontext = copy_context(ctx.lifetime, makeStrand, ctx);
            auto r = scbr.create(outcontext);
            return make_observer(r, lifetime, 
                [=](auto& r, auto v){
                    auto next = make_observer(r, subscription{}, [=](auto& r, auto& ){
                        r.next(v);
                    }, detail::pass{}, detail::skip{});
                    defer_after(outcontext, delay, next);
                },
                [=](auto& r, auto e){
                    auto error = make_observer(r, subscription{}, [=](auto& r, auto& ){
                        r.error(e);
                    }, detail::pass{}, detail::skip{});
                    defer_after(outcontext, delay, error);
                },
                [=](auto& r){
                    auto complete = make_observer(r, subscription{}, [=](auto& r, auto& ){
                        r.complete();
                    }, detail::pass{}, detail::skip{});
                    defer_after(outcontext, delay, complete);
                });
        });
    });
};

}