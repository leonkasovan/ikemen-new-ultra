#include "config_service.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace ikemen::ssz_native {

namespace {

void write_int(std::ofstream& out, const char* key, int val) {
	out << key << "=" << val << "\n";
}

void write_bool(std::ofstream& out, const char* key, bool val) {
	out << key << "=" << (val ? "true" : "false") << "\n";
}

void write_float(std::ofstream& out, const char* key, float val) {
	out << key << "=" << val << "\n";
}

void write_str(std::ofstream& out, const char* key, const std::string& val) {
	out << key << "=" << val << "\n";
}

void write_keys(std::ofstream& out, const char* prefix, const KeyBindings& k) {
	out << prefix << ".jn=" << k.jn << "\n";
	out << prefix << ".u=" << k.u << "\n";
	out << prefix << ".d=" << k.d << "\n";
	out << prefix << ".l=" << k.l << "\n";
	out << prefix << ".r=" << k.r << "\n";
	out << prefix << ".a=" << k.a << "\n";
	out << prefix << ".b=" << k.b << "\n";
	out << prefix << ".c=" << k.c << "\n";
	out << prefix << ".x=" << k.x << "\n";
	out << prefix << ".y=" << k.y << "\n";
	out << prefix << ".z=" << k.z << "\n";
	out << prefix << ".q=" << k.q << "\n";
	out << prefix << ".w=" << k.w << "\n";
	out << prefix << ".e=" << k.e << "\n";
	out << prefix << ".s=" << k.s << "\n";
}

std::string trim(const std::string& s) {
	size_t start = s.find_first_not_of(" \t\r\n");
	if (start == std::string::npos) return "";
	size_t end = s.find_last_not_of(" \t\r\n");
	return s.substr(start, end - start + 1);
}

bool parse_bool(const std::string& v) {
	return v == "true" || v == "1";
}

}

bool config_save(const std::string& path, const ConfigData& cfg) {
	std::ofstream out(path);
	if (!out.is_open()) return false;

	out << "[Video]\n";
	write_int(out, "Renderer", cfg.Renderer);
	write_bool(out, "AspectRatio", cfg.AspectRatio);
	write_bool(out, "FullScreen", cfg.FullScreen);
	write_bool(out, "FullScreenExclusive", cfg.FullScreenExclusive);
	write_int(out, "WindowType", cfg.WindowType);
	write_int(out, "Width", cfg.Width);
	write_int(out, "Height", cfg.Height);
	write_int(out, "Brightness", cfg.Brightness);
	write_float(out, "Opacity", cfg.Opacity);

	out << "\n[Audio]\n";
	write_float(out, "GlVol", cfg.GlVol);
	write_float(out, "SEVol", cfg.SEVol);
	write_float(out, "BGMVol", cfg.BGMVol);
	write_float(out, "PanStr", cfg.PanStr);
	write_int(out, "VideoVol", cfg.VideoVol);

	out << "\n[Performance]\n";
	write_bool(out, "SaveMemory", cfg.SaveMemory);
	write_int(out, "HelperMax", cfg.HelperMax);
	write_int(out, "PlayerProjectileMax", cfg.PlayerProjectileMax);
	write_int(out, "ExplodMax", cfg.ExplodMax);
	write_int(out, "AfterImageMax", cfg.AfterImageMax);

	out << "\n[Game]\n";
	write_int(out, "GameSpeed", cfg.GameSpeed);
	write_float(out, "Attack_LifeToPowerMul", cfg.Attack_LifeToPowerMul);
	write_float(out, "GetHit_LifeToPowerMul", cfg.GetHit_LifeToPowerMul);
	write_float(out, "Super_TargetDefenceMul", cfg.Super_TargetDefenceMul);
	write_float(out, "LifebarFontScale", cfg.LifebarFontScale);

	out << "\n[Portraits]\n";
	write_int(out, "CharPortraitsGroup", cfg.CharPortraitsGroup);
	write_int(out, "CharFacePortraitIndex", cfg.CharFacePortraitIndex);
	write_int(out, "CharBigPortraitIndex", cfg.CharBigPortraitIndex);
	write_int(out, "CharWinnerPortraitIndex", cfg.CharWinnerPortraitIndex);
	write_int(out, "CharLoserPortraitIndex", cfg.CharLoserPortraitIndex);
	write_int(out, "CharOrderPortraitIndex", cfg.CharOrderPortraitIndex);
	write_int(out, "CharVSPortraitIndex", cfg.CharVSPortraitIndex);
	write_int(out, "CharResultsPortraitIndex", cfg.CharResultsPortraitIndex);
	write_int(out, "CharExtraPortraitIndex", cfg.CharExtraPortraitIndex);
	write_int(out, "StagePortraitsGroup", cfg.StagePortraitsGroup);
	write_int(out, "StageIconPortraitIndex", cfg.StageIconPortraitIndex);
	write_int(out, "StageBigPortraitIndex", cfg.StageBigPortraitIndex);
	write_int(out, "StageVSPortraitIndex", cfg.StageVSPortraitIndex);
	write_int(out, "StageWinPortraitIndex", cfg.StageWinPortraitIndex);
	write_int(out, "StageExtraPortraitIndex", cfg.StageExtraPortraitIndex);

	out << "\n[System]\n";
	write_str(out, "Executable", cfg.Executable);
	write_str(out, "WindowTitle", cfg.WindowTitle);
	write_str(out, "ScreenshotFolder", cfg.ScreenshotFolder);
	write_str(out, "listenPort", cfg.listenPort);
	write_str(out, "UserName", cfg.UserName);
	write_str(out, "GlobalAnims", cfg.GlobalAnims);
	write_str(out, "GlobalCommands", cfg.GlobalCommands);
	write_str(out, "GlobalMatch", cfg.GlobalMatch);
	write_str(out, "system", cfg.system);
	write_str(out, "GamepadMappings", cfg.GamepadMappings);

	out << "\n[Input]\n";
	write_bool(out, "IgnoreMostErrors", cfg.IgnoreMostErrors);
	for (int i = 0; i < 14; i++) {
		std::string prefix = "in" + std::to_string(i);
		write_keys(out, prefix.c_str(), cfg.Input[i]);
	}

	return out.good();
}

