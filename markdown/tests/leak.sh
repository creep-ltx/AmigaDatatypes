#!/usr/bin/env bash
# leak.sh FILE [N] - open RAM:FILE in MultiView and close it N times (default 3)
# on the Amiga Wasabi points at (WASABI_HOST/WASABI_PORT), flushing memory
# between, and print what each cycle lost. 0 every time = no leak. Avail
# FLUSH also unloads markdown.datatype, so it is loaded fresh every cycle.
f=$1; n=${2:-3}
cyc(){ wasabi run "Avail FLUSH" | awk 'NR==4{print $2}'; }
for i in $(seq 1 "$n"); do
  b=$(cyc)
  wasabi run "Run >NIL: SYS:Utilities/MultiView RAM:$f" >/dev/null
  until wasabi windows | grep -q "$f"; do sleep 0.5; done; sleep 4
  a=$(wasabi ps | grep -i multiview | awk '{print $1}'); wasabi kill $a >/dev/null
  until ! wasabi windows | grep -q "$f"; do sleep 0.5; done; sleep 3
  c=$(cyc); echo "$f cycle $i lost $((b-c)) bytes"
done
