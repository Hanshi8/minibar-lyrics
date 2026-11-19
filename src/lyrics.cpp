// lyrics.cpp — synced lyrics via LRCLIB (+ NetEase fallback), on a worker
// thread. See lyrics.h for the rationale. Everything network happens here;
// the UI thread only receives the finished LyricsResult.
#include "lyrics.h"

#include <winhttp.h>

#include <algorithm>
#include <thread>

namespace {

// ------------------------------------------------------------------ text ---
std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

std::string WideToUtf8(const std::wstring& s) {
    if (s.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0,
                                      nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n, nullptr,
                        nullptr);
    return out;
}

std::wstring Trim(const std::wstring& s) {
    const size_t b = s.find_first_not_of(L" \t\r\n");
    if (b == std::wstring::npos) return {};
    const size_t e = s.find_last_not_of(L" \t\r\n");
    return s.substr(b, e - b + 1);
}

// Percent-encodes a UTF-8 string for a query parameter.
std::string UrlEncode(const std::wstring& value) {
    const std::string utf8 = WideToUtf8(value);
    static const char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(utf8.size());
    for (const unsigned char c : utf8) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
        if (ok) {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += kHex[(c >> 4) & 0xF];
            out += kHex[c & 0xF];
        }
    }
    return out;
}

// ------------------------------------------------------------------ http ---
// Minimal HTTPS GET. Returns true only on HTTP 200 with a body.
bool HttpGet(const std::wstring& host, const std::wstring& path, const wchar_t* extraHeaders,
             std::string& body) {
    body.clear();
    HINTERNET session = WinHttpOpen(L"MiniBar-Lyrics/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return false;
    WinHttpSetTimeouts(session, 5000, 5000, 5000, 8000);

    bool ok = false;
    HINTERNET conn = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (conn) {
        HINTERNET req = WinHttpOpenRequest(conn, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (req) {
            if (extraHeaders)
                WinHttpAddRequestHeaders(req, extraHeaders, static_cast<ULONG>(-1),
                                         WINHTTP_ADDREQ_FLAG_ADD);
            if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA,
                                   0, 0, 0) &&
                WinHttpReceiveResponse(req, nullptr)) {
                DWORD status = 0;
                DWORD len    = sizeof(status);
                if (WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &len,
                                        WINHTTP_NO_HEADER_INDEX) &&
                    status == 200) {
                    ok = true;
                    for (;;) {
                        DWORD avail = 0;
                        if (!WinHttpQueryDataAvailable(req, &avail) || avail == 0) break;
                        std::string chunk(avail, '\0');
                        DWORD read = 0;
                        if (!WinHttpReadData(req, chunk.data(), avail, &read) || read == 0) {
                            ok = false;
                            break;
                        }
                        chunk.resize(read);
                        body += chunk;
                        if (body.size() > 4u * 1024 * 1024) {  // sanity cap
                            ok = false;
                            break;
                        }
                    }
                }
            }
            WinHttpCloseHandle(req);
        }
        WinHttpCloseHandle(conn);
    }
    WinHttpCloseHandle(session);
    return ok;
}

