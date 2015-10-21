#pragma once

namespace rx {

const auto copy_if = [](auto pred){
    info("new copy_if");
    return make_lifter([=](auto scbr){
        info("copy_if bound to subscriber");
        return make_subscriber([=](auto ctx){
            info("copy_if bound to context lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(ctx.lifetime.store.get())));
            auto r = scbr.create(ctx);
            info("copy_if observer lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(r.lifetime.store.get())));
            return make_observer(r, r.lifetime, [=](auto& r, auto v){
                if (pred(v)) r.next(v);
            });
        });
    });
};

}