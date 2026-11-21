// Handball Spielstand – OBS-Plugin
// Liest die Anzeigetafel einer Kamera aus und stellt den Spielstand als Quelle bereit.

#include "config.hpp"
#include "dock.hpp"
#include "plugin-support.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/platform.h>

#include <QMainWindow>

OBS_DECLARE_MODULE()

namespace {

constexpr const char *kDockId = "handball-scoreboard-dock";

hbp::HandballDock *g_dock = nullptr;

void onFrontendEvent(enum obs_frontend_event event, void *)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		// Erst jetzt existieren Szenen und Quellen, die im Kamera-Menü auftauchen sollen.
		if (!g_dock) {
			// Beim allerersten Start das Dock sichtbar machen, danach merkt sich OBS die Sichtbarkeit.
			const bool firstRun = !os_file_exists(hbp::configFilePath().c_str());
			g_dock = new hbp::HandballDock();
			if (!obs_frontend_add_dock_by_id(kDockId, "Handball Spielstand", g_dock)) {
				blog(LOG_ERROR, "[handball-scoreboard] Dock konnte nicht hinzugefügt werden");
				delete g_dock;
				g_dock = nullptr;
			} else if (firstRun && g_dock->parentWidget()) {
				g_dock->parentWidget()->show();
			}
		}
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		if (g_dock)
			g_dock->shutdown();
		break;
	default:
		break;
	}
}

} // namespace

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Liest den Handball-Spielstand von der Hallenanzeige und stellt ihn als OBS-Quelle bereit.";
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return "Handball Spielstand";
}

bool obs_module_load(void)
{
	hbp::registerScorebugSource();
	obs_frontend_add_event_callback(onFrontendEvent, nullptr);
	blog(LOG_INFO, "[handball-scoreboard] geladen");
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(onFrontendEvent, nullptr);
	// Das Dock gehört nach add_dock OBS und wird von dort freigegeben.
	g_dock = nullptr;
}
