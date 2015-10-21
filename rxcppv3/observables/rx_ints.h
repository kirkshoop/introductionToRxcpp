#pragma once

namespace rx {

const auto ints = [](auto first, auto last){
    info("new ints");
    return make_observable([=](auto scrb){
        info("ints bound to subscriber");
        return make_starter([=](auto ctx) {
            info("ints bound to context");
            auto r = scrb.create(ctx);
            info("ints started");
            for(auto i = first;!r.lifetime.is_stopped(); ++i){
                r.next(i);
                if (i == last) break;
            }
            r.complete();
            return ctx.lifetime;
        });
    });
};

const auto async_ints = [](auto makeStrand, auto first, auto last){
    info("new async_ints");
    return make_observable([=](auto scrb){
        info("async_ints bound to subscriber");
        return make_starter([=](auto ctx) {
            info("async_ints bound to context");
            subscription lifetime;
            ctx.lifetime.insert(lifetime);
            auto outcontext = copy_context(ctx.lifetime, makeStrand, ctx);
            auto r = scrb.create(outcontext);
            auto state = make_state<decltype(first)>(ctx.lifetime, first);
            auto lifted = make_observer(
                r,
                lifetime, 
                [=](const auto& r, auto& self) mutable {
                    auto& s = state.get();
                    r.next(s);
                    if (++s == last) {
                        r.complete();
                    }
                    self(outcontext.now());
                }, detail::pass{}, detail::skip{});
            info("async_ints started");
            defer(outcontext, lifted);
            return ctx.lifetime;
        });
    });
};

}