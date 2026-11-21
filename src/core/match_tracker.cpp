#include "match_tracker.hpp"

#include <algorithm>
#include <cstdlib>

namespace hb {

MatchTracker::MatchTracker(const TrackerConfig &config) : cfg_(config) {}

bool MatchTracker::updateScore(std::optional<int> reading, int &current, bool &known, IntStabilizer &stab)
{
	if (!reading) {
		return false; // unlesbar/leer: nichts ändern, Kandidat bleibt
	}
	const int value = *reading;
	if (known && value == current) {
		stab.clear();
		return false;
	}

	int need = cfg_.stableReadings;
	if (known) {
		const int diff = value - current;
		if (diff < 0 || diff > cfg_.maxScoreJump)
			need = cfg_.backwardsReadings;
	}
	if (stab.feed(value, need)) {
		current = value;
		known = true;
		return true;
	}
	return false;
}

bool MatchTracker::updateClock(int seconds, double now)
{
	if (!state_.clockKnown) {
		// Erste Lesung: wie ein normaler Wert bestätigen lassen.
		if (clockStab_.feed(seconds, std::max(2, cfg_.stableReadings - 1), 3)) {
			state_.clockKnown = true;
			state_.clockSeconds = seconds;
			minClockThisHalf_ = maxClockThisHalf_ = seconds;
			lastClockChange_ = now;
			return true;
		}
		return false;
	}

	if (seconds == state_.clockSeconds) {
		clockStab_.clear();
		return false;
	}

	const int diff = std::abs(seconds - state_.clockSeconds);
	const int need = diff <= 3 ? 2 : cfg_.backwardsReadings;
	if (!clockStab_.feed(seconds, need, 3))
		return false;

	state_.clockSeconds = seconds;
	lastClockChange_ = now;

	// Großer Sprung nach einer (fast) vollen Halbzeit -> nächste Halbzeit.
	// Nur wenn kein echtes Halbzeitfeld vorhanden ist und die Automatik an ist.
	const int halfSeconds = cfg_.halfMinutes * 60;
	if (diff > 5 * 60 && scoreAuto_ && !halfFieldSeen_) {
		const int span = maxClockThisHalf_ - minClockThisHalf_;
		if (span >= int(0.6 * halfSeconds) && state_.half < 4) {
			++state_.half;
			minClockThisHalf_ = maxClockThisHalf_ = seconds;
			return true;
		}
	}
	// Sonst (normale Uhr oder manuelle Uhrkorrektur): Verlauf fortschreiben.
	if (diff > 5 * 60) {
		minClockThisHalf_ = maxClockThisHalf_ = seconds;
	} else {
		minClockThisHalf_ = std::min(minClockThisHalf_, seconds);
		maxClockThisHalf_ = std::max(maxClockThisHalf_, seconds);
	}
	return true;
}

bool MatchTracker::updateHalf(int reading)
{
	if (reading < 1 || reading > 4)
		return false;
	if (reading == state_.half) {
		halfStab_.clear();
		return false;
	}
	if (halfStab_.feed(reading, cfg_.stableReadings + 1)) {
		state_.half = reading;
		minClockThisHalf_ = maxClockThisHalf_ = state_.clockSeconds;
		return true;
	}
	return false;
}

bool MatchTracker::update(const Observation &obs)
{
	bool changed = false;

	if (obs.anyFieldVisible)
		lastVisible_ = obs.time;
	const bool signal = (obs.time - lastVisible_) <= cfg_.signalTimeout;
	if (signal != state_.signal) {
		state_.signal = signal;
		changed = true;
	}

	if (scoreAuto_) {
		const bool a = updateScore(obs.home, state_.home, homeKnown_, homeStab_);
		const bool b = updateScore(obs.away, state_.away, awayKnown_, awayStab_);
		if (a || b) {
			state_.scoreKnown = true;
			changed = true;
		}
	}

	if (obs.minutes && obs.seconds && *obs.seconds >= 0 && *obs.seconds < 60 && *obs.minutes >= 0)
		changed |= updateClock(*obs.minutes * 60 + *obs.seconds, obs.time);

	if (obs.half && *obs.half >= 1 && *obs.half <= 4)
		halfFieldSeen_ = true;
	if (scoreAuto_ && obs.half)
		changed |= updateHalf(*obs.half);

	// "Läuft die Uhr?"
	const bool running = state_.clockKnown && (obs.time - lastClockChange_) <= cfg_.clockRunningWindow;
	if (running != state_.clockRunning) {
		state_.clockRunning = running;
		changed = true;
	}
	return changed;
}

bool MatchTracker::tick(double now)
{
	bool changed = false;
	const bool signal = (now - lastVisible_) <= cfg_.signalTimeout;
	if (signal != state_.signal) {
		state_.signal = signal;
		changed = true;
	}
	const bool running = state_.clockKnown && (now - lastClockChange_) <= cfg_.clockRunningWindow;
	if (running != state_.clockRunning) {
		state_.clockRunning = running;
		changed = true;
	}
	return changed;
}

void MatchTracker::setScore(int home, int away)
{
	state_.home = std::max(0, home);
	state_.away = std::max(0, away);
	state_.scoreKnown = true;
	homeKnown_ = awayKnown_ = true;
	homeStab_.clear();
	awayStab_.clear();
}

void MatchTracker::setHalf(int half)
{
	state_.half = std::clamp(half, 1, 4);
	halfStab_.clear();
	minClockThisHalf_ = maxClockThisHalf_ = state_.clockSeconds;
}

void MatchTracker::resetAll()
{
	const bool automatic = scoreAuto_;
	state_ = MatchState();
	homeKnown_ = awayKnown_ = false;
	halfFieldSeen_ = false;
	homeStab_.clear();
	awayStab_.clear();
	clockStab_.clear();
	halfStab_.clear();
	lastVisible_ = -1e9;
	lastClockChange_ = -1e9;
	minClockThisHalf_ = maxClockThisHalf_ = 0;
	scoreAuto_ = automatic;
}

} // namespace hb
