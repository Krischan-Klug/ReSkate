#pragma once
#include "Engine/Game/World/park_rotation.h"
#include "Engine/Game/World/park_editor.h"
#include "Engine/Game/World/world_layers.h"
#include "Engine/Game/World/world_controls.h"
#include "Engine/Game/Rendering/graphics_controls.h"
#include "Engine/Game/Profile/object_persistence_model.h"
#include "Engine/Game/Profile/player_card.h"
#include "Engine/Game/Profile/progression.h"
#include "Engine/Game/Input/controller_bindings.h"
#include "Engine/Game/Settings/named_settings.h"
#include "Engine/Game/Multiplayer/session_model.h"
#include "Engine/Game/UI/menu_scale.h"
#include "Engine/Game/Skater/first_person_spring.h"
#include "Engine/Game/Skater/skater_body.h"

#include "Engine/Core/Console/console_entry.h"
#include "Engine/Resource/image_region.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace dingosdk::overlay {
struct Level {
    std::string asset;
    std::string display_name;
    std::vector<std::string> start_points;
    std::string manifest_start_point;
    // False only for a Studio manifest fallback which is valid as a detached
    // LM destination, not as the native request's base/root level.
    bool native_registered = true;
    bool can_load = true;
    std::string load_block_reason;
    bool custom = false; // Declared by a mod's level manifest.

    const std::string* automatic_start_point() const noexcept {
        if (!manifest_start_point.empty()) return &manifest_start_point;
        return !native_registered && start_points.size() == 1 ? &start_points.front() : nullptr;
    }
};

enum class DebugAction {
    set_free_camera,
    set_game_ui_hidden,
    set_camera_speed,
    restore_debug,
    set_noclip,
    add_forward_velocity,
    set_forward_velocity_speed,
    set_no_bail,
    set_park_editor,
    add_up_velocity,
    set_up_velocity_speed,
    set_first_person,
    set_first_person_fov,
    set_first_person_spring,
    set_first_person_offset_x,
    set_first_person_offset_y,
    set_first_person_offset_z,
    set_first_person_pitch,
    set_first_person_yaw,
    set_first_person_roll,
    set_first_person_spring_up,
    set_first_person_spring_down,
    set_first_person_spring_left,
    set_first_person_spring_right,
    reset_first_person_arm,
    set_free_camera_fov,  // 0 = the game's own FOV
    // Keep the last action in sync with the bound in request_scheduler.h.
};

struct DebugRequest {
    DebugAction action = DebugAction::restore_debug;
    bool enabled = false;
    float value = 0.0f;
};

struct FlightInput {
    bool active = false;
    bool boost = false;
    float right = 0, up = 0, forward = 0;
    float look_x = 0, look_y = 0;
};

struct DebugModel {
    bool available = false;
    bool camera_available = false;
    bool ui_available = false;
    bool free_camera = false;
    bool first_person = false;
    // Vertical degrees applied to the first-person view; 0 keeps the camera's own.
    float first_person_fov = 0;
    float free_camera_fov = 0;  // 0 = the game's own FOV
    first_person::Settings first_person_arm;
    bool park_editor = false;
    bool camera_transform_valid = false;
    std::array<float, 16> camera_transform{};
    float camera_fov = 55;
    bool noclip = false, noclip_available = false;
    bool forward_velocity_available = false;
    bool up_velocity_available = false;
    std::string camera_unavailable = "Waiting for local controls.";
    std::string noclip_unavailable = "Waiting for local controls.";
    std::string forward_velocity_unavailable = "Waiting for local controls.";
    std::string up_velocity_unavailable = "Waiting for local controls.";
    bool no_bail = false, no_bail_available = false, no_bail_active = false;
    std::uint64_t noclip_velocity_updates = 0, noclip_motion_updates = 0;
    std::uint64_t forward_velocity_updates = 0;
    std::uint64_t up_velocity_updates = 0;
    bool game_ui_hidden = false;
    bool settings_owned = false;
    // ReSkate free-flight speed in world units per second.
    float camera_speed = 15.0f;
    float forward_velocity_speed = 20.0f;
    float up_velocity_speed = 20.0f;
    bool camera_position_valid = false;
    bool skater_position_valid = false;
    std::array<float, 3> camera_position{};
    std::array<float, 3> skater_position{};
    std::uintptr_t skater_identity = 0;
    std::string status;
};

