// RmlUi를 SDL_Renderer로 그린다. 게임 화면 위에 얹으므로 화면을 지우지 않고, 이미지 파일은 쓰지 않아 SDL_image가 필요 없다.
#include "ui_render.h"
#include <vector>

UiRender::UiRender(SDL_Renderer *r) : ren(r) {
    // RmlUi 색은 알파가 미리 곱해져 있다.
    blend = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD,
                                       SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD);
}

Rml::CompiledGeometryHandle UiRender::CompileGeometry(Rml::Span<const Rml::Vertex> v, Rml::Span<const int> i) {
    return reinterpret_cast<Rml::CompiledGeometryHandle>(new Geometry{v, i});
}

void UiRender::ReleaseGeometry(Rml::CompiledGeometryHandle g) { delete reinterpret_cast<Geometry *>(g); }

void UiRender::RenderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f t, Rml::TextureHandle texture) {
    auto *g = reinterpret_cast<Geometry *>(handle);
    std::vector<SDL_Vertex> out(g->vertices.size());
    for (size_t i = 0; i < out.size(); i++) {
        const Rml::Vertex &v = g->vertices[i];
        out[i].position = {v.position.x + t.x, v.position.y + t.y};
        out[i].color = {v.colour.red / 255.f, v.colour.green / 255.f, v.colour.blue / 255.f, v.colour.alpha / 255.f};
        out[i].tex_coord = {v.tex_coord.x, v.tex_coord.y};
    }
    auto *tex = reinterpret_cast<SDL_Texture *>(texture);
    SDL_SetRenderDrawBlendMode(ren, blend);
    if (tex) SDL_SetTextureBlendMode(tex, blend);
    SDL_RenderGeometry(ren, tex, out.data(), (int)out.size(), g->indices.data(), (int)g->indices.size());
}

Rml::TextureHandle UiRender::LoadTexture(Rml::Vector2i &size, const Rml::String &source) {
    SDL_Surface *png = SDL_LoadPNG(source.c_str());
    if (!png) return 0;
    SDL_Surface *s = SDL_ConvertSurface(png, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(png);
    if (!s) return 0;
    for (int y = 0; y < s->h; y++) {
        Uint8 *p = (Uint8 *)s->pixels + y * s->pitch;
        for (int x = 0; x < s->w; x++, p += 4) {
            int a = p[3];
            p[0] = (Uint8)(p[0] * a / 255); p[1] = (Uint8)(p[1] * a / 255); p[2] = (Uint8)(p[2] * a / 255);
        }
    }
    size = {s->w, s->h};
    SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
    SDL_DestroySurface(s);
    return reinterpret_cast<Rml::TextureHandle>(t);
}

Rml::TextureHandle UiRender::GenerateTexture(Rml::Span<const Rml::byte> src, Rml::Vector2i size) {
    SDL_Surface *s = SDL_CreateSurfaceFrom(size.x, size.y, SDL_PIXELFORMAT_RGBA32, (void *)src.data(), size.x * 4);
    if (!s) return 0;
    SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
    SDL_DestroySurface(s);
    return reinterpret_cast<Rml::TextureHandle>(t);
}

void UiRender::ReleaseTexture(Rml::TextureHandle t) { SDL_DestroyTexture(reinterpret_cast<SDL_Texture *>(t)); }

void UiRender::EnableScissorRegion(bool enable) {
    scissor = enable;
    SDL_SetRenderClipRect(ren, enable ? &rect : nullptr);
}

void UiRender::SetScissorRegion(Rml::Rectanglei r) {
    rect = {r.Left(), r.Top(), r.Width(), r.Height()};
    if (scissor) SDL_SetRenderClipRect(ren, &rect);
}
