#pragma once
// Holt Einzelbilder aus einer Kamera oder einer vorhandenen OBS-Quelle.

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

extern "C" {
struct obs_source;
typedef struct obs_source obs_source_t;
struct gs_texture_render;
typedef struct gs_texture_render gs_texrender_t;
struct gs_stage_surface;
typedef struct gs_stage_surface gs_stagesurf_t;
}

namespace hbp {

struct CameraEntry {
	std::string label;
	std::string kind; // "device" | "source"
	std::string id;
};

// Alle wählbaren Kameras (direkt öffnen) und vorhandenen OBS-Videoquellen.
std::vector<CameraEntry> listCameras();

struct Frame {
	int width = 0;
	int height = 0;
	std::vector<uint8_t> bgra; // width * height * 4
};

class CameraCapture {
public:
	CameraCapture() = default;
	~CameraCapture();
	CameraCapture(const CameraCapture &) = delete;
	CameraCapture &operator=(const CameraCapture &) = delete;

	// Ziel setzen (idempotent: gleiche Werte öffnen nichts neu).
	void setTarget(const std::string &kind, const std::string &id, const std::string &resolution);
	void close();

	// Aktuelles Bild holen. Muss im Qt-Hauptthread aufgerufen werden.
	bool grab(Frame &out);

	const std::string &lastError() const { return error_; }
	bool isOpen() const { return source_ != nullptr; }

private:
	void releaseGraphics();

	std::string kind_, id_, resolution_;
	obs_source_t *source_ = nullptr;
	bool owned_ = false; // true = privat erzeugte Kamera-Quelle, die wir selbst aufräumen
	gs_texrender_t *texrender_ = nullptr;
	gs_stagesurf_t *stage_ = nullptr;
	uint32_t stageW_ = 0, stageH_ = 0;
	std::string error_;
};

} // namespace hbp