// ------------------------------------------------------------------ json ---
// Finds `"key":"..."` (or any marker ending right before a quoted string)
// starting at `from`, and returns the unescaped string value. Empty when the
// key is absent or its value is null / not a string.
std::wstring JsonStringAfter(const std::string& json, const std::string& marker, size_t from = 0) {
    const size_t at = json.find(marker, from);
    if (at == std::string::npos) return {};
    size_t i = at + marker.size();
    if (i >= json.size() || json[i] != '"') return {};  // null or not a string
    ++i;

    std::string raw;
    bool        esc = false;
    for (; i < json.size(); ++i) {
        const char c = json[i];
        if (esc) {
            raw += '\\';
            raw += c;
            esc = false;
        } else if (c == '\\') {
            esc = true;
        } else if (c == '"') {
            break;
        } else {
            raw += c;
        }
    }

    // Unescape into UTF-8, then convert: \n \r \t \" \\ \/ \uXXXX (with
    // surrogate pairs for characters outside the BMP).
    std::string utf8;
    for (size_t j = 0; j < raw.size(); ++j) {
        const char c = raw[j];
        if (c != '\\' || j + 1 >= raw.size()) {
            utf8 += c;
            continue;
        }
        const char e = raw[++j];
        switch (e) {
            case 'n': utf8 += '\n'; break;
            case 'r': utf8 += '\r'; break;
            case 't': utf8 += '\t'; break;
            case 'b': utf8 += '\b'; break;
            case 'f': utf8 += '\f'; break;
            case '/': utf8 += '/'; break;
            case '\\': utf8 += '\\'; break;
            case '"': utf8 += '"'; break;
            case 'u': {
                if (j + 4 >= raw.size()) break;
                auto hex = [&](size_t k) -> unsigned {
                    const char h = raw[k];
                    if (h >= '0' && h <= '9') return static_cast<unsigned>(h - '0');
                    if (h >= 'a' && h <= 'f') return static_cast<unsigned>(h - 'a' + 10);
                    if (h >= 'A' && h <= 'F') return static_cast<unsigned>(h - 'A' + 10);
                    return 0;
                };
                unsigned cp = (hex(j + 1) << 12) | (hex(j + 2) << 8) | (hex(j + 3) << 4) |
                              hex(j + 4);
                j += 4;
                // Surrogate pair: \uD83D\uDE00.
                if (cp >= 0xD800 && cp <= 0xDBFF && j + 6 < raw.size() && raw[j + 1] == '\\' &&
                    raw[j + 2] == 'u') {
                    const unsigned lo = (hex(j + 3) << 12) | (hex(j + 4) << 8) |
                                        (hex(j + 5) << 4) | hex(j + 6);
                    if (lo >= 0xDC00 && lo <= 0xDFFF) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        j += 6;
                    }
                }
                // Encode cp as UTF-8.
                if (cp < 0x80) {
                    utf8 += static_cast<char>(cp);
                } else if (cp < 0x800) {
                    utf8 += static_cast<char>(0xC0 | (cp >> 6));
                    utf8 += static_cast<char>(0x80 | (cp & 0x3F));
                } else if (cp < 0x10000) {
                    utf8 += static_cast<char>(0xE0 | (cp >> 12));
                    utf8 += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    utf8 += static_cast<char>(0x80 | (cp & 0x3F));
                } else {
                    utf8 += static_cast<char>(0xF0 | (cp >> 18));
                    utf8 += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                    utf8 += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    utf8 += static_cast<char>(0x80 | (cp & 0x3F));
                }
                break;
            }
            default: utf8 += e; break;
        }
    }
    return Utf8ToWide(utf8);
}

// ------------------------------------------------------------------- lrc ---
// "[mm:ss.cc]" (or "[mm:ss]") -> milliseconds; -1 when it is not a time tag
// (metadata like [ti:...] or garbage).
int64_t ParseTimeTag(const std::wstring& tag) {
    const size_t colon = tag.find(L':');
    if (colon == std::wstring::npos || colon == 0) return -1;
    auto allDigits = [](const std::wstring& s) {
        if (s.empty()) return false;
        for (const wchar_t c : s)
            if (c < L'0' || c > L'9') return false;
        return true;
    };
    const std::wstring mm  = tag.substr(0, colon);
    std::wstring       sec = tag.substr(colon + 1);
    if (!allDigits(mm)) return -1;

    int64_t fracMs = 0;
    const size_t dot = sec.find(L'.');
    if (dot != std::wstring::npos) {
        const std::wstring frac = sec.substr(dot + 1);
        if (!allDigits(frac) || frac.empty() || frac.size() > 3) return -1;
        sec = sec.substr(0, dot);
        int64_t f = 0;
        for (const wchar_t c : frac) f = f * 10 + (c - L'0');
        if (frac.size() == 1) f *= 100;
        if (frac.size() == 2) f *= 10;
        fracMs = f;
    }
    if (!allDigits(sec)) return -1;
    int64_t s = 0, m = 0;
    for (const wchar_t c : mm) m = m * 10 + (c - L'0');
    for (const wchar_t c : sec) s = s * 10 + (c - L'0');
    if (s >= 60) return -1;
    return ((m * 60) + s) * 1000 + fracMs;
}

