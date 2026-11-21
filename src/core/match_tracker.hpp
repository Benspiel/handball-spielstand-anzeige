#pragma once
// Macht aus den rohen, flackernden Einzelbild-Lesungen einen stabilen Spielstand.
//
//  * Ein neuer Wert wird erst übernommen, wenn er in mehreren Lesungen hintereinander gleich war.
//  * Tore dürfen nur um wenige Tore steigen. Sinkende oder stark springende Werte brauchen
//    deutlich mehr Bestätigungen (Schiedsrichter-Korrektur ist möglich, Lesefehler eher nicht).
//  * Die Halbzeit kommt aus einem eigenen Anzeigefeld, oder wird sonst aus dem Zeitverlauf
//    abgeleitet (Uhr springt nach einer vollen Halbzeit zurück/vor).

#include <cstdlib>
#include <optional>

namespace hb {

struct Observation {
	double time = 0.0;               // Zeitstempel in Sekunden (monoton)
	std::optional<int> home;         // nur gesetzt, wenn das Feld sicher gelesen wurde
	std::optional<int> away;
	std::optional<int> minutes;
	std::optional<int> seconds;
	std::optional<int> half;
	bool anyFieldVisible = false;    // mindestens ein Feld war lesbar (für die Signal-Anzeige)
};

struct TrackerConfig {
	int halfMinutes = 30;          // Länge einer Halbzeit
	int stableReadings = 3;        // Bestätigungen für einen neuen Wert
	int backwardsReadings = 10;    // Bestätigungen für sinkende / stark springende Tore
	int maxScoreJump = 3;          // größter "normaler" Torsprung
	double signalTimeout = 5.0;    // Sekunden ohne Lesung -> "Kein Signal"
	double clockRunningWindow = 2.5; // Uhr gilt als laufend, wenn sie sich in diesem Zeitraum änderte
};

struct MatchState {
	int home = 0;
	int away = 0;
	bool scoreKnown = false;   // wurde schon mal ein Spielstand gelesen/gesetzt?
	bool clockKnown = false;
	int clockSeconds = 0;      // wie auf der Tafel angezeigt
	bool clockRunning = false;
	int half = 1;              // 1, 2, 3 = Verlängerung 1, 4 = Verlängerung 2
	bool signal = false;
};

class MatchTracker {
public:
	explicit MatchTracker(const TrackerConfig &config = TrackerConfig());

	void setConfig(const TrackerConfig &config) { cfg_ = config; }
	const TrackerConfig &config() const { return cfg_; }

	// Neue Lesung einspeisen. Liefert true, wenn sich der sichtbare Zustand geändert hat.
	bool update(const Observation &obs);

	// Bei Zeitstempel `now` den Signalstatus neu bewerten (auch ohne neue Lesung aufrufbar).
	bool tick(double now);

	const MatchState &state() const { return state_; }

	// Manuelle Eingriffe (Dock-Buttons)
	void setScore(int home, int away);
	void setHalf(int half);
	void resetAll();

	// Wenn true, werden Tore und Halbzeit nicht automatisch geändert (nur die Uhr läuft weiter mit).
	void setScoreAutomatic(bool automatic) { scoreAuto_ = automatic; }
	bool scoreAutomatic() const { return scoreAuto_; }

private:
	struct IntStabilizer {
		int candidate = -1;
		int count = 0;
		void clear()
		{
			candidate = -1;
			count = 0;
		}
		// true, wenn `value` nach `need` gleichen Lesungen angenommen werden soll.
		// `tolerance`: Werte, die höchstens so weit vom Kandidaten entfernt sind, zählen als derselbe
		// Kandidat (nötig für eine laufende Uhr, die sich zwischen den Lesungen weiterbewegt).
		bool feed(int value, int need, int tolerance = 0)
		{
			if (candidate >= 0 && std::abs(value - candidate) <= tolerance) {
				candidate = value;
				++count;
			} else {
				candidate = value;
				count = 1;
			}
			if (count >= need) {
				clear();
				return true;
			}
			return false;
		}
	};

	bool updateScore(std::optional<int> reading, int &current, bool &known, IntStabilizer &stab);
	bool updateClock(int seconds, double now);
	bool updateHalf(int reading);

	TrackerConfig cfg_;
	MatchState state_;
	bool scoreAuto_ = true;
	bool homeKnown_ = false;
	bool awayKnown_ = false;
	bool halfFieldSeen_ = false; // ein echtes Halbzeitfeld liefert Werte -> keine Ableitung aus der Uhr

	IntStabilizer homeStab_, awayStab_, clockStab_, halfStab_;

	double lastVisible_ = -1e9;
	double lastClockChange_ = -1e9;
	int minClockThisHalf_ = 0;
	int maxClockThisHalf_ = 0;
};

} // namespace hb
