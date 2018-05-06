#include "util.h"
#include "hash.h"

#include "oos.h"

#include <cassert>
#include <algorithm>
#include <iterator>
#include <set>

#include <boost/container/small_vector.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>
#include <boost/container/pmr/monotonic_buffer_resource.hpp>

namespace oz {

using namespace std;

static constexpr int N_ACTIONS_SMALL = 16;
using action_vector = boost::container::small_vector<action_t, N_ACTIONS_SMALL>;
using prob_vector = boost::container::small_vector<prob_t, N_ACTIONS_SMALL>;

using boost::container::pmr::monotonic_buffer_resource;
using boost::container::pmr::polymorphic_allocator;

static action_prob_t sample_chance(const history_t &history, rng_t& rng,
                                   game_t::action_prob_allocator_t alloc);

void oos_t::search_t::prepare_suffix_probs() {
  Expects(suffix_prob_.x > 0);

  prob_t s1 = prefix_prob_.s1 * suffix_prob_.x;
  prob_t s2 = prefix_prob_.s2 * suffix_prob_.x;

  suffix_prob_.l = delta_*s1 + (1.0 - delta_)*s2;
  suffix_prob_.u = history_.utility(search_player_);

  Ensures(suffix_prob_.l > 0);
}

void oos_t::search_t::tree_step(action_prob_t ap) {
  const auto acting_player = history_.player();
  const auto alloc = get_allocator();

  const auto infoset = (acting_player != CHANCE) ?
                       history_.infoset(alloc) :
                       null_infoset();

  tree_step(ap, infoset);
}

static const bool is_normal(prob_t x) {
    switch(std::fpclassify(x)) {
        case FP_INFINITE:  return false;
        case FP_NAN:       return false;
        case FP_NORMAL:    return true;
        case FP_SUBNORMAL: return false;
        case FP_ZERO:      return true;
        default:           return false;
    }
}

void oos_t::search_t::tree_step(action_prob_t ap, const infoset_t &infoset) {
  Expects(state_ == state_t::SELECT || state_ == state_t::CREATE);
  Expects(!history_.is_terminal());

  // TODO does a zero pr_a make sense here?
  Expects(0 <= ap.pr_a && ap.pr_a <= 1);
  Expects(0 <  ap.rho1 && ap.rho1 <= 1);
  Expects(0 <  ap.rho2 && ap.rho2 <= 1);

  Expects(is_normal(ap.pr_a));
  Expects(is_normal(ap.rho1));
  Expects(is_normal(ap.rho1));

  const auto acting_player = history_.player();

  path_.emplace_back(path_item_t {
      acting_player, infoset,
      ap, prefix_prob_
  });

  // update prefix and sample probabilities
  if(acting_player == search_player_) {
    prefix_prob_.pi_i *= ap.pr_a;
  }
  else {
    prefix_prob_.pi_o *= ap.pr_a;
  }

  prefix_prob_.s1 *= ap.rho1;
  prefix_prob_.s2 *= ap.rho2;

  // move game forward one ply
  history_.act(ap.a);
}

auto oos_t::search_t::sample_tree(const tree_t &tree,
                                  const infoset_t &infoset,
                                  rng_t &rng) const
  -> tree_t::sample_ret_t
{
  const auto acting_player = history_.player();

  prob_t eps, gamma;
  if (acting_player == search_player_) {
    eps = eps_;
    gamma = 0;
  }
  else {
    eps = 0;
    gamma = gamma_;
  }

  // TODO cleanup this check is a bit fat
  const auto targets = (target_ && targeted_ && target_infoset_) ?
                       target_.target_actions(target_infoset_, history_) :
                       set<action_t> { };

  const auto r = tree.sample_sigma(infoset,
                                   targets, targeted_,
                                   eps, gamma, eta_,
                                   rng);

  return r;
}

void oos_t::search_t::select(const tree_t& tree, rng_t &rng) {
  Expects(state_ == state_t::SELECT);
  const auto alloc = get_allocator();

  auto d = uniform_real_distribution<>();
  const auto u = d(rng);
  targeted_ = (u < delta_);

  while (state_ == state_t::SELECT) {
    if (history_.is_terminal()) {
      prepare_suffix_probs();
      state_ = state_t::BACKPROP;
    }
    else if (history_.player() == CHANCE) {
      const auto ap = sample_chance(history_, rng, alloc);
      tree_step(ap);
    }
    else {
      const auto infoset = history_.infoset(alloc);
      const auto r = sample_tree(tree, infoset, rng);

      if (r.out_of_tree) {
        state_ = state_t::CREATE;
      }
      else {
        tree_step(r.ap, infoset);
      }
    }
  }

  Ensures(state_ == state_t::CREATE || state_ == state_t::BACKPROP);
}

void oos_t::search_t::create(tree_t& tree, rng_t &rng) {
  Expects(history_.player() != CHANCE);
  Expects(!history_.is_terminal());

  const auto infoset = history_.infoset();
  auto node = node_t(infoset.actions());

  insert_node_step(tree, infoset, node, rng);
}

void oos_t::search_t::create_prior(tree_t &tree,
                                   node_t::avg_map_t average_strategy,
                                   rng_t &rng)
{
  Expects(history_.player() != CHANCE);
  Expects(!history_.is_terminal());

  const auto infoset = history_.infoset();

  auto node = node_t(infoset.actions());

  node.average_strategy_ = average_strategy;
  node.prior_ = move(average_strategy);

  insert_node_step(tree, infoset, node, rng);
}

void oos_t::search_t::insert_node_step(tree_t &tree,
                                       const infoset_t &infoset,
                                       const node_t &node,
                                       rng_t &rng)
{
  Expects(state_ == state_t::CREATE);

  auto &nodes = tree.nodes();
  nodes.emplace(infoset, node);

  const auto r = sample_tree(tree, infoset, rng);
  Expects(!r.out_of_tree);

  tree_step(r.ap, infoset);

  if (history_.is_terminal()) {
    prepare_suffix_probs();
    state_ = state_t::BACKPROP;
  }
  else {
    state_ = state_t::PLAYOUT;
  }

  Ensures(state_ == state_t::PLAYOUT || state_ == state_t::BACKPROP);
}

void oos_t::search_t::playout_step(action_prob_t ap) {
  Expects(state_ == state_t::PLAYOUT);

  history_.act(ap.a);
  suffix_prob_.x *= ap.pr_a;

  if (history_.is_terminal()) {
    prepare_suffix_probs();
    state_ = state_t::BACKPROP;
  }

  Ensures(state_ == state_t::PLAYOUT || state_ == state_t::BACKPROP);
}

void oos_t::search_t::backprop(tree_t& tree) {
  Expects(state_ == state_t::BACKPROP);
  Expects(history_.is_terminal());

  prob_t c;
  prob_t x = suffix_prob_.x;

  const prob_t  l = suffix_prob_.l;
  const value_t u = suffix_prob_.u;

  for (auto i = rbegin(path_); i != rend(path_); ++i) {
    const auto& path_item = *i;
    const auto& acting_player = path_item.player;
    const auto& infoset = path_item.infoset;

    const auto a = path_item.action_prob.a;
    const auto pr_a = path_item.action_prob.pr_a;

    const auto pi_o = path_item.prefix_prob.pi_o;
    const auto s1 = path_item.prefix_prob.s1;
    const auto s2 = path_item.prefix_prob.s2;

    c = x;
    x = pr_a * x;

    if (acting_player == CHANCE) { // TODO make this more elegant
      continue;
    }

    auto &node = tree.lookup(infoset);

    if (acting_player == search_player_) {
      const value_t w = u * pi_o / l;
      for (const auto& a_prime : infoset.actions()) {
        value_t r;
        if (a_prime == a) {
          r = (c - x) * w;
        }
        else {
          r = -x * w;
        }

        node.regret(a_prime) += r;
      }

      node.regret_n() += 1;
    }
    else {
      const auto sigma = node.sigma_regret_matching();

      const prob_t q = delta_*s1 + (1 - delta_)*s2;
      for (const auto& a_prime : infoset.actions()) {
        const prob_t s = (pi_o * sigma.pr(infoset, a_prime)) / q;
        node.average_strategy(a_prime) += s;
      }
    }
  }

  state_ = state_t::FINISHED;
}

prob_t oos_t::search_t::targeting_ratio() const {
  auto r = prefix_prob_.s2 / prefix_prob_.s1;
  Ensures(r > 0);
  Ensures(is_normal(r));
  return r;
};

void oos_t::search_t::set_initial_weight(prob_t w) {
  Expects(0 < w);
  Expects(is_normal(w));

  prefix_prob_.s1 = w;
  prefix_prob_.s2 = w;
}

auto oos_t::search_t::get_allocator() const -> allocator_type {
  return path_.get_allocator().resource();
}

auto sigma_t::concept_t::sample_pr(infoset_t infoset, rng_t& rng) const
  -> action_prob_t
{
  auto actions = infoset.actions();
  auto probs = prob_vector(actions.size());

  transform(begin(actions), end(actions), begin(probs),
            [&](const auto& a) { return pr(infoset, a); });

  auto a_dist = discrete_distribution<>(begin(probs), end(probs));
  auto i = a_dist(rng);

  auto a = actions[i];
  auto pr_a = probs[i];
  auto rho1 = pr_a;
  auto rho2 = pr_a;

  return { a, pr_a, rho1, rho2 };
};

// TODO remove this function
auto sigma_t::sample_eps(infoset_t infoset, prob_t eps, rng_t &rng) const
  -> action_prob_t
{
  auto d_eps = uniform_real_distribution<>();
  const prob_t u = d_eps(rng);

  const auto actions = infoset.actions();
  Expects(!actions.empty());

  const auto N = static_cast<int>(actions.size());
  const auto p = (prob_t) 1/N;
  Expects(N > 0);

  if (u <= eps) {
    auto d_a = uniform_int_distribution<>(0, N-1);
    const auto i = d_a(rng);
    const auto a = actions[i];

    prob_t pr_a = pr(infoset, a);
    prob_t rho1 = eps*p + (1 - eps)*pr_a;
    prob_t rho2 = rho1;
    return { a, pr_a, rho1, rho2 };
  }
  else {
    auto ap = sample_pr(infoset, rng);
    ap.rho1 = eps*p + (1 - eps)*ap.pr_a;
    ap.rho2 = ap.rho1;
    return ap;
  }
};

// TODO break up this function
static auto sample_targeted(sigma_regret_t sigma,
                            const infoset_t &infoset,
                            const node_t &node,
                            const set<action_t> &targets, bool targeted,
                            prob_t eps, prob_t gamma, prob_t eta,
                            rng_t &rng) -> action_prob_t
{
  const auto actions = infoset.actions();

  Expects(!actions.empty());

  const auto N = static_cast<int>(actions.size());
  const auto p_eps = (prob_t) 1/N;
  const auto p_gamma = (prob_t) 1/N;

  // The difference between gamma and epsilon here is that
  // epsilon is extrinsic exploration that is accounted for by
  // importance weights (e.g. appears as a factor in rho)
  // gamma is considered part of the opponent strategy and models
  // a slightly fallible opponent that "slips" and plays a random
  // move with gamma probability.

  // raw action probabilities (with gamma "mistake" model)
  auto probs = prob_vector { };
  transform(begin(actions), end(actions), back_inserter(probs),
            [&](const action_t &a) -> prob_t {
              auto p_sigma = sigma.pr(infoset, a);
              auto p_prior = node.prior(a);

              auto p0 = gamma*p_gamma + (1 - gamma)*p_sigma;
              auto p1 = eta*p0 + (1 - eta)*p_prior;

              return p1;
            });

  // epsilon exploration probabilities
  auto probs_untargeted = prob_vector { };
  transform(begin(probs), end(probs), back_inserter(probs_untargeted),
            [&](const prob_t &p) -> prob_t {
              return eps*p_eps + (1 - eps)*p;
            });

  // targeted action weights (unscaled)
  auto weights_targeted = prob_vector { };
  if (targets.empty()) {
    weights_targeted = probs_untargeted;
  }
  else {
    transform(begin(actions), end(actions),
              begin(probs_untargeted), back_inserter(weights_targeted),
              [&](const action_t &a, const prob_t &p) -> prob_t {
                if(targets.find(a) != end(targets)) {
                  return p;
                }
                else {
                  return 0;
                }
              });
  }

  const auto total_weight = accumulate(begin(weights_targeted),
                                       end(weights_targeted),
                                       (prob_t) 0);

  // NB if the targeted weights total is zero, we have tried to target a
  // subgame that has zero probability. In this case we bail out
  // and proceed by sampling in an untargeted way.
  // We also need to twiddle the rho1 probability in the case :/
  const auto &sample_probs = (total_weight > 0) ?
                             weights_targeted :
                             probs_untargeted;

  auto a_dist = discrete_distribution<>(begin(sample_probs),
                                        end(sample_probs));
  const auto i = a_dist(rng);

  const auto a = actions[i];
  const auto pr_a = probs[i];

  const auto pr_targeted   = weights_targeted[i] / total_weight;
  const auto pr_untargeted = probs_untargeted[i];

  const auto rho1 = (total_weight > 0) ? pr_targeted : pr_untargeted;
  const auto rho2 = pr_untargeted;

  // TODO does it makes sense for pr_a to be zero?
  Ensures(0 <= pr_a && pr_a <= 1);
  Ensures(0 <  rho1 && rho1 <= 1);
  Ensures(0 <  rho2 && rho2 <= 1);
  Ensures(!targeted || rho2 - rho1 < 1e-6);

  return { a, pr_a, rho1, rho2 };
}

node_t::node_t(infoset_t::actions_list_t actions) {
  Expects(!actions.empty());

  static const auto make_zero_value =
      [](const action_t &a) { return make_pair(a, 0); };

  regrets_.reserve(actions.size());
  average_strategy_.reserve(actions.size());
  prior_.reserve(actions.size());

  transform(begin(actions), end(actions),
            inserter(regrets_, end(regrets_)),
            make_zero_value);

  transform(begin(actions), end(actions),
            inserter(average_strategy_, end(average_strategy_)),
            make_zero_value);

  transform(begin(actions), end(actions),
            inserter(prior_, end(prior_)),
            make_zero_value);

  Ensures(!regrets_.empty());
  Ensures(!average_strategy_.empty());
  Ensures(!prior_.empty());
}

static auto sample_chance(const history_t &history, rng_t& rng,
                          game_t::action_prob_allocator_t alloc)
  -> action_prob_t
{
  const auto actions_pr = history.chance_actions(alloc);
  Expects(!actions_pr.empty());

  auto actions = action_vector { };
  auto probs = prob_vector { };

  transform(begin(actions_pr), end(actions_pr), back_inserter(actions),
            [](const pair<action_t, prob_t> &x) -> action_t {
              return x.first;
            });

  transform(begin(actions_pr), end(actions_pr), back_inserter(probs),
            [](const pair<action_t, prob_t> &x) -> prob_t {
              return x.second;
            });

  auto a_dist = discrete_distribution<>(begin(probs), end(probs));
  auto i = a_dist(rng);

  auto a = actions[i];
  auto pr_a = probs[i];
  auto rho1 = pr_a;
  auto rho2 = pr_a;
  Ensures(0 <= pr_a && pr_a <= 1);

  return { a, pr_a, rho1, rho2 };
}

static auto sample_chance(const history_t &history, rng_t& rng)
  -> action_prob_t {
  return sample_chance(history, rng, { });
}

auto history_t::sample_chance(oz::rng_t &rng) const -> action_prob_t {
  return oz::sample_chance(*this, rng);
}

static auto sample_uniform(const history_t &history, rng_t &rng)
  -> action_prob_t
{
  auto actions = history.infoset().actions();
  auto N = static_cast<int>(actions.size());
  Expects(N > 0);

  auto d = uniform_int_distribution<>(0, N-1);
  auto i = d(rng);
  auto a = actions[i];
  auto pr_a = (prob_t) 1/N;
  Ensures(0 <= pr_a && pr_a <= 1);

  return { a, pr_a, pr_a, pr_a };
}

void tree_t::create_node(infoset_t infoset) {
  nodes_.emplace(infoset, node_t(infoset.actions()));
}

auto tree_t::sample_sigma(const infoset_t &infoset,
                          const set<action_t> &targets,
                          bool targeted,
                          prob_t eps, prob_t gamma, prob_t eta,
                          rng_t &rng) const
  -> tree_t::sample_ret_t
{
  const auto it = nodes_.find(infoset);

  if (it == end(nodes_)) {
    return { { }, true };
  }
  else {
    const auto &node = lookup(infoset);
    const auto sigma = node.sigma_regret_matching();

    // const auto ap = sigma.sample_eps(infoset, eps, rng);
    const auto ap = sample_targeted(sigma, infoset, node,
                                    targets, targeted,
                                    eps, gamma, eta,
                                    rng);

    return { ap, false };
  }
}

auto tree_t::sigma_average() const -> sigma_t {
  return make_sigma<sigma_average_t>(*this);
}

void tree_t::clear() {
  nodes_.clear();
}

auto sigma_regret_t::pr(infoset_t infoset, action_t a) const -> prob_t {
  auto sum_positive = accumulate(
    begin(regrets_), end(regrets_), (value_t) 0,
    [](const auto &r, const auto &x) {
        return r + max<value_t>(0, x.second);
    });

  prob_t p;
  if (sum_positive > 0) {
    auto r = regrets_.at(a);
    p = r > 0 ? r/sum_positive : 0;
  }
  else {
    p = (prob_t) 1/infoset.actions().size();
  }

  Ensures(0 <= p && p <= 1);
  return p;
}

auto sigma_regret_t::sample_pr(infoset_t infoset, rng_t &rng) const
  -> action_prob_t
{
  auto actions = action_vector { };
  auto weights = prob_vector { };

  transform(begin(regrets_), end(regrets_), back_inserter(actions),
            [](const auto &x) { return x.first; });

  transform(begin(regrets_), end(regrets_), back_inserter(weights),
            [](const auto &x) { return max<value_t>(0, x.second); });

  auto total = accumulate(begin(weights), end(weights), (prob_t) 0);
  auto N = static_cast<int>(weights.size());

  Expects(N > 0);
  Expects(!actions.empty());
  Expects(!weights.empty());
  Expects(actions.size() == weights.size());

  int i;
  if (total > 0) {
    auto a_dist = discrete_distribution<>(begin(weights), end(weights));
    i = a_dist(rng);
  }
  else {
    auto a_dist = uniform_int_distribution<>(0, N-1);
    i = a_dist(rng);
  }

  auto a = actions[i];
  auto pr_a = total > 0 ? weights[i]/total : (prob_t) 1/N;
  auto rho1 = pr_a, rho2 = pr_a;

  Ensures(0 <= pr_a && pr_a <= 1);

  return { a, pr_a, rho1, rho2 };
}

auto sigma_average_t::pr(infoset_t infoset, action_t a) const -> prob_t {
  const auto actions = infoset.actions();
  const auto N = static_cast<int>(actions.size());

  const auto &nodes = tree_.nodes();
  const auto it = nodes.find(infoset);

  prob_t p;
  if (it != end(nodes)) {
    const auto &node = it->second;

    const auto total =
        accumulate(begin(actions), end(actions), (prob_t) 0,
                   [&node](const auto &x, const auto &a_prime) {
                     return x + node.average_strategy(a_prime);
                   });

    p = total > 0 ?
      node.average_strategy(a) / total :
      (prob_t) 1/N;
  }
  else {
    p = (prob_t) 1/N;
  }

  Ensures(0 <= p && p <= 1);
  return p;
};

static inline auto sample_action(const history_t &h, rng_t &rng)
  -> action_prob_t
{
  if (h.player() == CHANCE) {
    return sample_chance(h, rng);
  }
  else {
    return sample_uniform(h, rng);
  }
}

void oos_t::search_iter(history_t h, player_t player,
                        tree_t &tree, rng_t &rng,
                        target_t target,
                        infoset_t target_infoset,
                        void *buffer, size_t buffer_size,
                        const prob_t eps,
                        const prob_t delta,
                        const prob_t gamma,
                        const prob_t beta)
{
  using state_t = search_t::state_t;

  monotonic_buffer_resource buf_rsrc(buffer, buffer_size);

  search_t s(move(h), player,
             move(target), move(target_infoset),
             &buf_rsrc,
             eps, delta, gamma);

  s.set_initial_weight(1.0/avg_targeting_ratio_);

  while (s.state() != state_t::FINISHED) {
    switch (s.state()) {
      case state_t::SELECT:
        s.select(tree, rng);
        break;
      case state_t::CREATE:
        s.create(tree, rng);
        break;
      case state_t::PLAYOUT:
        s.playout_step(sample_action(s.history(), rng));
        break;
      case state_t::BACKPROP:
        s.backprop(tree);
        break;
      case state_t::FINISHED:
        break;
    }
  }

  avg_targeting_ratio_ = beta*avg_targeting_ratio_ + (1-beta)*s.targeting_ratio();
}

static constexpr int WORK_BUFFER_SIZE = (2 << 20);

static unique_ptr<uint8_t[]> make_work_buffer(size_t size) {
  unique_ptr<uint8_t[]> ptr(new uint8_t[size]);
  return ptr;
}

void oos_t::search_targeted(history_t h, int n_iter, tree_t &tree, rng_t &rng,
                   target_t target, infoset_t target_infoset,
                   const prob_t eps,
                   const prob_t delta,
                   const prob_t gamma,
                   const prob_t beta)
{
  Expects(n_iter >= 0);

  auto ptr = make_work_buffer(WORK_BUFFER_SIZE);

  for(int i = 0; i < n_iter; i++) {
    search_iter(h, P1, tree, rng,
                target, target_infoset,
                ptr.get(), WORK_BUFFER_SIZE,
                eps, delta, gamma, beta);

    search_iter(h, P2, tree, rng,
                target, target_infoset,
                ptr.get(), WORK_BUFFER_SIZE,
                eps, delta, gamma, beta);
  }
}

void oos_t::search(history_t h, int n_iter, tree_t &tree, rng_t &rng,
                   const prob_t eps,
                   const prob_t delta,
                   const prob_t gamma,
                   const prob_t beta)
{
  search_targeted(move(h), n_iter, tree, rng,
                  null_target(), null_infoset(),
                  eps, delta, gamma, beta);
}

}
