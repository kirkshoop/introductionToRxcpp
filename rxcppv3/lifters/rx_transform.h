#pragma once

namespace rx {

const auto transform = [](auto f){
    info("new transform");
    return make_lifter([=](auto scbr){
        info("transform bound to subscriber");
        return make_subscriber([=](auto ctx){
            info("transform bound to context lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(ctx.lifetime.store.get())));
            auto r = scbr.create(ctx);
            info("transform observer lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(r.lifetime.store.get())));
            return make_observer(r, r.lifetime, [=](auto& r, auto& v){
                r.next(f(v));
            });
        });
    });
};

}