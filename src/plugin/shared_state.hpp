#pragma once
// Brücke zwischen Erkennung (Dock) und Anzeige (Scorebug-Quelle).
// Die Quelle wird im Grafik-Thread gerendert, das Dock läuft im Qt-Thread -> Zugriff über Mutex.

#include "match_tracker.hpp"

#include <cstdint>
#include <mutex>
#include <string>

namespace hbp {

struct Snapshot {
	hb::MatchState match;
	std::string homeName = "HEIM";
	std::string awayName = "GAST";
	uint32_t homeColor = 0xFFD32F2F;
	uint32_t awayColor = 0xFF1976D2;
	uint64_t version = 0;
};

class SharedState {
public:
	static SharedState &instance();

	void publish(const Snapshot &snapshot);
	Snapshot read() const;
	uint64_t version() const; // günstiger Änderungs-Check ohne Kopie

private:
	mutable std::mutex mutex_;
	Snapshot snapshot_;
	uint64_t counter_ = 0;
};

} // namespace hbp
