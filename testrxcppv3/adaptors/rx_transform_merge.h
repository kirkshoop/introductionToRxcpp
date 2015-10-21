#pragma once

namespace rx {

template<class MakeStrand, class F>
auto transform_merge(MakeStrand&& makeStrand, F&& f) {
    return transform(forward<F>(f)) | merge(forward<MakeStrand>(makeStrand));
};

}