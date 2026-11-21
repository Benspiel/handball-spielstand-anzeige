// OBS-Quelle "Handball Spielstand": zeigt den erkannten Spielstand als fertige Grafik.

#include "plugin-support.hpp"
#include "scorebug.hpp"
#include "shared_state.hpp"

#include <obs-module.h>
#include <graphics/graphics.h>

#include <memory>

namespace hbp {

namespace {

struct ScorebugSource {
	obs_source_t *source = nullptr;
	ScorebugStyle style;
	QImage image;
	gs_texture_t *texture = nullptr;
	uint64_t renderedVersion = 0;
	bool dirty = true;
};

void applySettings(ScorebugSource *s, obs_data_t *settings)
{
	obs_data_t *font = obs_data_get_obj(settings, "font");
	if (font) {
		const char *face = obs_data_get_string(font, "face");
		if (face && *face)
			s->style.fontFamily = QString::fromUtf8(face);
		obs_data_release(font);
	}
	s->style.theme = int(obs_data_get_int(settings, "theme"));
	s->style.showTime = obs_data_get_bool(settings, "show_time");
	s->style.showHalf = obs_data_get_bool(settings, "show_half");
	s->dirty = true;
}

const char *sbName(void *)
{
	return "Handball Spielstand";
}

void *sbCreate(obs_data_t *settings, obs_source_t *source)
{
	auto *s = new ScorebugSource();
	s->source = source;
	applySettings(s, settings);
	return s;
}

void sbDestroy(void *data)
{
	auto *s = static_cast<ScorebugSource *>(data);
	if (s->texture) {
		obs_enter_graphics();
		gs_texture_destroy(s->texture);
		obs_leave_graphics();
	}
	delete s;
}

void sbUpdate(void *data, obs_data_t *settings)
{
	applySettings(static_cast<ScorebugSource *>(data), settings);
}

void sbDefaults(obs_data_t *settings)
{
	obs_data_t *font = obs_data_create();
	obs_data_set_default_string(font, "face", "Segoe UI");
	obs_data_set_default_string(font, "style", "Bold");
	obs_data_set_default_int(font, "size", 48);
	obs_data_set_default_int(font, "flags", 1);
	obs_data_set_default_obj(settings, "font", font);
	obs_data_release(font);

	obs_data_set_default_int(settings, "theme", 0);
	obs_data_set_default_bool(settings, "show_time", true);
	obs_data_set_default_bool(settings, "show_half", true);
}

obs_properties_t *sbProperties(void *)
{
	obs_properties_t *props = obs_properties_create();

	obs_property_t *theme = obs_properties_add_list(props, "theme", "Design", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(theme, "Dunkel", 0);
	obs_property_list_add_int(theme, "Hell", 1);

	obs_properties_add_font(props, "font", "Schriftart");
	obs_properties_add_bool(props, "show_time", "Spielzeit anzeigen");
	obs_properties_add_bool(props, "show_half", "Halbzeit anzeigen");
	obs_properties_add_text(props, "info",
				"Spielstand, Zeit, Teamnamen und -farben stellst du im Dock \"Handball Spielstand\" ein.",
				OBS_TEXT_INFO);
	return props;
}

uint32_t sbWidth(void *)
{
	return kScorebugWidth;
}

uint32_t sbHeight(void *)
{
	return kScorebugHeight;
}

void sbTick(void *data, float)
{
	auto *s = static_cast<ScorebugSource *>(data);
	const uint64_t v = SharedState::instance().version();
	if (v != s->renderedVersion)
		s->dirty = true;
}

void sbRender(void *data, gs_effect_t *)
{
	auto *s = static_cast<ScorebugSource *>(data);
	// Bei OBS_SOURCE_CUSTOM_DRAW übergibt OBS keinen Effekt, wir holen den Standardeffekt selbst.
	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	if (!effect)
		return;

	// Auch hier die Version prüfen, nicht nur im Tick: Ein Tick kommt nicht in jedem Fall an
	// (z.B. bei ausgeblendeten Quellen oder im Vorschau-Zweig), sonst bliebe die Grafik auf alten Werten stehen.
	if (s->dirty || !s->texture || SharedState::instance().version() != s->renderedVersion) {
		const Snapshot snap = SharedState::instance().read();
		s->image = renderScorebug(snap, s->style);
		s->renderedVersion = snap.version;
		s->dirty = false;

		const uint8_t *bits = s->image.constBits();
		if (!s->texture) {
			s->texture = gs_texture_create(uint32_t(s->image.width()), uint32_t(s->image.height()), GS_RGBA, 1,
						       &bits, GS_DYNAMIC);
		} else {
			gs_texture_set_image(s->texture, bits, uint32_t(s->image.bytesPerLine()), false);
		}
	}
	if (!s->texture)
		return;

	// Wie die OBS-Bildquelle: vormultipliziertes Alpha, sRGB-korrekt.
	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(true);
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);

	gs_eparam_t *param = gs_effect_get_param_by_name(effect, "image");
	gs_effect_set_texture_srgb(param, s->texture);
	while (gs_effect_loop(effect, "Draw"))
		gs_draw_sprite(s->texture, 0, kScorebugWidth, kScorebugHeight);

	gs_blend_state_pop();
	gs_enable_framebuffer_srgb(previous);
}

} // namespace

void registerScorebugSource()
{
	static obs_source_info info = {};
	info.id = "handball_scorebug";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_SRGB;
	info.get_name = sbName;
	info.create = sbCreate;
	info.destroy = sbDestroy;
	info.update = sbUpdate;
	info.get_defaults = sbDefaults;
	info.get_properties = sbProperties;
	info.get_width = sbWidth;
	info.get_height = sbHeight;
	info.video_tick = sbTick;
	info.video_render = sbRender;
	obs_register_source(&info);
}

} // namespace hbp
