#include "engine.hpp"

#include <algorithm>

namespace hbp {

Engine::Engine() = default;

void Engine::setConfig(const Config &cfg)
{
	cfg_ = cfg;

	hb::TrackerConfig tc = tracker_.config();
	tc.halfMinutes = cfg_.halfMinutes;
	tracker_.setConfig(tc);
	tracker_.setScoreAutomatic(cfg_.scoreAutomatic);

	capture_.setTarget(cfg_.cameraKind, cfg_.cameraId, cfg_.resolution);
	publish();
}

void Engine::publish()
{
	Snapshot s;
	s.match = tracker_.state();
	s.homeName = cfg_.homeName;
	s.awayName = cfg_.awayName;
	s.homeColor = cfg_.homeColor;
	s.awayColor = cfg_.awayColor;
	SharedState::instance().publish(s);
}

void Engine::step(double now)
{
	frameValid_ = capture_.grab(frame_);
	if (!frameValid_) {
		for (hb::FieldReading &r : readings_)
			r = hb::FieldReading();
		if (tracker_.tick(now))
			publish();
		return;
	}

	if (!cfg_.detectionEnabled) {
		for (hb::FieldReading &r : readings_)
			r = hb::FieldReading();
		if (tracker_.tick(now))
			publish();
		return;
	}

	const hb::FrameView view{frame_.bgra.data(), frame_.width, frame_.height, frame_.width * 4};
	hb::SegmentOptions opt;
	opt.segmentThreshold = std::clamp(cfg_.sensitivity, 15, 80) / 100.0;
	opt.slant = cfg_.slantPercent / 100.0;

	hb::Observation obs;
	obs.time = now;

	for (int i = 0; i < FieldCount; ++i) {
		const FieldConfig &fc = cfg_.fields[i];
		if (!fc.rect.valid()) {
			readings_[i] = hb::FieldReading();
			continue;
		}
		readings_[i] = hb::SevenSegmentReader::read(view, fc.rect, fc.digits, opt);
	}

	auto good = [&](int i) -> std::optional<int> {
		const hb::FieldReading &r = readings_[i];
		if (r.status == hb::ReadStatus::Ok && r.confidence >= 0.05)
			return r.value;
		return std::nullopt;
	};

	obs.home = good(FieldHome);
	obs.away = good(FieldAway);
	obs.minutes = good(FieldMinutes);
	obs.seconds = good(FieldSeconds);
	obs.half = good(FieldHalf);
	obs.anyFieldVisible = obs.home || obs.away || obs.minutes || obs.seconds || obs.half;

	if (tracker_.update(obs))
		publish();
}

void Engine::setScore(int home, int away)
{
	tracker_.setScore(home, away);
	publish();
}

void Engine::setHalf(int half)
{
	tracker_.setHalf(half);
	publish();
}

void Engine::resetMatch()
{
	tracker_.resetAll();
	publish();
}

} // namespace hbp
