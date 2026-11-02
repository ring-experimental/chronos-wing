// 터치 패드와 설정 화면. 패드는 손가락마다 따로 판정해 방향키와 공격을 동시에 누를 수 있고, 설정 화면은 RmlUi 이벤트를 쓴다.
#include "ui.h"
#include "ui_render.h"
#include "gamepad.h"
#include <RmlUi/Core.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern "C" void host_key(int type, int code);
extern "C" bool game_in_dialog(void);
extern "C" bool devmode_hud_top(void);
extern "C" bool game_shortcuts_bought(void);
extern "C" bool dialog_auto(void);
extern "C" void dialog_set_auto(bool on);
extern "C" bool dialog_log_is_open(void);
extern "C" void dialog_log_toggle(void);
extern "C" void dialog_log_scroll(int rows);

namespace {

constexpr float UNPLACED = -1.f; // 옮기기 전의 스킬 슬롯. L 버튼 왼쪽에 줄지어 놓인다.

// 패드 묶음. 위치는 화면 비율, 크기는 em이다. 문서는 g[번호]로 묶으므로 ui/pad.rml과 순서를 맞춘다.
struct Group {
    const char *id;
    float def_x, def_y, w_em, h_em;
    float x, y, scale;
    float center_x, center_y; // 지금 화면에 놓인 중심(비율).
    Rml::String left, top, font;
};
Group groups[] = {
    {"dpad", 0.14f, 0.73f, 3.f, 3.f},
    {"ok", 0.91f, 0.80f, 1.6f, 1.6f},
    {"skill", 0.80f, 0.74f, 1.55f, 2.75f},
    {"aux", 0.95f, 0.06f, 2.65f, 0.8f},
    {"sys", 0.04f, 0.06f, 0.8f, 0.8f},
    {"skip", 0.91f, 0.60f, 1.6f, 0.8f},
    {"item", 0.91f, 0.60f, 1.f, 1.f},
    {"menu", 0.91f, 0.48f, 0.8f, 0.8f},
    {"slot1", UNPLACED, UNPLACED, 0.8f, 0.8f},
    {"slot3", UNPLACED, UNPLACED, 0.8f, 0.8f},
    {"slot0", UNPLACED, UNPLACED, 0.8f, 0.8f},
    {"auto", 0.91f, 0.48f, 1.6f, 0.8f},
    {"log", 0.91f, 0.36f, 1.6f, 0.8f},
};
constexpr int SYS_GROUP = 4, AUX_GROUP = 3, FIRST_SLOT = 8, SLOT_COUNT = 3;
constexpr float SLOT_PITCH_EM = 0.85f;
constexpr float BASE_DP = 34.f;
constexpr float LOG_ROW_RATIO = 14.f / 320.f; // 대사 기록 한 줄의 높이. 끌어서 넘길 때 쓴다.
constexpr float HUD_TOP_RATIO = 35.f / 320.f; // 상단 상태창 높이. 왼쪽 위 설정 버튼을 그만큼 내린다.
constexpr Uint64 CAPTURE_TIMEOUT_MS = 5000; // 맵핑 창이 버튼을 기다리는 시간.
bool hud_top = false;

// 데이터 모델 "ui"에 묶이는 값. 화면 표시는 문서의 data-* 바인딩이 결정한다.
struct Model {
    bool pad_on = false, dialog = false, edit = false, shortcuts = false, auto_on = false, log = false;
    int size = 100, alpha = 85, selected = -1;
    bool gamepad = false;
    int capture = -1; // 다음 버튼을 기다리는 맵핑 줄.
} model;

// 맵핑 창의 한 줄.
struct Bind { Rml::String name, key; };
std::vector<Bind> binds;
int pad_generation = -1;
Uint64 capture_until;

SDL_Window *window;
SDL_Renderer *renderer;
UiRender *render_if;
Rml::Context *ctx;
Rml::ElementDocument *pad, *settings, *editbar, *binds_doc;
Rml::ElementDocument *focus_pending; // 문서를 연 다음 Update 뒤에 첫 항목으로 포커스를 옮긴다. 목록 항목이 그때 생긴다.
Rml::DataModelHandle handle;
std::string cfg_path;
int out_w, out_h;

struct Finger { SDL_FingerID id; bool used; Rml::Element *button; int group; float grab_x, grab_y; };
Finger fingers[10];

void save_config() {
    FILE *f = fopen(cfg_path.c_str(), "w");
    if (!f) return;
    fprintf(f, "pad %d\nsize %d\nopacity %d\nautoplay %d\n", model.pad_on, model.size, model.alpha, dialog_auto());
    for (auto &g : groups) fprintf(f, "%s %.4f %.4f %.3f\n", g.id, g.x, g.y, g.scale);
    for (int i = 0; i < gamepad_action_count(); i++) fprintf(f, "bind %s %s\n", gamepad_config_id(i), gamepad_config_value(i));
    fclose(f);
}

void reset_groups() {
    for (auto &g : groups) { g.x = g.def_x; g.y = g.def_y; g.scale = 1.f; }
}

void load_config(bool default_pad) {
    reset_groups();
    model.pad_on = default_pad;
    FILE *f = fopen(cfg_path.c_str(), "r");
    if (!f) return;
    char name[32], line[128], value[32];
    float a, b, c;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "bind %31s %31s", name, value) == 2) { gamepad_config_read(name, value); continue; }
        int n = sscanf(line, "%31s %f %f %f", name, &a, &b, &c);
        if (n == 2 && !strcmp(name, "pad")) model.pad_on = a != 0;
        else if (n == 2 && !strcmp(name, "size")) model.size = (int)std::clamp(a, 50.f, 200.f);
        else if (n == 2 && !strcmp(name, "opacity")) model.alpha = (int)std::clamp(a, 10.f, 100.f);
        else if (n == 2 && !strcmp(name, "autoplay")) dialog_set_auto(a != 0);
        else if (n == 4) for (auto &g : groups) if (!strcmp(g.id, name)) { g.x = a < 0 ? UNPLACED : std::clamp(a, 0.f, 1.f); g.y = b < 0 ? UNPLACED : std::clamp(b, 0.f, 1.f); g.scale = std::clamp(c, 0.5f, 2.5f); }
    }
    fclose(f);
}

