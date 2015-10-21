#pragma once

using Ticks = rx::run_loop<std::chrono::steady_clock, std::exception_ptr>;
auto loop = Ticks(rx::subscription{});
auto makeStrand = loop.make();

void tick(){
    Ticks::guard_type guard(loop.loop.get().lock);
    loop.step(guard, 10ms);
}

rx::subscription lifetime{};

#if EMSCRIPTEN

extern"C" void EMSCRIPTEN_KEEPALIVE reset() {
    lifetime.stop();
    lifetime = rx::subscription{};
    //loop = Ticks(rx::subscription{});
    //makeStrand = loop.make();
    detail::start = steady_clock::now();
}

#endif