enum class OfflineFeatureGroup {
    activities,
    fast_travel,
    progression,
    developer_menus,
    board_wear,
    player_collision,
    restore_all,
};

struct OfflineFeatureRequest {
    OfflineFeatureGroup group = OfflineFeatureGroup::restore_all;
    bool enabled = false;
};

struct OfflineFeatureGroupModel {
    bool available = false;
    bool effective = false;
    bool override_active = false;
};

struct EngineVariableModel {
    std::string name;
    bool available = false;
    bool value = false;
    bool override_active = false;
};

struct OfflineFeatureModel {
    bool available = false;
    bool settings_owned = false;
    OfflineFeatureGroupModel activities;
    OfflineFeatureGroupModel fast_travel;
    OfflineFeatureGroupModel progression;
    OfflineFeatureGroupModel developer_menus;
    OfflineFeatureGroupModel board_wear;
    OfflineFeatureGroupModel player_collision;
    bool skater_slots_available = false;
    bool skater_slots_override_active = false;
    bool skater_slot_manager_available = false;
    bool skater_slot_selectors_enabled = false;
    std::uint32_t skater_slot_count = 0;
    std::uint32_t skater_slot_target = 10;
    std::string skater_slot_status;
    bool main_missions_available = false;
    bool main_missions_override_active = false;
    bool main_missions_authored_offline_route = false;
    std::uint32_t main_mission_claim_leases = 0;
    std::uint32_t main_mission_target = 20;
    std::uint32_t progression_unlock_claim_leases = 0;
    std::uint32_t progression_unlock_target = 2;
    std::uint32_t onboarding_dependency_claim_leases = 0;
    std::uint32_t onboarding_dependency_target = 2;
    std::uint32_t mission_pending_restore = 0;
    std::string main_mission_status;
    std::string status;
    // Exact-build, reflection-validated boolean settings. The console exposes
    // only this allowlist; arbitrary addresses and unvalidated fields are never
    // accepted as engine variables.
    std::vector<EngineVariableModel> variables;
};

using ConsoleLogLine = dingosdk::ConsoleLogLine;

struct MissionRow {
    std::string id, group;
    int completed = -1;
};

struct Model {
    std::string state = "Waiting for native state";
    std::string detail;
    std::vector<Level> levels;
    bool can_queue_load = false;
    std::string load_block_reason = "Native load handler is not connected.";
    DebugModel debug;
    OfflineFeatureModel offline;
    std::vector<NamedSettingModel> engine_settings;
    std::vector<ConsoleLogLine> console_log;
    bool missions_available = false;
    std::vector<MissionRow> missions;
    std::string mission_feedback;
    ParksModel parks;
    WorldLayersModel world;
    WorldControlsModel world_controls;
    ProgressionModel progression;
    PlayerCardModel player_card;
    ObjectPersistenceModel object_persistence;
    ParkEditorModel editor;
    float menu_scale = default_menu_scale;
    // Offline mode: no Steam, so every multiplayer page and control is hidden.
    bool steam_offline = false;
    ControllerBindingsModel bindings;
    GraphicsControlsModel graphics;
    MultiplayerModel multiplayer;
    // Host bookkeeping: the revision each part of this copy was taken at (zero:
    // never). A reader that hands its previous copy back to read_model has only
    // the parts that changed since copied; the rest are left as they are.
    struct Revisions {
        std::uint64_t model{}, debug{}, offline{}, engine_settings{};
    } revisions;
};

// Callbacks run on the presentation thread. They must be bounded and thread
// safe; queue callbacks must enqueue requests, never call engine code here.
struct Callbacks {
    void* user = nullptr;
    void (*read_model)(void* user, Model& output) = nullptr;
    bool (*queue_load)(void* user, const char* asset, const char* start_point,
                       const char* lm_level, const char* lm_start_point,
                       char* result, std::size_t result_size) = nullptr;
    bool (*queue_debug)(void* user, const DebugRequest& request,
                        char* result, std::size_t result_size) = nullptr;
};

// Extended callback table for hosts that expose process-local offline controls.
// Keep Callbacks and DingoSDKOverlayStart unchanged for binary compatibility with
// hosts built against the original four-field table.
struct CallbacksV2 {
    void* user = nullptr;
    void (*read_model)(void* user, Model& output) = nullptr;
    bool (*queue_load)(void* user, const char* asset, const char* start_point,
                       const char* lm_level, const char* lm_start_point,
                       char* result, std::size_t result_size) = nullptr;
    bool (*queue_debug)(void* user, const DebugRequest& request,
                        char* result, std::size_t result_size) = nullptr;
    bool (*queue_offline_feature)(void* user, const OfflineFeatureRequest& request,
                                 char* result, std::size_t result_size) = nullptr;
};

