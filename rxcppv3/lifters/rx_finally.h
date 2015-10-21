#pragma once

namespace rx {

const auto finally = [](auto f){
    info("new finally");
    return make_lifter([=](auto scbr){
        info("finally bound to subscriber");
        return make_subscriber([=](auto ctx){
            info("finally bound to context lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(ctx.lifetime.store.get())));
            auto r = scbr.create(ctx);
            r.lifetime.insert(f);
            info("finally observer lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(r.lifetime.store.get())));
            return make_observer(r, r.lifetime);
        });
    });
};

}