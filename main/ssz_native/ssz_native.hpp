// ssz_native.hpp — Umbrella header for all native SSZ compatibility services.
//
// Including this single header gives access to every native service
// that replaces a corresponding ssz_script/ module.

#pragma once

#include "ssz_value.hpp"
#include "consts.hpp"

// Phase 1 — Foundation libraries
#include "consts.hpp"
#include "math_service.hpp"
#include "string_service.hpp"
#include "alert_service.hpp"
#include "thread_service.hpp"
#include "time_service.hpp"
#include "table_service.hpp"
#include "crypto_service.hpp"

// Phase 2 — Plugin wrappers
#include "file_service.hpp"
#include "regex_service.hpp"
#include "socket_service.hpp"
#include "sound_service.hpp"
#include "ogg_service.hpp"
#include "shell_service.hpp"
#include "lua_service.hpp"
#include "mesdialog_service.hpp"
#include "sdlevent_service.hpp"
#include "sdlplugin_service.hpp"

// Phase 3 — Core engine modules
#include "share_service.hpp"
#include "system_service.hpp"
#include "debug_script_service.hpp"
#include "loader_service.hpp"
#include "common_service.hpp"
#include "trigger_script_service.hpp"
#include "script_service.hpp"
#include "system_script_service.hpp"
#include "statebuilder_service.hpp"

// Phase 4 — Gameplay resource modules
#include "action_service.hpp"
#include "bg_service.hpp"
#include "char_service.hpp"
#include "command_service.hpp"
#include "fight_service.hpp"
#include "fighting_service.hpp"
#include "font_service.hpp"
#include "sff_service.hpp"
#include "sound_resource_service.hpp"
#include "stage_service.hpp"
#include "video_service.hpp"

// Phase 5 — Config
#include "config_service.hpp"
#include "config_net_service.hpp"

// Additional utilities
#include "stack_service.hpp"
#include "ssz_trace.hpp"
#include "plugin_native_api.hpp"
