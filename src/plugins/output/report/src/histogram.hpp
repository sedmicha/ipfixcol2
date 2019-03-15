#pragma once
#include <vector>
#include <climits>

struct Histogram {
    struct value_s {
        int from;
        int to;
        int count;
    };

    int from = 0;
    int to = 0;
    int bin_width = 0;
    int length = 0;
    std::vector<int> counts;

    Histogram() {}

    Histogram(int from, int to, int bin_width) : from(from), to(to), bin_width(bin_width)
    {
        length = (to - from) / bin_width + 2;
        counts.resize(length, 0);
    }

    void
    operator()(int value)
    {
        if (value < from) {
            counts[0]++;
        } else if (value >= to) {
            counts[length - 1]++;
        } else {
            counts[(value - from) / bin_width + 1]++;
        }
    }

    value_s operator[](int index) const
    {
        if (index == 0) {
            return {INT_MIN, from, counts[0]};
        } else if (index == length - 1) {
            return {to, INT_MAX, counts[length - 1]};
        } else {
            int x = (index - 1) * bin_width + from;
            return {x, x + bin_width, counts[index]};
        }
    }
};