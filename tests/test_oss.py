import unittest
# from .context import oz

from enum import Enum
from copy import copy

from oz.game.flipguess import FlipGuess
from oz.game.kuhn import KuhnPoker

from oz import oss
from oz import best_response

import numpy.random


class TerminalHistory:
    class Player(Enum):
        Chance = 0

    def __init__(self):
        self.player = self.Player.Chance

    def is_terminal(self):
        return True

    def utility(self, player):
        return 1


class TestPlayoutSigma:
    def sample_pr(self, infoset):
        actions = infoset.actions
        tails = FlipGuess.Action.Tails
        left = FlipGuess.Action.Left
        if tails in actions:
            return tails, .5
        elif left in actions:
            return left, .5
        else:
            raise RuntimeError


class TestOSS(unittest.TestCase):

    def test_terminal(self):
        h = TerminalHistory()
        context = oss.Context()
        tree = oss.Tree()
        x, l, u = oss.oss(h, context, tree, 1, 1, 1, 1, 0)
        self.assertEqual(x, 1)

    def test_playout(self):
        h = FlipGuess()
        sigma = TestPlayoutSigma()
        s = 0.1
        x_target = .5**3
        x, l, u = oss.playout(h, s, sigma)
        self.assertEqual(x, x_target)
        self.assertEqual(l, s*x_target)
        self.assertEqual(u, 3)

    def test_flipguess(self):
        h = FlipGuess()
        context = oss.Context(seed=0)
        tree = oss.Tree()

        oss.solve(h, context, tree, n_iter=20000)
        self.assertEqual(len(tree.nodes), 2)

        node = tree.nodes[FlipGuess.PlayerInfoset(FlipGuess.Player.P2)]
        nl = node.average_strategy[FlipGuess.Action.Left]
        nr = node.average_strategy[FlipGuess.Action.Right]
        self.assertAlmostEqual(nl / (nl + nr), 1./3, places=2)

    def test_flipguess_exploitability(self):
        h = FlipGuess()
        context = oss.Context(seed=0)
        tree = oss.Tree()

        oss.solve(h, context, tree, n_iter=100)

        sigma = tree.sigma_average_strategy(rng=context.rng)
        ex1 = best_response.exploitability(h, sigma)

        oss.solve(h, context, tree, n_iter=1000)

        sigma = tree.sigma_average_strategy(rng=context.rng)
        ex2 = best_response.exploitability(h, sigma)

        self.assertLess(ex2, ex1)

    def test_kuhn(self):
        h = KuhnPoker()
        tree = oss.Tree()
        context = oss.Context()

        for i in range(10):
            oss.solve(h, context, tree, n_iter=1000)
            if len(tree.nodes) >= 12:
                sigma = tree.sigma_average_strategy(rng=context.rng)
                # ex = best_response.exploitability(h, sigma)
                # print('kuhn ex:', ex)

        self.assertEqual(len(tree.nodes), 12)

        ex = best_response.exploitability(h, sigma)
        self.assertLess(ex, 0.1)

        # for infoset in tree.nodes:
        #     print(infoset)
        #     for a in infoset.actions:
        #         print("\t{}: {}".format(a.value, sigma.pr(infoset, a)))

