#!/usr/bin/env bash
# leak.sh FILE [N] - open RAM:FILE in MultiView and close it N times (default 3)
# on the Amiga Wasabi points at (WASABI_HOST/WASABI_PORT), flushing memory
# between, and print what each cycle lost. 0 every time = no leak. Avail
# FLUSH also unloads amiansi.datatype, so it is loaded fresh every cycle.
# Close every MultiView first: all of them are stopped at each cycle.
# (The window's title is the art's SAUCE title, so this waits for any
# MultiView window that is not "Loading...".)
f=$1; n=${2:-3}
cyc(){ wasabi run "Avail FLUSH" | awk 'NR==4{print $2}'; }
shown(){ wasabi windows | grep 'task SYS:Utilities/MultiView' | grep -qv 'Loading'; }
for i in $(seq 1 "$n"); do
  b=$(cyc)
  wasabi run "Run >NIL: SYS:Utilities/MultiView RAM:$f" >/dev/null
  until shown; do sleep 0.5; done; sleep 4
  for p in $(wasabi ps | grep -i multiview | awk '{print $1}'); do wasabi kill "$p" >/dev/null; done
  while wasabi windows | grep -q 'task SYS:Utilities/MultiView'; do sleep 0.5; done; sleep 3
  c=$(cyc); echo "$f cycle $i lost $((b-c)) bytes"
done
