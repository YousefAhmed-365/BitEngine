#include "BitAudio.hpp"
#include <iostream>

BitAudio::BitAudio() {}

BitAudio::~BitAudio() {
    Shutdown();
}

void BitAudio::Initialize() {
    InitAudioDevice();
}

void BitAudio::Shutdown() {
    UnloadAll();
    CloseAudioDevice();
}

void BitAudio::PlayMusic(const std::string& path) {
    if (m_currentMusicPath == path && m_isMusicPlaying) return;
    
    if (m_isMusicPlaying) {
        StopMusic();
    }
    
    m_currentMusic = GetOrLoadMusic(path);
    m_currentMusicPath = path;
    PlayMusicStream(m_currentMusic);
    m_isMusicPlaying = true;
}

void BitAudio::StopMusic() {
    if (m_isMusicPlaying) {
        StopMusicStream(m_currentMusic);
        m_isMusicPlaying = false;
    }
}

void BitAudio::UpdateMusic() {
    if (m_isMusicPlaying) {
        UpdateMusicStream(m_currentMusic);
    }
}

void BitAudio::PlaySFX(const std::string& path) {
    Sound sfx = GetOrLoadSFX(path);
    PlaySound(sfx);
}

Sound BitAudio::GetOrLoadSFX(const std::string& path) {
    auto it = m_sfxCache.find(path);
    if (it != m_sfxCache.end()) {
        return it->second;
    }
    Sound sfx = LoadSound(path.c_str());
    m_sfxCache[path] = sfx;
    return sfx;
}

Music BitAudio::GetOrLoadMusic(const std::string& path) {
    auto it = m_musicCache.find(path);
    if (it != m_musicCache.end()) {
        return it->second;
    }
    Music music = LoadMusicStream(path.c_str());
    m_musicCache[path] = music;
    return music;
}

void BitAudio::UnloadAll() {
    StopMusic();
    for (auto& [p, s] : m_sfxCache) UnloadSound(s);
    for (auto& [p, m] : m_musicCache) UnloadMusicStream(m);
    m_sfxCache.clear();
    m_musicCache.clear();
}
