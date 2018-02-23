import oz

kp = oz.KuhnPoker()

print(kp.player)
print(kp.legal_actions())

kp.act(oz.KuhnPoker.Action.QK)

print(kp.infoset(kp.player))
print(kp.player)
print(kp.legal_actions())

kp.act(oz.KuhnPoker.Action.Bet)

print(kp.infoset(kp.player))
print(kp.is_terminal())
print(kp.showdown)
print(kp.pot)
print(kp.folded)

print(kp.hand)