// 묶음 중심(화면 비율)과 크기로 스타일 값을 만든다. 화면 밖으로 나가지 않게 막는다.
void update_layout() {
    float dp = ctx->GetDensityIndependentPixelRatio();
    float screen_w = out_w / dp, screen_h = out_h / dp;
    float aux_left = 0, aux_top = 0;
    for (auto &g : groups) {
        float font = BASE_DP * model.size / 100.f * g.scale;
        float w = g.w_em * font, h = g.h_em * font;
        g.font = Rml::ToString(font) + "dp";
        float y = g.y + (hud_top && &g == &groups[SYS_GROUP] ? HUD_TOP_RATIO : 0.f);
        float left = g.x * screen_w - w / 2, top = y * screen_h - h / 2;
        int index = (int)(&g - groups);
        if (g.x == UNPLACED) {
            float pitch = SLOT_PITCH_EM * BASE_DP * model.size / 100.f * groups[AUX_GROUP].scale;
            left = aux_left - (SLOT_COUNT - (index - FIRST_SLOT)) * pitch;
            top = aux_top;
        }
        left = std::clamp(left, 0.f, std::max(0.f, screen_w - w));
        top = std::clamp(top, 0.f, std::max(0.f, screen_h - h));
        if (index == AUX_GROUP) { aux_left = left; aux_top = top; }
        g.center_x = (left + w / 2) / screen_w;
        g.center_y = (top + h / 2) / screen_h;
        g.left = Rml::ToString(left) + "dp";
        g.top = Rml::ToString(top) + "dp";
    }
    handle.DirtyVariable("g");
}

void update_sizes() {
    int w, h;
    SDL_GetCurrentRenderOutputSize(renderer, &w, &h);
    if (w == out_w && h == out_h) return;
    out_w = w; out_h = h;
    ctx->SetDimensions({w, h});
    ctx->SetDensityIndependentPixelRatio(h / 320.f);
    update_layout();
}

void show_settings() {
    settings->Show();
    settings->PullToFront();
    focus_pending = settings;
}

void refresh_binds() {
    binds.resize(gamepad_action_count());
    for (int i = 0; i < (int)binds.size(); i++) binds[i] = {gamepad_action_name(i), gamepad_action_key(i)};
    handle.DirtyVariable("binds");
}

void set_capture(int row) {
    model.capture = row;
    capture_until = SDL_GetTicks() + CAPTURE_TIMEOUT_MS;
    handle.DirtyVariable("capture");
}

void close_binds() {
    set_capture(-1);
    binds_doc->Hide();
    save_config();
    show_settings();
}