bool config_load(const std::string& path, ConfigData& cfg) {
	std::ifstream in(path);
	if (!in.is_open()) return false;

	cfg = make_default_config();

	std::string line;
	while (std::getline(in, line)) {
		line = trim(line);
		if (line.empty() || line[0] == '[' || line[0] == '#' || line[0] == ';')
			continue;

		auto eq = line.find('=');
		if (eq == std::string::npos) continue;

		std::string key = trim(line.substr(0, eq));
		std::string val = trim(line.substr(eq + 1));

		if (key == "Renderer") cfg.Renderer = std::atoi(val.c_str());
		else if (key == "AspectRatio") cfg.AspectRatio = parse_bool(val);
		else if (key == "FullScreen") cfg.FullScreen = parse_bool(val);
		else if (key == "FullScreenExclusive") cfg.FullScreenExclusive = parse_bool(val);
		else if (key == "WindowType") cfg.WindowType = std::atoi(val.c_str());
		else if (key == "Width") cfg.Width = std::atoi(val.c_str());
		else if (key == "Height") cfg.Height = std::atoi(val.c_str());
		else if (key == "Brightness") cfg.Brightness = std::atoi(val.c_str());
		else if (key == "Opacity") cfg.Opacity = static_cast<float>(std::atof(val.c_str()));
		else if (key == "GlVol") cfg.GlVol = static_cast<float>(std::atof(val.c_str()));
		else if (key == "SEVol") cfg.SEVol = static_cast<float>(std::atof(val.c_str()));
		else if (key == "BGMVol") cfg.BGMVol = static_cast<float>(std::atof(val.c_str()));
		else if (key == "PanStr") cfg.PanStr = static_cast<float>(std::atof(val.c_str()));
		else if (key == "VideoVol") cfg.VideoVol = std::atoi(val.c_str());
		else if (key == "SaveMemory") cfg.SaveMemory = parse_bool(val);
		else if (key == "HelperMax") cfg.HelperMax = std::atoi(val.c_str());
		else if (key == "PlayerProjectileMax") cfg.PlayerProjectileMax = std::atoi(val.c_str());
		else if (key == "ExplodMax") cfg.ExplodMax = std::atoi(val.c_str());
		else if (key == "AfterImageMax") cfg.AfterImageMax = std::atoi(val.c_str());
		else if (key == "GameSpeed") cfg.GameSpeed = std::atoi(val.c_str());
		else if (key == "Attack_LifeToPowerMul") cfg.Attack_LifeToPowerMul = static_cast<float>(std::atof(val.c_str()));
		else if (key == "GetHit_LifeToPowerMul") cfg.GetHit_LifeToPowerMul = static_cast<float>(std::atof(val.c_str()));
		else if (key == "Super_TargetDefenceMul") cfg.Super_TargetDefenceMul = static_cast<float>(std::atof(val.c_str()));
		else if (key == "LifebarFontScale") cfg.LifebarFontScale = static_cast<float>(std::atof(val.c_str()));
		else if (key == "CharPortraitsGroup") cfg.CharPortraitsGroup = std::atoi(val.c_str());
		else if (key == "CharFacePortraitIndex") cfg.CharFacePortraitIndex = std::atoi(val.c_str());
		else if (key == "CharBigPortraitIndex") cfg.CharBigPortraitIndex = std::atoi(val.c_str());
		else if (key == "CharWinnerPortraitIndex") cfg.CharWinnerPortraitIndex = std::atoi(val.c_str());
		else if (key == "CharLoserPortraitIndex") cfg.CharLoserPortraitIndex = std::atoi(val.c_str());
		else if (key == "CharOrderPortraitIndex") cfg.CharOrderPortraitIndex = std::atoi(val.c_str());
		else if (key == "CharVSPortraitIndex") cfg.CharVSPortraitIndex = std::atoi(val.c_str());
		else if (key == "CharResultsPortraitIndex") cfg.CharResultsPortraitIndex = std::atoi(val.c_str());
		else if (key == "CharExtraPortraitIndex") cfg.CharExtraPortraitIndex = std::atoi(val.c_str());
		else if (key == "StagePortraitsGroup") cfg.StagePortraitsGroup = std::atoi(val.c_str());
		else if (key == "StageIconPortraitIndex") cfg.StageIconPortraitIndex = std::atoi(val.c_str());
		else if (key == "StageBigPortraitIndex") cfg.StageBigPortraitIndex = std::atoi(val.c_str());
		else if (key == "StageVSPortraitIndex") cfg.StageVSPortraitIndex = std::atoi(val.c_str());
		else if (key == "StageWinPortraitIndex") cfg.StageWinPortraitIndex = std::atoi(val.c_str());
		else if (key == "StageExtraPortraitIndex") cfg.StageExtraPortraitIndex = std::atoi(val.c_str());
		else if (key == "Executable") cfg.Executable = val;
		else if (key == "WindowTitle") cfg.WindowTitle = val;
		else if (key == "ScreenshotFolder") cfg.ScreenshotFolder = val;
		else if (key == "listenPort") cfg.listenPort = val;
		else if (key == "UserName") cfg.UserName = val;
		else if (key == "GlobalAnims") cfg.GlobalAnims = val;
		else if (key == "GlobalCommands") cfg.GlobalCommands = val;
		else if (key == "GlobalMatch") cfg.GlobalMatch = val;
		else if (key == "system") cfg.system = val;
		else if (key == "GamepadMappings") cfg.GamepadMappings = val;
		else if (key == "IgnoreMostErrors") cfg.IgnoreMostErrors = parse_bool(val);
		else {
			for (int i = 0; i < 14; i++) {
				std::string pfx = "in" + std::to_string(i) + ".";
				if (key.substr(0, pfx.size()) != pfx) continue;
				std::string field = key.substr(pfx.size());
				int v = std::atoi(val.c_str());
				if (field == "jn") cfg.Input[i].jn = v;
				else if (field == "u") cfg.Input[i].u = v;
				else if (field == "d") cfg.Input[i].d = v;
				else if (field == "l") cfg.Input[i].l = v;
				else if (field == "r") cfg.Input[i].r = v;
				else if (field == "a") cfg.Input[i].a = v;
				else if (field == "b") cfg.Input[i].b = v;
				else if (field == "c") cfg.Input[i].c = v;
				else if (field == "x") cfg.Input[i].x = v;
				else if (field == "y") cfg.Input[i].y = v;
				else if (field == "z") cfg.Input[i].z = v;
				else if (field == "q") cfg.Input[i].q = v;
				else if (field == "w") cfg.Input[i].w = v;
				else if (field == "e") cfg.Input[i].e = v;
				else if (field == "s") cfg.Input[i].s = v;
				break;
			}
		}
	}

	return true;
}

} // namespace ikemen::ssz_native
