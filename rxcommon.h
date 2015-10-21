#pragma once


//
// setup up a run_loop scheduler and register a tick 
// function to dispatch from requestAnimationFrame
//

run_loop rl;

auto jsthread = observe_on_run_loop(rl);

void tick(){
    if (!rl.empty() && rl.peek().when < rl.now()) {
        rl.dispatch();
    }
}

composite_subscription lifetime;

extern"C" void EMSCRIPTEN_KEEPALIVE reset() {
    lifetime.unsubscribe();
    lifetime = composite_subscription();
}

#ifdef EMSCRIPTEN_RESULT
struct MouseEvent 
{
    long x;
    long y;
};

const auto mousedown$ = [](const char* targetId){
    string t = targetId;
    return observable<>::create<MouseEvent>([=](subscriber<MouseEvent> s){
        auto dest = make_unique<subscriber<MouseEvent>>(s);
        auto result = dest->get_subscription();
        result.add([t, expired = dest.get()](){
            emscripten_set_mousedown_callback(t.c_str(), nullptr, 1, nullptr);
            delete expired;
        });
        auto mouse_callback = [](int eventType, const EmscriptenMouseEvent *e, void *userData) -> EM_BOOL {
            subscriber<MouseEvent>* dest = reinterpret_cast<subscriber<MouseEvent>*>(userData);
            dest->on_next(MouseEvent{e->targetX, e->targetY});
            return false;
        };
        EMSCRIPTEN_RESULT ret = emscripten_set_mousedown_callback(t.c_str(), dest.get(), 1, mouse_callback);
        if (ret < 0) {
            throw runtime_error("emscripten_set_mousedown_callback failed!");
        }
        dest.release();
        return result;
    });
};

const auto mouseup$ = [](const char* targetId){
    string t = targetId;
    return observable<>::create<MouseEvent>([=](subscriber<MouseEvent> s){
        auto dest = make_unique<subscriber<MouseEvent>>(s);
        auto result = dest->get_subscription();
        result.add([t, expired = dest.get()](){
            emscripten_set_mousedown_callback(t.c_str(), nullptr, 1, nullptr);
            delete expired;
        });
        auto mouse_callback = [](int eventType, const EmscriptenMouseEvent *e, void *userData) -> EM_BOOL {
            subscriber<MouseEvent>* dest = reinterpret_cast<subscriber<MouseEvent>*>(userData);
            dest->on_next(MouseEvent{e->targetX, e->targetY});
            return false;
        };
        EMSCRIPTEN_RESULT ret = emscripten_set_mouseup_callback(t.c_str(), dest.get(), 1, mouse_callback);
        if (ret < 0) {
            throw runtime_error("emscripten_set_mouseup_callback failed!");
        }
        dest.release();
        return result;
    });
};

const auto mousemove$ = [](const char* targetId){
    string t = targetId;
    return observable<>::create<MouseEvent>([=](subscriber<MouseEvent> s){
        auto dest = make_unique<subscriber<MouseEvent>>(s);
        auto result = dest->get_subscription();
        result.add([t, expired = dest.get()](){
            emscripten_set_mousedown_callback(t.c_str(), nullptr, 1, nullptr);
            delete expired;
        });
        auto mouse_callback = [](int eventType, const EmscriptenMouseEvent *e, void *userData) -> EM_BOOL {
            subscriber<MouseEvent>* dest = reinterpret_cast<subscriber<MouseEvent>*>(userData);
            dest->on_next(MouseEvent{e->targetX, e->targetY});
            return false;
        };
        EMSCRIPTEN_RESULT ret = emscripten_set_mousemove_callback(t.c_str(), dest.get(), 1, mouse_callback);
        if (ret < 0) {
            throw runtime_error("emscripten_set_mousemove_callback failed!");
        }
        dest.release();
        return result;
    });
};
#endif