void set_edit(bool on) {
    model.edit = on;
    model.selected = -1;
    handle.DirtyVariable("edit");
    handle.DirtyVariable("selected");
    if (on) { settings->Hide(); editbar->Show(); editbar->PullToFront(); }
    else { editbar->Hide(); save_config(); }
}

void bind_model() {
    Rml::DataModelConstructor m = ctx->CreateDataModel("ui");
    if (auto s = m.RegisterStruct<Group>()) {
        s.RegisterMember("left", &Group::left);
        s.RegisterMember("top", &Group::top);
        s.RegisterMember("font", &Group::font);
    }
    static std::vector<Group *> refs;
    for (auto &g : groups) refs.push_back(&g);
    m.RegisterArray<std::vector<Group *>>();
    m.Bind("g", &refs);
    m.Bind("dialog", &model.dialog);
    m.Bind("shortcuts", &model.shortcuts);
    m.Bind("auto_on", &model.auto_on);
    m.Bind("log", &model.log);
    m.Bind("edit", &model.edit);
    m.Bind("selected", &model.selected);
    m.Bind("pad_on", &model.pad_on);
    m.Bind("alpha", &model.alpha);
    m.Bind("gamepad", &model.gamepad);
    m.Bind("capture", &model.capture);
    if (auto s = m.RegisterStruct<Bind>()) {
        s.RegisterMember("name", &Bind::name);
        s.RegisterMember("key", &Bind::key);
    }
    m.RegisterArray<std::vector<Bind>>();
    m.Bind("binds", &binds);
    m.BindEventCallback("open_binds", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &) {
        settings->Hide();
        binds_doc->Show();
        binds_doc->PullToFront();
        focus_pending = binds_doc;
    });
    m.BindEventCallback("close_binds", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &) { close_binds(); });
    m.BindEventCallback("bind_start", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &args) {
        if (!args.empty()) set_capture(args[0].Get<int>());
    });
    m.BindEventCallback("bind_reset", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &) {
        gamepad_bind_reset();
        save_config();
    });
    m.BindFunc("pad_size", [](Rml::Variant &v) { v = model.size; }, [](const Rml::Variant &v) { model.size = v.Get<int>(); update_layout(); save_config(); });
    m.BindEventCallback("save", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &) { save_config(); });
    m.BindEventCallback("open_edit", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &) { set_edit(true); });
    m.BindEventCallback("close", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &) { settings->Hide(); save_config(); });
    m.BindEventCallback("done", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &) { set_edit(false); show_settings(); });
    m.BindEventCallback("reset", [](Rml::DataModelHandle h, Rml::Event &, const Rml::VariantList &) {
        reset_groups();
        model.size = 100;
        model.alpha = 85;
        h.DirtyVariable("pad_size");
        h.DirtyVariable("alpha");
        update_layout();
        save_config();
    });
    m.BindEventCallback("resize", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &args) {
        if (model.selected < 0 || args.empty()) return;
        auto &g = groups[model.selected];
        g.scale = std::clamp(g.scale + args[0].Get<float>(), 0.5f, 2.5f);
        update_layout();
        save_config();
    });
    handle = m.GetModelHandle();
}

int group_of(Rml::Element *e) {
    for (; e; e = e->GetParentNode())
        for (int i = 0; i < (int)std::size(groups); i++) if (e->GetId() == groups[i].id) return i;
    return -1;
}

Rml::Element *button_at(float x, float y) {
    Rml::Element *e = ctx->GetElementAtPoint({x, y});
    for (; e; e = e->GetParentNode()) if (e->HasAttribute("code") || e->HasAttribute("action")) return e;
    return nullptr;
}

void press(Rml::Element *b) {
    if (!b) return;
    if (b->HasAttribute("action")) {
        std::string a = b->GetAttribute<Rml::String>("action", "");
        if (a == "settings") ui_toggle_settings();
        else if (a == "auto") ui_toggle_auto();
        else if (a == "log") dialog_log_toggle();
        return;
    }
    int n = b->GetAttribute<int>("presses", 0);
    b->SetAttribute("presses", n + 1);
    b->SetClass("on", true);
    if (n == 0) host_key(2, b->GetAttribute<int>("code", 0));
}

void release(Rml::Element *b) {
    if (!b || b->HasAttribute("action")) return;
    int n = b->GetAttribute<int>("presses", 0);
    if (n <= 0) return;
    b->SetAttribute("presses", n - 1);
    if (n == 1) { b->SetClass("on", false); host_key(3, b->GetAttribute<int>("code", 0)); }
}

