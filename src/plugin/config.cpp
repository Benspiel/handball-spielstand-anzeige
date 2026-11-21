#include "config.hpp"

#include <obs-module.h>
#include <util/platform.h>

namespace hbp {

const char *fieldKey(int field)
{
	static const char *k[FieldCount] = {"home", "away", "minutes", "seconds", "half"};
	return field >= 0 && field < FieldCount ? k[field] : "";
}

const char *fieldLabel(int field)
{
	static const char *l[FieldCount] = {"Tore Heim", "Tore Gast", "Minuten", "Sekunden", "Halbzeit (optional)"};
	return field >= 0 && field < FieldCount ? l[field] : "";
}

Config::Config()
{
	fields[FieldHome].digits = 2;
	fields[FieldAway].digits = 2;
	fields[FieldMinutes].digits = 2;
	fields[FieldSeconds].digits = 2;
	fields[FieldHalf].digits = 1;
}

std::string configFilePath()
{
	char *p = obs_module_config_path("settings.json");
	std::string s = p ? p : "";
	bfree(p);
	return s;
}

Config loadConfig()
{
	Config cfg;
	const std::string path = configFilePath();
	if (path.empty())
		return cfg;

	obs_data_t *d = obs_data_create_from_json_file_safe(path.c_str(), "bak");
	if (!d)
		return cfg;

	cfg.cameraKind = obs_data_get_string(d, "camera_kind");
	cfg.cameraId = obs_data_get_string(d, "camera_id");
	cfg.resolution = obs_data_get_string(d, "resolution");
	if (obs_data_has_user_value(d, "home_name"))
		cfg.homeName = obs_data_get_string(d, "home_name");
	if (obs_data_has_user_value(d, "away_name"))
		cfg.awayName = obs_data_get_string(d, "away_name");
	if (obs_data_has_user_value(d, "home_color"))
		cfg.homeColor = uint32_t(obs_data_get_int(d, "home_color"));
	if (obs_data_has_user_value(d, "away_color"))
		cfg.awayColor = uint32_t(obs_data_get_int(d, "away_color"));
	if (obs_data_has_user_value(d, "half_minutes"))
		cfg.halfMinutes = int(obs_data_get_int(d, "half_minutes"));
	if (obs_data_has_user_value(d, "sensitivity"))
		cfg.sensitivity = int(obs_data_get_int(d, "sensitivity"));
	if (obs_data_has_user_value(d, "slant"))
		cfg.slantPercent = int(obs_data_get_int(d, "slant"));
	if (obs_data_has_user_value(d, "detection_enabled"))
		cfg.detectionEnabled = obs_data_get_bool(d, "detection_enabled");
	if (obs_data_has_user_value(d, "score_automatic"))
		cfg.scoreAutomatic = obs_data_get_bool(d, "score_automatic");

	obs_data_t *fields = obs_data_get_obj(d, "fields");
	if (fields) {
		for (int i = 0; i < FieldCount; ++i) {
			obs_data_t *f = obs_data_get_obj(fields, fieldKey(i));
			if (!f)
				continue;
			FieldConfig &fc = cfg.fields[i];
			fc.rect.x = obs_data_get_double(f, "x");
			fc.rect.y = obs_data_get_double(f, "y");
			fc.rect.w = obs_data_get_double(f, "w");
			fc.rect.h = obs_data_get_double(f, "h");
			if (obs_data_has_user_value(f, "digits"))
				fc.digits = int(obs_data_get_int(f, "digits"));
			obs_data_release(f);
		}
		obs_data_release(fields);
	}
	obs_data_release(d);

	if (cfg.halfMinutes < 1 || cfg.halfMinutes > 60)
		cfg.halfMinutes = 30;
	for (FieldConfig &fc : cfg.fields)
		if (fc.digits < 1 || fc.digits > 4)
			fc.digits = 2;
	return cfg;
}

bool saveConfig(const Config &cfg)
{
	const std::string path = configFilePath();
	if (path.empty())
		return false;

	// Ordner anlegen (obs_module_config_path liefert den Pfad zur Datei).
	char *dir = obs_module_config_path("");
	if (dir) {
		os_mkdirs(dir);
		bfree(dir);
	}

	obs_data_t *d = obs_data_create();
	obs_data_set_string(d, "camera_kind", cfg.cameraKind.c_str());
	obs_data_set_string(d, "camera_id", cfg.cameraId.c_str());
	obs_data_set_string(d, "resolution", cfg.resolution.c_str());
	obs_data_set_string(d, "home_name", cfg.homeName.c_str());
	obs_data_set_string(d, "away_name", cfg.awayName.c_str());
	obs_data_set_int(d, "home_color", cfg.homeColor);
	obs_data_set_int(d, "away_color", cfg.awayColor);
	obs_data_set_int(d, "half_minutes", cfg.halfMinutes);
	obs_data_set_int(d, "sensitivity", cfg.sensitivity);
	obs_data_set_int(d, "slant", cfg.slantPercent);
	obs_data_set_bool(d, "detection_enabled", cfg.detectionEnabled);
	obs_data_set_bool(d, "score_automatic", cfg.scoreAutomatic);

	obs_data_t *fields = obs_data_create();
	for (int i = 0; i < FieldCount; ++i) {
		obs_data_t *f = obs_data_create();
		obs_data_set_double(f, "x", cfg.fields[i].rect.x);
		obs_data_set_double(f, "y", cfg.fields[i].rect.y);
		obs_data_set_double(f, "w", cfg.fields[i].rect.w);
		obs_data_set_double(f, "h", cfg.fields[i].rect.h);
		obs_data_set_int(f, "digits", cfg.fields[i].digits);
		obs_data_set_obj(fields, fieldKey(i), f);
		obs_data_release(f);
	}
	obs_data_set_obj(d, "fields", fields);
	obs_data_release(fields);

	const bool ok = obs_data_save_json_safe(d, path.c_str(), "tmp", "bak");
	obs_data_release(d);
	return ok;
}

} // namespace hbp
