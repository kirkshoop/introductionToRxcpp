#pragma once

namespace rx {

struct joiner {    
};
joiner join() {
    return {};
}

/// \brief chain operator overload for
/// void = Subscription | Joiner
/// \param subscription
/// \param joiner
/// \returns void
void operator|(subscription s, joiner ) {
    s.join();
}

}