void finger_event(const SDL_Event *e) {
    float x = e->tfinger.x * out_w, y = e->tfinger.y * out_h;
    Finger *f = nullptr;
    for (auto &c : fingers) if (c.used && c.id == e->tfinger.fingerID) f = &c;
    if (e->type == SDL_EVENT_FINGER_DOWN) {
        if (f) return;
        for (auto &c : fingers) if (!c.used) { f = &c; break; }
        if (!f) return;
        *f = {e->tfinger.fingerID, true, nullptr, -1, 0, 0};
        if (model.edit) {
            int g = group_of(ctx->GetElementAtPoint({x, y}));
            if (g < 0) return;
            model.selected = g;
            handle.DirtyVariable("selected");
            f->group = g;
            if (groups[g].x == UNPLACED) { groups[g].x = groups[g].center_x; groups[g].y = groups[g].center_y; }
            f->grab_x = groups[g].x - x / out_w;
            f->grab_y = groups[g].y - y / out_h;
        } else {
            f->button = button_at(x, y);
            f->grab_y = y;
            press(f->button);
        }
    } else if (f && e->type == SDL_EVENT_FINGER_MOTION) {
        if (!model.edit && !f->button && dialog_log_is_open()) {
            // 대사 기록은 끌어서 넘긴다. 손가락을 올리면 아래 줄이 보인다.
            float row = LOG_ROW_RATIO * out_h;
            int rows = (int)((f->grab_y - y) / row);
            if (rows) { dialog_log_scroll(rows); f->grab_y -= rows * row; }
        } else if (model.edit) {
            if (f->group >= 0) {
                groups[f->group].x = std::clamp(x / out_w + f->grab_x, 0.f, 1.f);
                groups[f->group].y = std::clamp(y / out_h + f->grab_y, 0.f, 1.f);
                update_layout();
            }
        } else if (f->button && group_of(f->button) == 0) {
            // 방향 패드는 손가락을 떼지 않고 옮겨 갈 수 있다.
            Rml::Element *hit = button_at(x, y);
            if (hit && hit != f->button && group_of(hit) == 0) { press(hit); release(f->button); f->button = hit; }
        }
    } else if (f) {
        release(f->button);
        if (model.edit && f->group >= 0) save_config();
        f->used = false;
    }
}

int mouse_button(int b) { return b == SDL_BUTTON_LEFT ? 0 : b == SDL_BUTTON_RIGHT ? 1 : 2; }

} // namespace

extern "C" bool ui_modal(void) { return ctx && (model.edit || settings->IsVisible() || binds_doc->IsVisible()); }

extern "C" bool ui_pad_capturing(void) { return ctx && model.capture >= 0; }

extern "C" bool ui_pad_capture(int input) {
    if (!ui_pad_capturing()) return false;
    gamepad_bind(model.capture, input);
    set_capture(-1);
    save_config();
    return true;
}

extern "C" void ui_pad_nav(int nav) {
    if (!ctx) return;
    if (nav == UI_NAV_BACK) { ui_toggle_settings(); return; }
    static const Rml::Input::KeyIdentifier KEYS[] = {Rml::Input::KI_UP, Rml::Input::KI_DOWN, Rml::Input::KI_LEFT, Rml::Input::KI_RIGHT, Rml::Input::KI_RETURN};
    ctx->ProcessKeyDown(KEYS[nav], 0);
    ctx->ProcessKeyUp(KEYS[nav], 0);
}

extern "C" void ui_release_all(void) {
    if (!ctx) return;
    for (auto &c : fingers) if (c.used) { release(c.button); c.used = false; }
}

extern "C" void ui_toggle_settings(void) {
    if (!ctx) return;
    gamepad_release_all();
    if (ui_pad_capturing()) { set_capture(-1); return; }
    if (binds_doc->IsVisible()) { close_binds(); return; }
    if (model.edit) { set_edit(false); return; }
    ui_release_all();
    if (settings->IsVisible()) { settings->Hide(); save_config(); }
    else show_settings();
}

extern "C" void ui_toggle_auto(void) {
    dialog_set_auto(!dialog_auto());
    if (ctx) save_config();
}

