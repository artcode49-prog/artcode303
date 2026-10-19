// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 artcode49-prog  <https://github.com/artcode49-prog/artcode303>
// ACID-303 is free software licensed under the GNU AGPL v3 or later; see LICENSE.

// Tb303Transport.h - decides whether the internal sequencer should be running.
//
// Two clock sources share one PLAY button:
//
//   Internal -- the button *is* the transport.
//   Host     -- the DAW transport drives the sequencer, but the button can
//               still stop the current run. A fresh host transport start
//               re-arms it, so a synced 303 plays again the next time the DAW
//               rolls, which is what users expect.
//
// The class is plain C++ (no JUCE) so the offline renderer can exercise the
// exact same state machine with unit tests.

#pragma once

#include <atomic>

namespace tb303
{

class TransportGate
{
public:
    // Effective transport state for the current audio block.
    // hostSync = the plugin is following the host clock.
    bool update (bool hostSync, bool hostPlaying)
    {
        if (! hostSync)
        {
            hostWasPlaying = false;
            return ! userStop.load();
        }

        if (hostPlaying && ! hostWasPlaying)
            userStop.store (false);          // a fresh host start re-arms

        hostWasPlaying = hostPlaying;

        return hostPlaying && ! userStop.load();
    }

    // Called from the message thread when the PLAY button is toggled.
    void requestPlay (bool shouldPlay) { userStop.store (! shouldPlay); }

    // True while the user has not stopped the sequencer from the GUI.
    bool isRequested() const { return ! userStop.load(); }

private:
    std::atomic<bool> userStop { true };     // stopped until the transport runs
    bool hostWasPlaying = false;
};

} // namespace tb303