// Extended callback table for the in-game command console. Keep V1 and V2
// unchanged so previously built hosts remain binary compatible.
// Internal private action queue, separate from the stable V3 callback ABI and console history.
using MultiplayerQueue = bool (*)(const char *action, const char *argument, const char *password,
                                 char *result, std::size_t size);
void set_multiplayer_queue(MultiplayerQueue) noexcept;
// The session's text chat, read by the chat panel every frame (thread-safe, cheap).
using ChatFeed = MultiplayerChat (*)();
void set_chat_feed(ChatFeed) noexcept;
// Text the game draws with its debug text natives, where retail draws nothing (the
// S.K.A.T.E. throwdown HUD). One frame of lines, positioned in a width x height screen;
// color is R, G, B, A bytes (ImGui's IM_COL32 layout). Read every presented frame
// (thread-safe, cheap; empty = nothing).
struct GameTextLine {
    float x{}, y{}, scale{1};
    std::uint32_t color{0xffffffffU};
    bool centered{}; // x is the middle of the line
    std::string text;
};
struct GameText {
    float width{}, height{};
    std::vector<GameTextLine> lines;
};
using GameTextFeed = GameText (*)();
void set_game_text_feed(GameTextFeed) noexcept;
// The S.K.A.T.E. throwdown's HUD as ReSkate draws it (skate_hud_overlay.cpp), read from what
// the game's own debug HUD lays out each frame: the messages at the top (what to do, the
// trick to copy, how an attempt went) and every player's letters. Read every presented
// frame; no messages and no players = nothing to draw.
struct SkateHudMessage {
    enum class Kind : std::uint8_t { prompt, trick, success, failure } kind{};
    std::string text;
};
struct SkateHudPlayer {
    std::string name; // empty when the game has none
    int letters{};    // 0-5 of S.K.A.T.E.
    bool up{};        // whose turn it is
    bool out{};       // eliminated
};
struct SkateHud {
    std::vector<SkateHudMessage> messages; // top to bottom
    std::vector<SkateHudPlayer> players;   // in the game's order
    std::vector<std::string> done_tricks;  // set so far this game, oldest first (none repeat)
};
using SkateHudFeed = SkateHud (*)();
void set_skate_hud_feed(SkateHudFeed) noexcept;
// ReSkate's own nametags: one per other player, placed over the world with the camera the
// client last used. Close ones show a name and distance, far ones a dot, and players off
// screen a dot at the screen's edge. Empty = nothing to draw (off, or the game hides its UI).
// One line in a player's bubble stack: the text (masked when the chat filter is on), the line
// as sent when the filter changed it (same length; emote names are taken back from it), how
// far its pop-in has come (0 just arrived, 1 settled) and how opaque it still is (1 down to 0).
struct NametagBubble {
    std::string text, raw;
    float appear{1}, fade{1};
};
struct Nametag {
    std::array<float, 3> position{}; // above the skater's head, world space
    std::string name;
    std::uint32_t color{0xffffffffU}; // R, G, B, A bytes (IM_COL32)
    std::string tag;                  // role badge before the name ("Dev", "Staff", "Creator", "Centrix", "Homie", "Admin", "Host", "Friend")
    float distance{};                 // metres from the local skater
    bool talking{};
    // Recent chat lines to show as bubbles above the head, oldest first ("" = none).
    std::vector<NametagBubble> bubbles;
    bool self{};                      // the local player: bubbles only, never a name or dot
};
struct Nametags {
    std::array<float, 16> camera{}; // world matrix: right, up, back, position rows
    float vertical_fov{};
    bool show_names{true};          // draw the name, distance and role badge
    bool show_bubbles{};            // draw chat bubbles above the heads
    float bubble_distance{40.f};    // furthest a player may be and still show a bubble (metres)
    std::vector<Nametag> tags;
};
using NametagFeed = Nametags (*)();
void set_nametag_feed(NametagFeed) noexcept;
// skate.'s own skeleton mesh posed for one frame (Engine/Game/Skater/skater_skeleton.h), in world
// space with the camera to see it from: any feature's to show (skeleton_overlay.cpp).
struct SkeletonFrame {
    std::array<float, 16> camera{}; // world matrix: right, up, back, position rows
    float vertical_fov{};
    std::vector<std::array<float, 3>> positions, normals;        // world space, one per vertex
    std::shared_ptr<const std::vector<std::uint32_t>> triangles; // vertex indices, three a triangle
    std::shared_ptr<const std::vector<std::uint8_t>> parts;      // each vertex's body (skater_body.h)
};
// Images out of the installed game's data (Engine/Vfs/game_textures.h), for any feature to draw
// in the overlay by key (overlay_images.cpp). Read in the background as they are added; drawn
// once the overlay's next atlas holds them. A key already added keeps its first image.
enum class GameImageColours : std::uint8_t {
    original,
    silhouette, // white with the texture's own alpha, to be drawn in any colour
    brightness, // white with the texture's brightness as alpha: light marks on black (scratches), in any colour
};
struct GameImage {
    std::string key;
    std::string toc, bundle, name; // the texture: superbundle TOC, bundle, resource name
    std::uint32_t side{};          // the texture fitted into a side x side square, its aspect kept
    GameImageColours colours{};
    frostbite::ImageRegion region{}; // the part of the texture kept: the picture
    // A shape's: the part of the picture a box is laid on, the rest drawn around it (a brush
    // stroke's bar, its splatter beyond), and how much of each edge a panel keeps unstretched
    // (nine-slice), both as fractions of the picture's width and height.
    frostbite::ImageRegion body{};
    float slice{};
};
void add_game_images(std::vector<GameImage> images);
// A result card for any feature (score_card_overlay.cpp), laid out like skate. 3's Hall of Meat
// in skate.'s own colours: a row per stat, then the logo, the title and the total, in the top
// right corner. Icons, the logo and the skin are game images' keys (add_game_images), white and
// tinted as they are drawn. A row new on the card fades in, so a card can grow as its stats come.
struct ScoreCardRow {
    std::string key;           // the same row from frame to frame
    std::string icon;          // empty for none
    std::string value;         // what was measured, as shown: "6.2 s"
    std::optional<int> points; // what it scored; none for a stat that scores nothing
};
// The shapes the card is drawn with; one empty or not loaded draws a plain tile instead, or nothing.
struct ScoreCardSkin {
    std::string row;       // a stat's bar, laid under its value and points (its body on them)
    std::string icon;      // the block a stat's icon sits on
    std::string panel;     // the logo's, the title's and the total's, nine-sliced
    std::string scratches; // over the panel
    std::string underline; // under the total
};
struct ScoreCard {
    float opacity{}; // 0: no card
    ScoreCardSkin skin;
    std::vector<ScoreCardRow> rows;
    std::string logo;
    std::string title;
    int total{};       // counts up to its value as it changes
    std::string badge; // beside the title, e.g. "NEW BEST" or "BEST 12,345"; empty for none
    bool highlight{};  // the badge marks a record
};
// Hall of Meat: from the local skater's bail until they get up, the bones it hurt over the world,
// each coloured by how hard it was hit, and the bail's score card. Empty parts draw nothing.
enum class MeatInjury : std::uint8_t { none, hit, broken };
struct MeatSkeleton {
    SkeletonFrame frame;
    float alpha{}; // fades the whole skeleton out
    std::array<MeatInjury, skater_body::count> injuries{}; // each body's
    std::array<float, skater_body::count> flashes{};       // 1 the moment it is hit, falling to 0
};
struct MeatFrame {
    MeatSkeleton skeleton;
    ScoreCard card;
};
struct HallOfMeatHooks {
    MeatFrame (*frame)() = nullptr; // every presented frame
    bool (*enabled)() = nullptr;    // the SKATER menu's switch shows this
};
void set_hall_of_meat_hooks(HallOfMeatHooks) noexcept;
// The switch as the hooks report it: available once the game side handed them over.
bool hall_of_meat_available() noexcept;
bool hall_of_meat_enabled() noexcept;
// The debug panel (Extension/Debug/debug_panel.h): one source's live values in the bottom
// right corner. A field flashes when its value changes.
struct DebugField {
    std::string label, value;
    float changed{}; // 1 the moment it changed, falling to 0
    bool heading{};  // a section title: label only
};
struct DebugPanel {
    std::string title; // the shown source's
    std::vector<DebugField> fields;
};
struct DebugSource {
    std::string id, title; // `debugpanel <id>` shows it
};
struct DebugPanelHooks {
    DebugPanel (*panel)() = nullptr;                 // every presented frame; no fields while hidden
    std::vector<DebugSource> (*sources)() = nullptr; // SETTINGS > INTERFACE offers these
    std::string (*selected)() = nullptr;             // the shown source's id, empty while hidden
};
void set_debug_panel_hooks(DebugPanelHooks) noexcept;
std::vector<DebugSource> debug_panel_sources();
std::string debug_panel_selected();
using ParkSurfaceQueue = bool (*)(const EditorSurfaceRequest &);
void set_park_surface_queue(ParkSurfaceQueue) noexcept;
using ParkPreviewQueue = bool (*)(const EditorPreviewRequest &);
void set_park_preview_queue(ParkPreviewQueue) noexcept;
using ParkSelectionQueue = bool (*)(const EditorSelectionRequest &);
void set_park_selection_queue(ParkSelectionQueue) noexcept;
using ParkPasteQueue = bool (*)(const EditorPasteRequest &);
void set_park_paste_queue(ParkPasteQueue) noexcept;
struct CallbacksV3 {
    void* user = nullptr;
    void (*read_model)(void* user, Model& output) = nullptr;
    bool (*queue_load)(void* user, const char* asset, const char* start_point,
                       const char* lm_level, const char* lm_start_point,
                       char* result, std::size_t result_size) = nullptr;
    bool (*queue_debug)(void* user, const DebugRequest& request,
                        char* result, std::size_t result_size) = nullptr;
    bool (*queue_offline_feature)(void* user, const OfflineFeatureRequest& request,
                                 char* result, std::size_t result_size) = nullptr;
    bool (*queue_console_command)(void* user, const char* command,
                                  char* result, std::size_t result_size) = nullptr;
};

