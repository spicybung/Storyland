#pragma once
#include <algorithm>
#include <cwctype>
#include <string>

inline std::wstring storylandAreaStem(std::wstring stem) {
    std::transform(stem.begin(),stem.end(),stem.begin(),[](wchar_t value){return wchar_t(std::towlower(value));});
    const auto suffix=stem.find(L'(');
    const auto base=stem.substr(0,suffix);
    if(base!=L"mainla"&&base!=L"beach"&&base!=L"mall")return stem;
    if(suffix==std::wstring::npos)return base;
    if(stem.back()!=L')')return stem;
    for(size_t i=suffix+1;i+1<stem.size();++i)
        if(!std::iswdigit(stem[i])&&stem[i]!=L'-')return stem;
    return base;
}
