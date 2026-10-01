#pragma once
#include "leeds_texture.h"
#include <string>

bool loadImageWithWic(const std::wstring& path, RgbaImage& image, std::string& errorMessage);
bool savePngWithWic(const std::wstring& path, const RgbaImage& image, std::string& errorMessage);
