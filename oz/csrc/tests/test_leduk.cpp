#include <catch.hpp>

#include "game.h"
#include "games/leduk.h"

using namespace oz;

TEST_CASE("leduk poker basic actions", "[leduk]") {
  leduk_poker_t g;

  CHECK(g.player() == CHANCE);
  g.act_(leduk_poker_t::action_t::J1);
  CHECK(g.player() == CHANCE);
  g.act_(leduk_poker_t::action_t::Q2);
  CHECK(g.player() == P1);
  g.act_(leduk_poker_t::action_t::Raise);
  CHECK(g.player() == P2);
  g.act_(leduk_poker_t::action_t::Call);
  CHECK(g.player() == CHANCE);
}

leduk_poker_t _leduk_after_deal() {
  auto g = leduk_poker_t();
  g.act_(leduk_poker_t::action_t::J1);
  g.act_(leduk_poker_t::action_t::Q2);
  return g;
}

TEST_CASE("leduk poker bet", "[leduk]") {
  leduk_poker_t g = _leduk_after_deal();

  g.act_(leduk_poker_t::action_t::Raise);
  g.act_(leduk_poker_t::action_t::Call);

  CHECK(g.pot(P1) == 3);
  CHECK(g.pot(P2) == 3);
  CHECK(g.round() == 1);
}

TEST_CASE("leduk poker check", "[leduk]") {
  leduk_poker_t g = _leduk_after_deal();

  g.act_(leduk_poker_t::action_t::Call);
  g.act_(leduk_poker_t::action_t::Call);

  CHECK(g.pot(P1) == 1);
  CHECK(g.pot(P2) == 1);
  CHECK(g.round() == 1);
  CHECK(g.board() == leduk_poker_t::card_t::NA);
}

TEST_CASE("leduk poker fold", "[leduk]") {
  leduk_poker_t g = _leduk_after_deal();

  g.act_(leduk_poker_t::action_t::Raise);
  g.act_(leduk_poker_t::action_t::Fold);

  CHECK(g.pot(P1) == 3);
  CHECK(g.pot(P2) == 1);
  CHECK(g.is_terminal());
  CHECK(g.utility(P1) == 1);
}

TEST_CASE("leduk poker reraise", "[leduk]") {
  leduk_poker_t g = _leduk_after_deal();

  g.act_(leduk_poker_t::action_t::Raise);
  g.act_(leduk_poker_t::action_t::Raise);

  // TODO
}

TEST_CASE("leduk poker bet rounds", "[leduk]") {
  leduk_poker_t g = _leduk_after_deal();

  g.act_(leduk_poker_t::action_t::Raise);
  g.act_(leduk_poker_t::action_t::Raise);
  g.act_(leduk_poker_t::action_t::Call);
  CHECK(g.player() == CHANCE);
  g.act_(leduk_poker_t::action_t::K);
  g.act_(leduk_poker_t::action_t::Raise);
  g.act_(leduk_poker_t::action_t::Raise);
  g.act_(leduk_poker_t::action_t::Call);

  CHECK(g.pot(P1) == 13);
  CHECK(g.pot(P2) == 13);
  CHECK(g.is_terminal());
}

TEST_CASE("leduk poker bet reward", "[leduk]") {
  leduk_poker_t g = leduk_poker_t();

  g.act_(leduk_poker_t::action_t::K1);
  g.act_(leduk_poker_t::action_t::Q2);
  g.act_(leduk_poker_t::action_t::Raise);
  g.act_(leduk_poker_t::action_t::Call);
  g.act_(leduk_poker_t::action_t::J);
  g.act_(leduk_poker_t::action_t::Raise);
  g.act_(leduk_poker_t::action_t::Call);

  CHECK(g.hand(P1) == leduk_poker_t::card_t::King);
  CHECK(g.hand(P2) == leduk_poker_t::card_t::Queen);
  CHECK(g.board() == leduk_poker_t::card_t::Jack);
  CHECK(g.is_terminal());
  CHECK(g.utility(P1) == 7);

  g = leduk_poker_t();

  g.act_(leduk_poker_t::action_t::K1);
  g.act_(leduk_poker_t::action_t::Q2);
  g.act_(leduk_poker_t::action_t::Raise);
  g.act_(leduk_poker_t::action_t::Raise);
  g.act_(leduk_poker_t::action_t::Call);
  g.act_(leduk_poker_t::action_t::Q);
  g.act_(leduk_poker_t::action_t::Raise);
  g.act_(leduk_poker_t::action_t::Raise);
  g.act_(leduk_poker_t::action_t::Call);

  CHECK(g.hand(P1) == leduk_poker_t::card_t::King);
  CHECK(g.hand(P2) == leduk_poker_t::card_t::Queen);
  CHECK(g.board() == leduk_poker_t::card_t::Queen);
  CHECK(g.is_terminal());
  CHECK(g.utility(P1) == -13);

  g.act_(leduk_poker_t::action_t::Q1);
  g.act_(leduk_poker_t::action_t::Q2);
  g.act_(leduk_poker_t::action_t::Raise);
  g.act_(leduk_poker_t::action_t::Call);
  g.act_(leduk_poker_t::action_t::K);
  g.act_(leduk_poker_t::action_t::Raise);
  g.act_(leduk_poker_t::action_t::Call);

  CHECK(g.hand(P1) == leduk_poker_t::card_t::Queen);
  CHECK(g.hand(P2) == leduk_poker_t::card_t::Queen);
  CHECK(g.board() == leduk_poker_t::card_t::King);
  CHECK(g.is_terminal());
  CHECK(g.utility(P1) == 0);
}