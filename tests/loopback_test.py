#!/usr/bin/env python
from __future__ import print_function
import os
import sys
import time
import random

from hexdump import hexdump
from itertools import permutations

sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)), ".."))
from panda import Panda

def get_test_string():
  return b"test"+os.urandom(10)

def run_test():
  pandas = Panda.list()
  print(pandas)

  if len(pandas) == 0:
    print("NO PANDAS")
    assert False

  if len(pandas) == 1:
    # if we only have one on USB, assume the other is on wifi
    pandas.append("WIFI")
  run_test_w_pandas(pandas)

def run_test_w_pandas(pandas):
  h = list(map(lambda x: Panda(x), pandas))
  print("H", h)

  for hh in h:
    hh.set_controls_allowed(True)

  # test both directions
  for ho in permutations(range(len(h)), r=2):
    print("***************** TESTING", ho)

    # **** test health packet ****
    print("health", ho[0], h[ho[0]].health())

    # **** test K/L line loopback ****
    for bus in [2,3]:
      # flush the output
      h[ho[1]].kline_drain(bus=bus)

      # send the characters
      st = get_test_string()
      st = b"\xaa"+chr(len(st)+3).encode()+st
      h[ho[0]].kline_send(st, bus=bus, checksum=False)

      # check for receive
      ret = h[ho[1]].kline_drain(bus=bus)

      print("ST Data:")
      hexdump(st)
      print("RET Data:")
      hexdump(ret)
      assert st == ret
      print("K/L pass", bus, ho, "\n")

    # **** test can line loopback ****
    for bus in [0,1,4,5,6]:
      panda0 = h[ho[0]]
      panda1 = h[ho[1]]
      print("\ntest can", bus)
      # flush
      cans_echo = panda0.can_recv()
      cans_loop = panda1.can_recv()

      # set GMLAN mode
      if bus == 5:
        panda0.set_gmlan(True,2)
        panda1.set_gmlan(True,2)
        bus = 1    # GMLAN is multiplexed with CAN2
      elif bus == 6:
        # on REV B panda, this just retests CAN2 GMLAN
        panda0.set_gmlan(True,3)
        panda1.set_gmlan(True,3)
        bus = 4    # GMLAN is also multiplexed with CAN3
      else:
        panda0.set_gmlan(False)
        panda1.set_gmlan(False)

      # send the characters
      # pick addresses high enough to not conflict with honda code
      at = random.randint(1024, 2000)
      st = get_test_string()[0:8]
      panda0.can_send(at, st, bus)
      time.sleep(0.1)

      # check for receive
      cans_echo = panda0.can_recv()
      cans_loop = panda1.can_recv()

      print("Bus", bus, "echo", cans_echo, "loop", cans_loop)

      assert len(cans_echo) == 1
      assert len(cans_loop) == 1

      assert cans_echo[0][0] == at
      assert cans_loop[0][0] == at

      assert cans_echo[0][2] == st
      assert cans_loop[0][2] == st

      assert cans_echo[0][3] == bus+2
      if cans_loop[0][3] != bus:
        print("EXPECTED %d GOT %d" % (bus, cans_loop[0][3]))
      assert cans_loop[0][3] == bus

      print("CAN pass", bus, ho)

if __name__ == "__main__":
  if len(sys.argv) > 1:
    for i in range(int(sys.argv[1])):
      run_test()
  else :
    i = 0
    while True:
      print("************* testing %d" % i)
      run_test()
      i += 1
