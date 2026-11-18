#pragma once
#include "src/leeds/leeds_archive.h"
#include <windows.h>
#include <GL/gl.h>
#include "src/graphics/storyland_directx.h"
#include "src/graphics/storyland_texture_upload.h"
#include "src/graphics/storyland_ps2_texture_upload.h"
#include <map>

// Upload progressively and retain a bounded working set. Keeping the shared
// pixel owner also prevents a reloaded archive from reusing a stale cache key.
class StorylandGpuTextureCache {
    struct Entry {
        GLuint handle = 0;
        size_t bytes = 0;
        uint64_t lastFrame = 0;
        StorylandSharedBuffer<uint8_t> pixels;
    };
    std::map<const uint8_t*, Entry> entries;
    HGLRC context = nullptr;
    size_t residentBytes = 0, uploadedBytes = 0;
    uint64_t frame = 0;
    bool nativePs2 = false;
    size_t capacity = 96u * 1024u * 1024u;
    size_t frameBudget = 4u * 1024u * 1024u;
public:
    explicit StorylandGpuTextureCache(size_t maxResidentBytes = 96u * 1024u * 1024u,
        size_t maxUploadBytesPerFrame = 4u * 1024u * 1024u)
        : capacity(maxResidentBytes), frameBudget(maxUploadBytesPerFrame) {}
    void reset() {
        // A null context does not make OpenGL deletion safe. A switched or
        // destroyed context owns its GPU objects; release our CPU owners here.
        if (context != nullptr && context == wglGetCurrentContext()) {
            for (const auto& item : entries) {
                if (item.second.handle != 0)
                    glDeleteTextures(1, &item.second.handle);
            }
        }
        entries.clear();
        residentBytes = 0;
        uploadedBytes = 0;
        context = nullptr;
    }
    void beginFrame(bool ps2 = false) {
        if(nativePs2!=ps2) {reset();nativePs2=ps2;}
        const HGLRC current = wglGetCurrentContext();
        if (context != current) { reset(); context = current; }
        uploadedBytes = 0; ++frame;
    }
    GLuint get(const StorylandDirectTextureResource* texture,bool immediate=false) {
        if (!texture || texture->width <= 0 || texture->height <= 0 || texture->rgba.empty()) return 0;
        const auto key = texture->rgba.data();
        const auto found = entries.find(key);
        if (found != entries.end()) { found->second.lastFrame = frame; return found->second.handle; }
        size_t bytes = texture->rgba.size();
        const size_t mipCount=storylandAuthoredMipCount(texture->width,texture->height,texture->mipLevels);
        for(size_t level=0;level<mipCount;++level)bytes+=texture->mipLevels[level].rgba.size();
        if (bytes > capacity || (!immediate && uploadedBytes && uploadedBytes + bytes > frameBudget)) return 0;
        // A mesh drawn again this frame needs the same texture handle.
        while (residentBytes + bytes > capacity) {
            auto oldest = entries.end();
            for (auto item = entries.begin(); item != entries.end(); ++item)
                if (item->second.lastFrame != frame &&
                    (oldest == entries.end() || item->second.lastFrame < oldest->second.lastFrame)) oldest = item;
            if (oldest == entries.end()) return 0;
            glDeleteTextures(1, &oldest->second.handle);
            residentBytes -= oldest->second.bytes;
            entries.erase(oldest);
        }
        GLuint handle = 0;
        glGenTextures(1, &handle);
        if (!handle) return 0;
        GLint previousTexture = 0;
        GLint previousAlignment = 4;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);
        glBindTexture(GL_TEXTURE_2D, handle);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        storylandUploadStoriesTexture(*texture,nativePs2);
        glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);
        glBindTexture(GL_TEXTURE_2D, GLuint(previousTexture));
        entries.emplace(key, Entry{handle, bytes, frame, texture->rgba});
        residentBytes += bytes; uploadedBytes += bytes;
        return handle;
    }
};
