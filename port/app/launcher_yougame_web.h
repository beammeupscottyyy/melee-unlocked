// YouGame's web session for the launcher: the player signs in on YouGame's own login page, shown in a
// browser view this launcher owns (Windows: WebView2, with its own data folder), and from then on the
// launcher's YouGame requests are made by a page of YouGame's site in that same view, as the site's own
// pages make them. The launcher never sees the password and holds no token of its own: the session is
// the browser view's, remembered in its folder across launches, and gone when the folder is removed.
// Linux: the equivalent is a WebKitGTK view (WebKitWebContext with its own data directory,
// webkit_web_view_evaluate_javascript for the page requests); not built yet, so these are stubs there.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <string>

namespace launcher::yougame::web {

struct Reply {
  int status = 0;      // the site's HTTP status; 0: the page could not ask (no view, no network, timeout)
  std::string body;
};

#if defined(_WIN32) && defined(_MSC_VER)
// The browser engine is on this computer (Microsoft Edge WebView2: part of Windows 11, on nearly every Windows 10).
bool supported();
// No WebView2 on this computer: downloads Microsoft's own installer (go.microsoft.com, the Evergreen bootstrapper),
// checks its Authenticode signature is valid and Microsoft's, runs it silently and waits for it. True once the
// runtime answers. Blocking (up to about 5 minutes); never on a window thread. *why: the step that failed.
bool install_runtime(std::string* why);
// Where the view keeps its session (one folder per YouGame site). Call before anything else.
void set_folder(const std::wstring& folder);
// A session folder exists: a sign-in made before may still hold.
bool remembered();
// Shows YouGame's sign-in page in a window owned by `owner` (an HWND, may be null) and returns once
// `signed_in_path` answers 200 from the page (true), or the player closed the window (false).
// Blocking; never call it on the thread that pumps `owner`.
// test_script: hidden test runs only, a script run on every page of the site while the window is open.
// The window is its own top-level window, never owned by `owner` (only placed over it): an owner on
// another thread would join the two threads' input, so a slow browser start would freeze the owner too.
// Until the page has loaded it reads "Loading YouGame..." on a light background.
bool sign_in(void* owner, const std::string& site, const std::string& signed_in_path, std::string* error,
             const std::string& test_script = {}, const std::wstring& title = L"Sign in to YouGame");
// The open sign-in window comes to the front (posted; never waits on the view's thread).
void raise_sign_in();
// One request from a page of `site` (same origin, with the page's session). Blocking, any thread.
Reply request(const std::string& site, const std::string& method, const std::string& path, const std::string& body,
              int timeout_ms = 20000);
// Ends the browser view; the session stays in its folder. Also done by itself when idle.
void close();
// Ends the view and removes its folder: signed out. Blocking (the folder frees up within seconds).
void sign_out();
#else
inline bool supported() { return false; }
inline bool install_runtime(std::string* why) { if (why) *why = "not on this system"; return false; }
inline void set_folder(const std::wstring&) {}
inline bool remembered() { return false; }
inline bool sign_in(void*, const std::string&, const std::string&, std::string* error, const std::string& = {},
                    const std::wstring& = {}) {
  if (error) *error = "YouGame's web sign-in is not available on this system yet.";
  return false;
}
inline void raise_sign_in() {}
inline Reply request(const std::string&, const std::string&, const std::string&, const std::string&, int = 20000) { return {}; }
inline void close() {}
inline void sign_out() {}
#endif

}  // namespace launcher::yougame::web
