
// source ~/source/emsdk_portable/emsdk_env.sh

// em++ -std=c++14 --memory-init-file 0 -s ASSERTIONS=2 -s DEMANGLE_SUPPORT=1 -s DISABLE_EXCEPTION_CATCHING=0 -s EXPORTED_FUNCTIONS="['_main', '_reset', '_designcontext']" -O2 context.cpp -o context.js

#if EMSCRIPTEN
#include "emscripten.h"
#include <emscripten/html5.h>
#endif

#include "rxcpp/rx.hpp"
using namespace rxcpp;
using namespace rxcpp::schedulers;
using namespace rxcpp::subjects;
using namespace rxcpp::sources;
using namespace rxcpp::util;

#include "rxcpp/rx-test.hpp"
using namespace rxcpp::test;
using namespace rxcpp::notifications;

#include <regex>
#include <random>
#include <chrono>
using namespace std;
using namespace std::literals;

const auto info = [](auto&&... an){
//    cout << an... << endl;
};

auto even = [](auto v){return (v % 2) == 0;};

auto always_throw = [](auto... ){
    throw runtime_error("always throw!");
    return true;
};

struct destruction
{
    ~destruction(){
        info("destructed");
    }
};

#include "rxcommon.h"
#include "designcontext.h"

int main() {
    //emscripten_set_main_loop(tick, -1, false);
    designcontext(0, 1000000);
    return 0;
}

