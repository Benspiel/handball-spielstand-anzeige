#pragma once
// Verbindet Kamera -> Feld-Erkennung -> Tracker -> gemeinsamer Zustand.

#include "camera_capture.hpp"
#include "config.hpp"
#include "match_tracker.hpp"
#include "seven_segment.hpp"
#include "shared_state.hpp"

namespace hbp {

class Engine {
public:
	Engine();

	void setConfig(const Config &cfg);
	const Config &config() const { return cfg_; }

	// Ein Erkennungsdurchlauf (vom Dock-Timer aufgerufen, ca. 4x pro Sekunde).
	void step(double nowSeconds);

	const Frame &lastFrame() const { return frame_; }
	bool hasFrame() const { return frameValid_; }
	const hb::FieldReading &lastReading(int field) const { return readings_[field]; }
	const hb::MatchState &state() const { return tracker_.state(); }
	const std::string &cameraError() const { return capture_.lastError(); }
	bool cameraOpen() const { return capture_.isOpen(); }

	// Manuelle Eingriffe
	void setScore(int home, int away);
	void setHalf(int half);
	void resetMatch();

private:
	void publish();

	Config cfg_;
	CameraCapture capture_;
	hb::MatchTracker tracker_;
	Frame frame_;
	bool frameValid_ = false;
	hb::FieldReading readings_[FieldCount];
};

} // namespace hbp
