#pragma once

namespace rx {

const auto intervals = [](auto makeStrand, auto initial, auto period){
    info("new intervals");
    return make_observable([=](auto scrb){
        info("intervals bound to subscriber");
        return make_starter([=](auto ctx) {
            info("intervals bound to context");
            subscription lifetime;
            ctx.lifetime.insert(lifetime);
            auto intervalcontext = copy_context(lifetime, makeStrand, ctx);
            auto r = scrb.create(ctx);
            info("intervals started");
            defer_periodic(intervalcontext, initial, period, r);
            return ctx.lifetime;
        });
    });
};

}