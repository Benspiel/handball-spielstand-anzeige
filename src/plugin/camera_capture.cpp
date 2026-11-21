#include "camera_capture.hpp"

#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/vec4.h>

#include <algorithm>
#include <cstring>

namespace hbp {

namespace {

#if defined(_WIN32)
constexpr const char *kDeviceSourceId = "dshow_input";
constexpr const char *kDeviceKey = "video_device_id";
#elif defined(__APPLE__)
constexpr const char *kDeviceSourceId = "av_capture_input";
constexpr const char *kDeviceKey = "device";
#else
constexpr const char *kDeviceSourceId = "v4l2_input";
constexpr const char *kDeviceKey = "device_id";
#endif

constexpr const char *kScorebugId = "handball_scorebug";
constexpr uint32_t kMaxCaptureWidth = 1920;

struct EnumCtx {
	std::vector<CameraEntry> *out;
};

bool enumSource(void *param, obs_source_t *src)
{
	auto *ctx = static_cast<EnumCtx *>(param);
	const char *id = obs_source_get_id(src);
	if (id && std::strcmp(id, kScorebugId) == 0)
		return true;
	if (!(obs_source_get_output_flags(src) & OBS_SOURCE_VIDEO))
		return true;
	const char *name = obs_source_get_name(src);
	if (!name || !*name)
		return true;
	ctx->out->push_back({std::string("OBS-Quelle: ") + name, "source", name});
	return true;
}

bool enumScene(void *param, obs_source_t *src)
{
	auto *ctx = static_cast<EnumCtx *>(param);
	const char *name = obs_source_get_name(src);
	if (name && *name)
		ctx->out->push_back({std::string("OBS-Szene: ") + name, "source", name});
	return true;
}

} // namespace

std::vector<CameraEntry> listCameras()
{
	std::vector<CameraEntry> list;

#if defined(_WIN32) || defined(__APPLE__)
	// Geräteliste von der OBS-eigenen Kamera-Quelle erfragen (gleiche Liste wie im "Videoaufnahmegerät").
	// Das ist derselbe Ablauf wie beim Hinzufügen eines Videoaufnahmegeräts in OBS: Quelle ohne
	// Gerät anlegen, Eigenschaften abfragen. Unter Linux ist das nicht sicher (OBS' V4L2-Plugin stürzt
	// ohne Kamera ab), deshalb gibt es dort nur die OBS-Quellen unten. Das Plugin ist für Windows gedacht.
	obs_source_t *probe = obs_source_create_private(kDeviceSourceId, "hb-probe", nullptr);
	if (probe) {
		obs_properties_t *props = obs_source_properties(probe);
		if (props) {
			obs_property_t *p = obs_properties_get(props, kDeviceKey);
			if (p) {
				const size_t n = obs_property_list_item_count(p);
				for (size_t i = 0; i < n; ++i) {
					const char *name = obs_property_list_item_name(p, i);
					const char *val = obs_property_list_item_string(p, i);
					if (!name || !val || !*val)
						continue;
					list.push_back({std::string("Kamera: ") + name, "device", val});
				}
			}
			obs_properties_destroy(props);
		}
		obs_source_release(probe);
	}
#endif

	EnumCtx ctx{&list};
	obs_enum_sources(enumSource, &ctx);
	obs_enum_scenes(enumScene, &ctx);
	return list;
}

CameraCapture::~CameraCapture()
{
	close();
}

void CameraCapture::releaseGraphics()
{
	if (!texrender_ && !stage_)
		return;
	obs_enter_graphics();
	if (texrender_)
		gs_texrender_destroy(texrender_);
	if (stage_)
		gs_stagesurface_destroy(stage_);
	obs_leave_graphics();
	texrender_ = nullptr;
	stage_ = nullptr;
	stageW_ = stageH_ = 0;
}

void CameraCapture::close()
{
	if (source_) {
		if (owned_) {
			obs_source_dec_showing(source_);
			obs_source_dec_active(source_);
		}
		obs_source_release(source_);
		source_ = nullptr;
	}
	owned_ = false;
	releaseGraphics();
}

void CameraCapture::setTarget(const std::string &kind, const std::string &id, const std::string &resolution)
{
	// Bei "source" ändert sich die Auflösung nicht für uns, daher nur bei "device" vergleichen.
	const bool sameResolution = (kind != "device") || resolution == resolution_;
	if (kind == kind_ && id == id_ && sameResolution && (source_ || kind.empty()))
		return;

	close();
	kind_ = kind;
	id_ = id;
	resolution_ = resolution;
	error_.clear();

	if (kind.empty() || id.empty())
		return;

	if (kind == "source") {
		source_ = obs_get_source_by_name(id.c_str());
		if (!source_)
			error_ = "Quelle \"" + id + "\" nicht gefunden";
		return;
	}

	if (kind == "device") {
		obs_data_t *settings = obs_data_create();
		obs_data_set_string(settings, kDeviceKey, id.c_str());
#if defined(_WIN32)
		if (!resolution.empty()) {
			obs_data_set_int(settings, "res_type", 1); // benutzerdefiniert
			obs_data_set_string(settings, "resolution", resolution.c_str());
		}
#endif
		source_ = obs_source_create_private(kDeviceSourceId, "Handball Kamera", settings);
		obs_data_release(settings);
		if (!source_) {
			error_ = "Kamera konnte nicht geöffnet werden";
			return;
		}
		owned_ = true;
		obs_source_inc_showing(source_);
		obs_source_inc_active(source_);
	}
}

bool CameraCapture::grab(Frame &out)
{
	if (!source_)
		return false;

	const uint32_t w = obs_source_get_base_width(source_);
	const uint32_t h = obs_source_get_base_height(source_);
	if (w < 16 || h < 16)
		return false; // noch kein Bild

	uint32_t tw = w, th = h;
	if (tw > kMaxCaptureWidth) {
		th = uint32_t(uint64_t(h) * kMaxCaptureWidth / w);
		tw = kMaxCaptureWidth;
	}

	bool ok = false;
	obs_enter_graphics();

	if (!texrender_)
		texrender_ = gs_texrender_create(GS_BGRA, GS_ZS_NONE);
	if (!stage_ || stageW_ != tw || stageH_ != th) {
		if (stage_)
			gs_stagesurface_destroy(stage_);
		stage_ = gs_stagesurface_create(tw, th, GS_BGRA);
		stageW_ = tw;
		stageH_ = th;
	}

	if (texrender_ && stage_) {
		gs_texrender_reset(texrender_);
		if (gs_texrender_begin_with_color_space(texrender_, tw, th, GS_CS_SRGB)) {
			vec4 clear;
			vec4_zero(&clear);
			gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
			gs_ortho(0.0f, float(w), 0.0f, float(h), -100.0f, 100.0f);

			gs_blend_state_push();
			gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
			obs_source_video_render(source_);
			gs_blend_state_pop();
			gs_texrender_end(texrender_);

			gs_stage_texture(stage_, gs_texrender_get_texture(texrender_));
			uint8_t *data = nullptr;
			uint32_t linesize = 0;
			if (gs_stagesurface_map(stage_, &data, &linesize)) {
				out.width = int(tw);
				out.height = int(th);
				out.bgra.resize(size_t(tw) * size_t(th) * 4);
				for (uint32_t y = 0; y < th; ++y)
					std::memcpy(&out.bgra[size_t(y) * tw * 4], data + size_t(y) * linesize, size_t(tw) * 4);
				gs_stagesurface_unmap(stage_);
				ok = true;
			}
		}
	}

	obs_leave_graphics();
	return ok;
}

} // namespace hbp
