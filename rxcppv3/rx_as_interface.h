#pragma once

namespace rx {

template<class... TN>
struct interface_extractor{
    template<class O>
    auto extract(O&& o){
        return o.template as_interface<TN...>();
    }
};
template<class... TN>
interface_extractor<TN...> as_interface() {
    return {};
}

/// \brief chain operator overload for
/// AnyInterface = Any | InterfaceExtractor
/// \param any
/// \param interface_extractor
/// \returns any_interface
template<class O, class... TN>
auto operator|(O&& o, interface_extractor<TN...>&& ie) {
    return ie.extract(forward<O>(o));
}

}