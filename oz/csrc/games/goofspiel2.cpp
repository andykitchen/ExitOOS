#include "goofspiel2.h"

#include <iostream>
#include <sstream>

#include "hash.h"

namespace oz {

using namespace std;

goofspiel2_t::goofspiel2_t(int n_cards) :
  n_cards_(n_cards),
  turn_(0),
  player_(P1),
  score1_(0),
  score2_(0)
{
  Expects(n_cards < MAX_CARDS);

  for (card_t n = 0; n < n_cards; n++) {
    hand(P1).set(n);
    hand(P2).set(n);
  }
}

auto goofspiel2_t::infoset() const -> oz::infoset_t {
  Expects(player() != CHANCE);
  return make_infoset<infoset_t>(player_, hand(player_), bids(player_), wins_);
}

auto goofspiel2_t::utility(player_t p) const -> value_t {
  Expects(is_terminal());
  value_t u;

  if(score(P1) > score(P2)) {
    u = 1;
  }
  else if (score(P1) < score(P2)) {
    u = -1;
  }
  else {
    u = 0;
  }

  return relative_utility(p, u);
}

template <typename T>
bool contains(const set<T> &s, T x) {
  return s.find(x) != end(s);
}

void goofspiel2_t::act_(goofspiel2_t::action_t a) {
  Expects(player() != CHANCE);
  Expects(a < MAX_CARDS);

  auto card = static_cast<card_t>(a);

  Expects(hand(player_).test(card));

  if (player_ == P1) {
    P1_bid_ = card;
    hand(P1).set(P1_bid_, false);

    player_ = P2;
  }
  else { // player_ == P2
    card_t P2_bid = card;
    hand(P2).set(P2_bid, false);

    if (P2_bid < P1_bid_) {
      score(P1) += turn_;
      wins_.push_back(P1);
    }
    else if (P2_bid > P1_bid_) {
      score(P2) += turn_;
      wins_.push_back(P2);
    }
    else {
      // NB Chance player "winning" a bid means draw
      wins_.push_back(CHANCE);
    }

    bids(P1).push_back(P1_bid_);
    bids(P2).push_back(P2_bid);

    player_ = P1;
    turn_++;
  }
}

auto goofspiel2_t::chance_actions() const -> action_prob_map_t {
  Expects(false);
  return { };
}

auto goofspiel2_t::infoset_t::actions() const -> actions_list_t {
  actions_list_t actions;

  for(size_t card = 0; card < hand_.size(); card++) {
    if(hand_.test(card)) {
      actions.push_back(make_action(card));
    }
  }

  return actions;
}

auto goofspiel2_t::infoset_t::str() const -> string {
  stringstream ss;

  // TODO implement

  return ss.str();
}

bool goofspiel2_t::infoset_t::is_equal(const oz::infoset_t::concept_t& that)
const {
  if (typeid(*this) == typeid(that)) {
    auto that_ = static_cast<const goofspiel2_t::infoset_t&>(that);
    return player_ == that_.player_ &&
           bids_ == that_.bids_ &&
           wins_ == that_.wins_;
  }
  else {
    return false;
  }
}

size_t goofspiel2_t::infoset_t::hash() const {
  size_t seed = 0;
  hash_combine(seed, player_);
  for (const auto& bid : bids_) { hash_combine(seed, bid); }
  for (const auto& win : wins_) { hash_combine(seed, win); }
  return seed;
}

} // namespace oz
