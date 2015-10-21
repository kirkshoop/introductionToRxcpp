#pragma once

namespace rx {

inline context<> start(subscription lifetime = subscription{}) {
    info("start default lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(lifetime.store.get())));
    return make_context(lifetime);
}

template<class Payload, class... AN>
auto start(AN&&... an) {
    subscription lifetime;
    info("start payload lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(lifetime.store.get())));
    return make_context<Payload>(lifetime, forward<AN>(an)...);
}

template<class Payload, class... ArgN>
auto start(subscription lifetime, ArgN&&... an) {
    info("start lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(lifetime.store.get())) + " & payload");
    return make_context<Payload>(lifetime, forward<ArgN>(an)...);
}

template<class Payload, class Clock, class... AN>
auto start(AN&&... an) {
    subscription lifetime;
    info("start clock & payload lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(lifetime.store.get())));
    return make_context<Payload, Clock>(subscription{}, forward<AN>(an)...);
}

template<class Payload, class Clock, class... AN>
auto start(subscription lifetime, AN&&... an) {
    info("start lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(lifetime.store.get())) + " & clock & payload");
    return make_context<Payload, Clock>(lifetime, forward<AN>(an)...);
}

template<class Payload, class MakeStrand, class Clock>
auto start(const context<Payload, MakeStrand, Clock>& o) {
    info("start copy lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(o.lifetime.store.get())));
    return o;
}

template<class... CN>
auto start(subscription lifetime, const context<CN...>& o) {
    info("start copy with new lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(lifetime.store.get())) + " old lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(lifetime.store.get())));
    return copy_context(lifetime, o);
}

}