#pragma once
#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

class UiRender : public Rml::RenderInterface {
public:
    explicit UiRender(SDL_Renderer *r);
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;
    void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
    Rml::TextureHandle LoadTexture(Rml::Vector2i &dimensions, const Rml::String &source) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i dimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture) override;
    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(Rml::Rectanglei region) override;

private:
    struct Geometry {
        Rml::Span<const Rml::Vertex> vertices;
        Rml::Span<const int> indices;
    };
    SDL_Renderer *ren;
    SDL_BlendMode blend;
    SDL_Rect rect{};
    bool scissor = false;
};
