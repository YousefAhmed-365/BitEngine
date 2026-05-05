#ifndef BIT_AUDIO_HPP
#define BIT_AUDIO_HPP

#include "raylib.h"
#include <unordered_map>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// BitAudio: Sound & Music Management
// ─────────────────────────────────────────────────────────────────────────────
// Manages all audio playback for BitEngine using Raylib's audio API.
//
// Audio Types:
//   - BGM (Background Music): Streamed, looped, single instance at a time
//   - SFX (Sound Effects): Short samples, concurrent playback, cached
//
// Features:
//   - Asset caching prevents repeated disk I/O
//   - Fade transitions between tracks
//   - Graceful handling of missing audio files
// ─────────────────────────────────────────────────────────────────────────────
class BitAudio {
public:
    BitAudio();
    ~BitAudio();
    
    // Initialization
    void Initialize();
    void Shutdown();
    
    // Music Management
    void PlayMusic(const std::string& path);
    void StopMusic();
    void UpdateMusic();
    bool IsMusicPlaying() const { return m_isMusicPlaying; }
    
    // SFX Management
    void PlaySFX(const std::string& path);
    
    // Cache Management
    Sound GetOrLoadSFX(const std::string& path);
    Music GetOrLoadMusic(const std::string& path);
    
    // Cleanup
    void UnloadAll();

private:
    std::unordered_map<std::string, Sound> m_sfxCache;
    std::unordered_map<std::string, Music> m_musicCache;
    
    std::string m_currentMusicPath;
    Music m_currentMusic = {};
    bool m_isMusicPlaying = false;
};

#endif
