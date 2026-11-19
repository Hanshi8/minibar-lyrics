// lyrics.h — synced lyrics for the track that is playing.
//
// This is a fork-only addition: upstream MiniBar deliberately leaves lyrics
// out of scope (SMTC does not expose them). The lyrics come from public
// lyric databases, no account or API key needed:
//
//   1. LRCLIB (lrclib.net) — exact match by artist/title/album/duration,
//      then a search fallback.
//   2. NetEase Cloud Music public search + lyric endpoints — fallback,
//      useful for Chinese songs.
//
// The fetch runs on a worker thread and posts WM_APP_LYRICS to the UI
// thread, the same pattern SmtcService uses. No hooks, no injection, no
// extra DLLs: plain WinHTTP, which ships with Windows.
#pragma once
#include "common.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct LyricLine {
    int64_t      ms   = 0;  // milliseconds from the start of the track
    std::wstring text;
};

// Payload of WM_APP_LYRICS. The receiver takes ownership.
struct LyricsResult {
    uint32_t               gen   = 0;
    std::wstring           key;  // title + L'\n' + artist of the request
    bool                   found = false;
    std::vector<LyricLine> lines;
};

class LyricsFetcher {
public:
    struct Impl;  // shared with the worker threads, like SmtcService::Impl

    LyricsFetcher();
    ~LyricsFetcher();

    void Start(HWND hwnd);
    void Stop();

    // Fetches lyrics for a track in the background. The result arrives as
    // WM_APP_LYRICS; `gen` lets the UI drop answers for tracks that have
    // already changed. Tracks looked up before are served from an in-memory
    // cache (last 16).
    void Request(uint32_t gen, const std::wstring& title, const std::wstring& artist,
                 const std::wstring& album, int durationSec);

    // Current line for a playback position in milliseconds; empty before the
    // first timestamp.
    static std::wstring LineAt(const std::vector<LyricLine>& lines, int64_t posMs);

    // Parses "[mm:ss.cc]text" lines (also "[mm:ss]"); metadata tags
    // ([ti:...], [ar:...]) and empty lines are skipped.
    static std::vector<LyricLine> ParseLrc(const std::wstring& lrc);

private:
    std::shared_ptr<Impl> d_;
};