extern "C" bool ui_init(SDL_Window *win, SDL_Renderer *ren, const char *ui_dir, const char *config_path, bool force_pad) {
    window = win;
    renderer = ren;
    cfg_path = config_path;
    render_if = new UiRender(ren);
    Rml::SetRenderInterface(render_if);
    if (!Rml::Initialise()) return false;
    std::string dir = ui_dir;
    if (!Rml::LoadFontFace(dir + "/ChronosGame.ttf", true)) return false;
    ctx = Rml::CreateContext("main", {640, 320});
    bind_model();
#ifdef __ANDROID__
    load_config(true);
#else
    load_config(false);
#endif
    if (force_pad) model.pad_on = true;
    update_sizes();
    pad = ctx->LoadDocument(dir + "/pad.rml");
    settings = ctx->LoadDocument(dir + "/settings.rml");
    editbar = ctx->LoadDocument(dir + "/editbar.rml");
    binds_doc = ctx->LoadDocument(dir + "/gamepad.rml");
    if (!ctx || !pad || !settings || !editbar || !binds_doc) { ctx = nullptr; return false; }
    pad->Show();
    return true;
}

extern "C" bool ui_event(const SDL_Event *e) {
    if (!ctx) return false;
    bool modal = ui_modal();
    switch (e->type) {
    case SDL_EVENT_FINGER_DOWN: case SDL_EVENT_FINGER_MOTION: case SDL_EVENT_FINGER_UP: case SDL_EVENT_FINGER_CANCELED:
        if (model.edit || (model.pad_on && !modal)) finger_event(e);
        return true;
    case SDL_EVENT_MOUSE_MOTION: case SDL_EVENT_MOUSE_BUTTON_DOWN: case SDL_EVENT_MOUSE_BUTTON_UP: {
        if (!modal) return false;
        int ww, wh;
        SDL_GetWindowSize(window, &ww, &wh);
        float sx = ww ? (float)out_w / ww : 1, sy = wh ? (float)out_h / wh : 1;
        if (e->type == SDL_EVENT_MOUSE_MOTION) ctx->ProcessMouseMove((int)(e->motion.x * sx), (int)(e->motion.y * sy), 0);
        else {
            ctx->ProcessMouseMove((int)(e->button.x * sx), (int)(e->button.y * sy), 0);
            if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN) ctx->ProcessMouseButtonDown(mouse_button(e->button.button), 0);
            else ctx->ProcessMouseButtonUp(mouse_button(e->button.button), 0);
        }
        return true;
    }
    case SDL_EVENT_KEY_DOWN: case SDL_EVENT_KEY_UP:
        if (e->key.scancode == SDL_SCANCODE_F1) {
            if (e->type == SDL_EVENT_KEY_DOWN && !e->key.repeat) ui_toggle_settings();
            return true;
        }
        if (modal) {
            if (e->type == SDL_EVENT_KEY_DOWN && !e->key.repeat && (e->key.scancode == SDL_SCANCODE_ESCAPE || e->key.scancode == SDL_SCANCODE_AC_BACK)) ui_toggle_settings();
            return e->key.scancode != SDL_SCANCODE_F11;
        }
        return false;
    default:
        return false;
    }
}

extern "C" void ui_render(void) {
    if (!ctx) return;
    update_sizes();
    bool dialog = game_in_dialog();
    if (dialog != model.dialog) {
        if (dialog) ui_release_all();
        model.dialog = dialog;
        handle.DirtyVariable("dialog");
    }
    if (devmode_hud_top() != hud_top) {
        hud_top = !hud_top;
        update_layout();
    }
    bool shortcuts = game_shortcuts_bought();
    if (shortcuts != model.shortcuts) {
        model.shortcuts = shortcuts;
        handle.DirtyVariable("shortcuts");
    }
    if (dialog_auto() != model.auto_on) {
        model.auto_on = dialog_auto();
        handle.DirtyVariable("auto_on");
    }
    if (dialog_log_is_open() != model.log) {
        model.log = !model.log;
        ui_release_all();
        handle.DirtyVariable("log");
    }
    if (gamepad_generation() != pad_generation) {
        pad_generation = gamepad_generation();
        refresh_binds();
        if (gamepad_connected() != model.gamepad) {
            model.gamepad = !model.gamepad;
            handle.DirtyVariable("gamepad");
            if (!model.gamepad && binds_doc->IsVisible()) close_binds();
        }
    }
    if (ui_pad_capturing() && SDL_GetTicks() > capture_until) set_capture(-1);
    ctx->Update();
    if (focus_pending) {
        Rml::Element *first = focus_pending->QuerySelector("button.bind, #first");
        if (first) first->Focus(true);
        focus_pending = nullptr;
    }
    ctx->Render();
    SDL_SetRenderClipRect(renderer, nullptr);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}
