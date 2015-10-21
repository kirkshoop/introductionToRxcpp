#pragma once

namespace rx {

template<class Clock, class Observer>
struct observe_at;

template<class Clock, class... ON>
struct observe_at<Clock, observer<ON...>>
{
    using clock_type = decay_t<Clock>;
    using observer_type = observer<ON...>;

    observe_at(time_point<clock_type> when, observer_type o)
        : when(when)
        , what(std::move(o))
    {
    }
    time_point<clock_type> when;
    observer_type what;
};


// Sorts observe_at items in priority order sorted
// on value of observe_at.when. Items with equal
// values for when are sorted in fifo order.

template<class Clock, class Observer>
class observe_at_queue;

template<class Clock, class... ON>
class observe_at_queue<Clock, observer<ON...>> {
public:
    using clock_type = decay_t<Clock>;
    using observer_type = observer<ON...>;
    using item_type = observe_at<clock_type, observer_type>;
    using elem_type = std::pair<item_type, int64_t>;
    using container_type = std::vector<elem_type>;
    using const_reference = const item_type&;

private:
    struct compare_elem
    {
        bool operator()(const elem_type& lhs, const elem_type& rhs) const {
            if (lhs.first.when == rhs.first.when) {
                return lhs.second > rhs.second;
            }
            else {
                return lhs.first.when > rhs.first.when;
            }
        }
    };

    typedef std::priority_queue<
        elem_type,
        container_type,
        compare_elem
    > queue_type;

    queue_type q;

    int64_t ordinal;
public:
    const_reference top() const {
        return q.top().first;
    }

    void pop() {
        q.pop();
    }

    bool empty() const {
        return q.empty();
    }

    void push(const item_type& value) {
        q.push(elem_type(value, ordinal++));
    }

    void push(item_type&& value) {
        q.push(elem_type(std::move(value), ordinal++));
    }
};

}