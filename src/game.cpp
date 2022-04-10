#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <iostream>
#include <smk/Audio.hpp>
#include <thread>
#include "board.hpp"
#include "intro.hpp"
#include "resources.hpp"

namespace term_breaker {

void StartGame(bool enable_audio) {
  (void)enable_audio;
  using namespace ftxui;
  smk::Audio audio;  // Initialize OpenAL.

  if (enable_audio)
    term_breaker::LoadResources();

  BoardConfig config;
  config.balls = 10;  // NOLINT
  Board board(config);

  // A triangle following the mouse, using braille characters.
  auto renderer = Renderer([&] {
    auto c = Canvas(150, 150);  // NOLINT
    board.Draw(c);
    return canvas(std::move(c)) | border | bold;
  });

  renderer |= CatchEvent([&](Event event) {  // NOLINT
    if (event == Event::Custom) {
      board.Step();
      return true;
    }

    return board.OnEvent(event);
  });

  auto screen = ScreenInteractive::FitComponent();
  // This thread exists to make sure that the event queue has an event to
  // process at approximately a rate of 30 FPS
  std::atomic<bool> refresh_ui_continue = true;
  std::thread refresh_ui([&] {
    while (refresh_ui_continue) {
      using namespace std::chrono_literals;
      std::this_thread::sleep_for(1.0s / 60.0);  // NOLINT magic numbers
      screen.PostEvent(ftxui::Event::Custom);
    }
  });
  screen.Loop(renderer);

  refresh_ui_continue = false;
  refresh_ui.join();

  term_breaker::UnloadResources();
}
}  // namespace term_breaker

// Copyright 2022 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.