struct Status {
    bool started = false;
    bool bound = false;
    bool ready = false;
    bool visible = false;
    bool stopping = false;
    bool failed = false;
    std::uint64_t rendered_frames = 0;
};
bool keyboard_shortcuts_allowed() noexcept;

// A short message in the top-left corner of the game window: a title line and
// optional detail, stacked under earlier ones and fading out on their own
// (info a few seconds, warnings and errors longer). Safe from any thread and
// before the overlay has started: queued notices show once the first frame
// draws. They take no input and never open the menu.
enum class NoticeLevel { info, warning, error };
void notify(NoticeLevel level, std::string title, std::string text = {}) noexcept;
}

// Call outside DllMain, before the game's first DXGI factory is created. No
// remote-process access and no image-specific patching happen in this component.
extern "C" __declspec(dllexport) bool DingoSDKOverlayStart(
    const dingosdk::overlay::Callbacks* callbacks);
extern "C" __declspec(dllexport) bool DingoSDKOverlayStartV2(
    const dingosdk::overlay::CallbacksV2* callbacks);
extern "C" __declspec(dllexport) bool DingoSDKOverlayStartV3(
    const dingosdk::overlay::CallbacksV3* callbacks);

// Nonblocking. A later Present releases GPU resources after its fence completes.
// The DLL stays pinned until process exit so no thread can return into unloaded
// detours. Input is released immediately, even if rendering has stopped.
extern "C" __declspec(dllexport) void DingoSDKOverlayRequestStop();
extern "C" __declspec(dllexport) void DingoSDKOverlayGetStatus(dingosdk::overlay::Status* status);
// Poll only on the client update thread. Returns no input while either overlay
// surface is open, the game is unfocused, or the overlay has stopped.
extern "C" void DingoSDKOverlayReadFlightInput(dingosdk::overlay::FlightInput* input, bool flight_active, bool player_flight = false);
// allow_menu is used only by the Binds page while recording. Focus and lifetime
// gates still apply. Gameplay callers leave it false.
extern "C" void DingoSDKOverlayReadControllerInput(dingosdk::ControllerInput* input, bool allow_menu = false);

// Optional direct integration for a host that already knows its exact pair.
struct IDXGISwapChain;
struct ID3D12CommandQueue;
extern "C" __declspec(dllexport) bool DingoSDKOverlayBindDx12(
    IDXGISwapChain* swapchain, ID3D12CommandQueue* command_queue);
