#pragma once
// Einstellungen des Plugins (werden als JSON im OBS-Konfigurationsordner gespeichert).

#include "frame.hpp"

#include <cstdint>
#include <string>

namespace hbp {

enum FieldId { FieldHome = 0, FieldAway, FieldMinutes, FieldSeconds, FieldHalf, FieldCount };

const char *fieldKey(int field);   // "home", "away", ... (für die Speicherung)
const char *fieldLabel(int field); // deutscher Anzeigename

struct FieldConfig {
	hb::NormRect rect; // w == 0 -> nicht festgelegt
	int digits = 2;
};

struct Config {
	// Kamera: kind = "" (keine) | "device" (Kamera direkt öffnen) | "source" (vorhandene OBS-Quelle nutzen)
	std::string cameraKind;
	std::string cameraId;
	std::string resolution; // "" = Gerätestandard, sonst z.B. "1920x1080"

	FieldConfig fields[FieldCount];

	std::string homeName = "HEIM";
	std::string awayName = "GAST";
	uint32_t homeColor = 0xFFD32F2F; // ARGB
	uint32_t awayColor = 0xFF1976D2;

	int halfMinutes = 30;
	int sensitivity = 40;  // Segmentschwelle in Prozent
	int slantPercent = 0;  // Kursivität in Prozent der Ziffernbreite
	bool detectionEnabled = true;
	bool scoreAutomatic = true;

	Config();
};

std::string configFilePath();
Config loadConfig();
bool saveConfig(const Config &cfg);

} // namespace hbp
