#pragma once

namespace rx {

const auto last_or_default = [](auto def){
        info("new last_or_default");
    return make_lifter([=](auto scbr){
        info("last_or_default bound to subscriber");
        return make_subscriber([=](auto ctx){
            info("last_or_default bound to context lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(ctx.lifetime.store.get())));
            auto r = scbr.create(ctx);
            auto last = make_state<std::decay_t<decltype(def)>>(ctx.lifetime, def);
            info("last_or_default observer lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(r.lifetime.store.get())));
            return make_observer(r, r.lifetime,
                [last](auto& , auto v){
                    last.get() = v;
                },
                detail::skip{},
                [last](auto& r){
                    r.next(last.get());
                    r.complete();
                });
        });
    });
};

}