// --------------------------------------------------------------- sources ---
std::vector<LyricLine> FetchLrclib(const std::wstring& title, const std::wstring& artist,
                                   const std::wstring& album, int durationSec) {
    std::string body;
    std::wstring path = L"/api/get?artist_name=" + Utf8ToWide(UrlEncode(artist)) +
                        L"&track_name=" + Utf8ToWide(UrlEncode(title));
    if (!album.empty()) path += L"&album_name=" + Utf8ToWide(UrlEncode(album));
    if (durationSec > 0) path += L"&duration=" + std::to_wstring(durationSec);

    if (HttpGet(L"lrclib.net", path, nullptr, body)) {
        const std::wstring lrc = JsonStringAfter(body, "\"syncedLyrics\":");
        if (!lrc.empty()) {
            auto lines = LyricsFetcher::ParseLrc(lrc);
            if (!lines.empty()) return lines;
        }
    }

    // Search fallback: first result that actually has synced lyrics.
    path = L"/api/search?track_name=" + Utf8ToWide(UrlEncode(title)) +
           L"&artist_name=" + Utf8ToWide(UrlEncode(artist));
    if (HttpGet(L"lrclib.net", path, nullptr, body)) {
        const std::wstring lrc = JsonStringAfter(body, "\"syncedLyrics\":");
        if (!lrc.empty()) {
            auto lines = LyricsFetcher::ParseLrc(lrc);
            if (!lines.empty()) return lines;
        }
    }
    return {};
}

std::vector<LyricLine> FetchNetease(const std::wstring& title, const std::wstring& artist) {
    static const wchar_t kHeaders[] =
        L"Referer: https://music.163.com/\r\n"
        L"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        L"(KHTML, like Gecko) Chrome/120.0 Safari/537.36\r\n";

    std::string body;
    const std::wstring query = Utf8ToWide(UrlEncode(title + L" " + artist));
    const std::wstring path =
        L"/api/search/get/web?s=" + query + L"&type=1&offset=0&total=true&limit=3";
    if (!HttpGet(L"music.163.com", path, kHeaders, body)) return {};

    // First song id inside "songs":[{..."id":12345...
    const size_t songs = body.find("\"songs\":[{");
    if (songs == std::string::npos) return {};
    const size_t idAt = body.find("\"id\":", songs);
    if (idAt == std::string::npos) return {};
    size_t i = idAt + 5;
    while (i < body.size() && (body[i] == ' ' || body[i] == '\t')) ++i;
    std::string idStr;
    while (i < body.size() && body[i] >= '0' && body[i] <= '9') idStr += body[i++];
    if (idStr.empty()) return {};

    const std::wstring lyricPath =
        L"/api/song/lyric?id=" + Utf8ToWide(idStr) + L"&lv=-1&kv=-1&tv=-1";
    if (!HttpGet(L"music.163.com", lyricPath, kHeaders, body)) return {};

    const size_t lrcAt = body.find("\"lrc\":{");
    if (lrcAt == std::string::npos) return {};
    const std::wstring lrc = JsonStringAfter(body, "\"lyric\":", lrcAt);
    if (lrc.empty()) return {};
    return LyricsFetcher::ParseLrc(lrc);
}

}  // namespace

struct LyricsFetcher::Impl {
    HWND hwnd = nullptr;
    std::atomic<bool> alive{false};

    std::mutex cacheMutex;
    // Most-recent-first; capped at 16 entries.
    std::vector<std::pair<std::wstring, std::vector<LyricLine>>> cache;

