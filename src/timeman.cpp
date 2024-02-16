/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2024 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "timeman.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>

#include "search.h"
#include "ucioption.h"

namespace Stockfish {
int timeman_cpp_96_0 = 9, timeman_cpp_96_1 = 142, timeman_cpp_96_2 = 100, timeman_cpp_100_0 = 344, timeman_cpp_100_1 = 2, timeman_cpp_100_2 = 45, timeman_cpp_101_0 = 39, timeman_cpp_101_1 = 31, timeman_cpp_101_2 = 25, timeman_cpp_103_0 = 155, timeman_cpp_103_1 = 30, timeman_cpp_103_2 = 45, timeman_cpp_104_0 = 2, timeman_cpp_106_0 = 65, timeman_cpp_106_1 = 136, timeman_cpp_119_0 = 81;
TUNE(timeman_cpp_96_0, timeman_cpp_96_1, timeman_cpp_96_2, timeman_cpp_100_0, timeman_cpp_100_1, timeman_cpp_100_2, timeman_cpp_101_0, timeman_cpp_101_1, timeman_cpp_101_2, timeman_cpp_103_0, timeman_cpp_103_1, timeman_cpp_103_2, timeman_cpp_104_0, timeman_cpp_106_0, timeman_cpp_106_1, timeman_cpp_119_0);

TimePoint TimeManagement::optimum() const { return optimumTime; }
TimePoint TimeManagement::maximum() const { return maximumTime; }
TimePoint TimeManagement::elapsed(size_t nodes) const {
    return useNodesTime ? TimePoint(nodes) : now() - startTime;
}

void TimeManagement::clear() {
    availableNodes = 0;  // When in 'nodes as time' mode
}

void TimeManagement::advance_nodes_time(std::int64_t nodes) {
    assert(useNodesTime);
    availableNodes += nodes;
}

// Called at the beginning of the search and calculates
// the bounds of time allowed for the current game ply. We currently support:
//      1) x basetime (+ z increment)
//      2) x moves in y seconds (+ z increment)
void TimeManagement::init(Search::LimitsType& limits,
                          Color               us,
                          int                 ply,
                          const OptionsMap&   options) {
    // If we have no time, no need to initialize TM, except for the start time,
    // which is used by movetime.
    startTime = limits.startTime;
    if (limits.time[us] == 0)
        return;

    TimePoint moveOverhead = TimePoint(options["Move Overhead"]);
    TimePoint npmsec       = TimePoint(options["nodestime"]);

    // optScale is a percentage of available time to use for the current move.
    // maxScale is a multiplier applied to optimumTime.
    double optScale, maxScale;

    // If we have to play in 'nodes as time' mode, then convert from time
    // to nodes, and use resulting values in time management formulas.
    // WARNING: to avoid time losses, the given npmsec (nodes per millisecond)
    // must be much lower than the real engine speed.
    if (npmsec)
    {
        useNodesTime = true;

        if (!availableNodes)                            // Only once at game start
            availableNodes = npmsec * limits.time[us];  // Time is in msec

        // Convert from milliseconds to nodes
        limits.time[us] = TimePoint(availableNodes);
        limits.inc[us] *= npmsec;
        limits.npmsec = npmsec;
    }

    // Maximum move horizon of 60 moves
    int mtg = limits.movestogo ? std::min(limits.movestogo, 60) : 60;

    // Make sure timeLeft is > 0 since we may use it as a divisor
    TimePoint timeLeft = std::max(TimePoint(1), limits.time[us] + limits.inc[us] * (mtg - 1)
                                                  - moveOverhead * (2 + mtg));

    // x basetime (+ z increment)
    // If there is a healthy increment, timeLeft can exceed actual available
    // game time for the current move, so also cap to 20% of available game time.
    if (limits.movestogo == 0)
    {
        // Use extra time with larger increments
        double optExtra = std::clamp((timeman_cpp_96_0 / 10.0) + (timeman_cpp_96_1 / 10.0) * limits.inc[us] / limits.time[us], 1.0, (timeman_cpp_96_2 / 100.0));

        // Calculate time constants based on current time left.
        double optConstant =
          std::min((timeman_cpp_100_0 / 100000.0) + (timeman_cpp_100_1 / 10000.0) * std::log10(limits.time[us] / 1000.0), (timeman_cpp_100_2 / 10000.0));
        double maxConstant = std::max((timeman_cpp_101_0 / 10.0) + (timeman_cpp_101_1 / 10.0) * std::log10(limits.time[us] / 1000.0), (timeman_cpp_101_2 / 10.0));

        optScale = std::min((timeman_cpp_103_0 / 10000.0) + std::pow(ply + (timeman_cpp_103_1 / 10.0), (timeman_cpp_103_2 / 100.0)) * optConstant,
                            (timeman_cpp_104_0 / 10.0) * limits.time[us] / double(timeLeft))
                 * optExtra;
        maxScale = std::min((timeman_cpp_106_0 / 10.0), maxConstant + ply / (timeman_cpp_106_1 / 10.0));
    }

    // x moves in y seconds (+ z increment)
    else
    {
        optScale = std::min((0.88 + ply / 116.4) / mtg, 0.88 * limits.time[us] / double(timeLeft));
        maxScale = std::min(6.3, 1.5 + 0.11 * mtg);
    }

    // Limit the maximum possible time for this move
    optimumTime = TimePoint(optScale * timeLeft);
    maximumTime =
      TimePoint(std::min((timeman_cpp_119_0 / 100.0) * limits.time[us] - moveOverhead, maxScale * optimumTime)) - 10;

    if (options["Ponder"])
        optimumTime += optimumTime / 4;
}

}  // namespace Stockfish
