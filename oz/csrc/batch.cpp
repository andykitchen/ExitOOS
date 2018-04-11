#include <algorithm>
#include <iterator>
#include "batch.h"

namespace oz {

using namespace std;
using namespace at;

batch_search_t::batch_search_t(history_t root,
                               encoder_ptr_t encoder,
                               int batch_size) :
    root_(std::move(root)),
    encoder_(std::move(encoder)),
    batch_size_(batch_size)
{
  player_t search_player = P1;
  for (int i = 0; i < batch_size_; i++) {
    searches_.emplace_back(root_, search_player);
    search_player = search_player == P1 ? P2 : P1;
  }
}

auto batch_search_t::generate_batch() -> Tensor {
  Tensor d = CPU(kFloat).ones({ batch_size_, encoder_->encoding_size() });

  int playout_n = 0;
  for (auto &search : searches_) {
    if (search.state() == oos_t::search_t::state_t::PLAYOUT && search.history().player() != CHANCE) {
      int i = playout_n++;
      encoder_->encode(search.infoset(), d[i]);
    }
  }

  if (playout_n == 0) {
    return CPU(kFloat).ones(0);
  }
  else {
    return d.narrow(0, 0, playout_n);
  }
}

void batch_search_t::step(at::Tensor d, rng_t &rng) {
  int playout_n = 0;
  for (auto it = begin(searches_); it != end(searches_); ++it) {
    auto &search = *it;
    const auto &history = search.history();
    switch (search.state()) {
      case oos_t::search_t::state_t::SELECT:
        search.select(tree_, rng);
        break;
      case oos_t::search_t::state_t::CREATE:
        search.create(tree_, rng);
        break;
      case oos_t::search_t::state_t::PLAYOUT:
        if (history.player() == CHANCE) {
          auto ap = history.sample_chance(rng);
          search.playout_step(ap);
        }
        else {
          int i = playout_n++;
          assert (i < d.size(0));
          auto ap = encoder_->decode_and_sample(search.infoset(), d[i], rng);
          search.playout_step(ap);
        }
        break;
      case oos_t::search_t::state_t::BACKPROP:
        search.backprop(tree_);
        break;
      case oos_t::search_t::state_t::FINISHED:
        // TODO is there a better way to do this?
        auto last_player = search.search_player();
        search = oos_t::search_t(root_, last_player == P1 ? P2 : P1);
        break;
    }
  }
}

} // namespace oz