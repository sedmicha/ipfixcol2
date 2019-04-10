/**
 * \file src/plugins/output/report/src/histogram.hpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Simple histogram class for report output plugin (header file)
 * \date 2019
 */

/* Copyright (C) 2019 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

#ifndef PLUGIN_REPORT__HISTOGRAM_HPP
#define PLUGIN_REPORT__HISTOGRAM_HPP

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

    /**
     * \brief      Creates a histogram given an interval and the width of a bin.
     *             
     * \param[in]  from       The lower bound of the interval of values
     * \param[in]  to         The upper bound of the interval of values
     * \param[in]  bin_width  The width of one bin
     */
    Histogram(int from, int to, int bin_width) : from(from), to(to), bin_width(bin_width)
    {
        length = (to - from) / bin_width + 2;
        counts.resize(length, 0);
    }

    /**
     * Records a value to the histogram.
     */
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

    /**
     * \brief      Gets a value of a histogram bin.
     *
     * \param[in]  index  The index of the bin
     *
     * \return     The interval and count of the bin
     */
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

#endif // PLUGIN_REPORT__HISTOGRAM_HPP