    void Post(LyricsResult* result) {
        if (alive && hwnd && IsWindow(hwnd)) {
            if (PostMessageW(hwnd, WM_APP_LYRICS, result->gen, reinterpret_cast<LPARAM>(result)))
                return;
        }
        delete result;  // window gone: nobody will own it
    }
};

LyricsFetcher::LyricsFetcher() : d_(std::make_shared<Impl>()) {}
LyricsFetcher::~LyricsFetcher() { Stop(); }

void LyricsFetcher::Start(HWND hwnd) {
    d_->hwnd  = hwnd;
    d_->alive = true;
}

void LyricsFetcher::Stop() {
    if (!d_) return;
    d_->alive = false;
    d_->hwnd  = nullptr;
}

void LyricsFetcher::Request(uint32_t gen, const std::wstring& title, const std::wstring& artist,
                             const std::wstring& album, int durationSec) {
    if (!d_->alive || title.empty()) return;

    const std::wstring key = title + L'\n' + artist;

    {
        std::lock_guard<std::mutex> lock(d_->cacheMutex);
        for (size_t i = 0; i < d_->cache.size(); ++i) {
            if (d_->cache[i].first == key) {
                auto lines = d_->cache[i].second;
                // Move to front (MRU).
                auto entry = d_->cache[i];
                d_->cache.erase(d_->cache.begin() + static_cast<long>(i));
                d_->cache.insert(d_->cache.begin(), std::move(entry));
                auto* res = new LyricsResult{gen, key, !lines.empty(), std::move(lines)};
                d_->Post(res);
                return;
            }
        }
    }

    auto d = d_;
    std::thread([d, gen, key, title, artist, album, durationSec] {
        auto lines = FetchLrclib(title, artist, album, durationSec);
        if (lines.empty()) lines = FetchNetease(title, artist);

        if (!lines.empty()) {
            std::lock_guard<std::mutex> lock(d->cacheMutex);
            d->cache.insert(d->cache.begin(), {key, lines});
            if (d->cache.size() > 16) d->cache.pop_back();
        }
        auto* res = new LyricsResult{gen, key, !lines.empty(), std::move(lines)};
        d->Post(res);
    }).detach();
}

std::wstring LyricsFetcher::LineAt(const std::vector<LyricLine>& lines, int64_t posMs) {
    if (lines.empty()) return {};
    // Last line whose timestamp is <= posMs.
    const auto it = std::upper_bound(
        lines.begin(), lines.end(), posMs,
        [](int64_t value, const LyricLine& line) { return value < line.ms; });
    if (it == lines.begin()) return {};
    return (it - 1)->text;
}

std::vector<LyricLine> LyricsFetcher::ParseLrc(const std::wstring& lrc) {
    std::vector<LyricLine> out;
    size_t pos = 0;
    while (pos < lrc.size()) {
        const size_t eol  = lrc.find(L'\n', pos);
        std::wstring line = (eol == std::wstring::npos) ? lrc.substr(pos)
                                                        : lrc.substr(pos, eol - pos);
        pos = (eol == std::wstring::npos) ? lrc.size() : eol + 1;
        line = Trim(line);
        if (line.empty()) continue;

        std::vector<int64_t> times;
        size_t i = 0;
        while (i < line.size() && line[i] == L'[') {
            const size_t close = line.find(L']', i);
            if (close == std::wstring::npos) break;
            const int64_t ms = ParseTimeTag(line.substr(i + 1, close - i - 1));
            if (ms < 0) break;  // metadata tag ([ti:...], ...): not a lyric line
            times.push_back(ms);
            i = close + 1;
        }
        if (times.empty()) continue;
        const std::wstring text = Trim(line.substr(i));
        if (text.empty()) continue;
        for (const int64_t t : times) out.push_back(LyricLine{t, text});
    }

    std::sort(out.begin(), out.end(),
              [](const LyricLine& a, const LyricLine& b) { return a.ms < b.ms; });
    out.erase(std::unique(out.begin(), out.end(),
                          [](const LyricLine& a, const LyricLine& b) { return a.ms == b.ms; }),
              out.end());
    return out;
}
