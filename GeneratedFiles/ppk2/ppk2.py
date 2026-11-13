"""Nordic PPK2 current measurement from the command line (ampere-meter mode).

Nordic's Power Profiler GUI has no ARM Linux build, so this drives the PPK2
through ppk2-api (pip install ppk2-api) from the project venv:

    python ppk2.py measure 20            # power DUT, settle 2 s, measure 20 s
    python ppk2.py measure 20 --csv out.csv
    python ppk2.py hold                  # keep DUT powered (e.g. while flashing)

Wiring for the NUCLEO-WL55JC (STM32WL current only): remove JP1, PPK2 VIN to
the JP1 pin on the board 3V3 side, VOUT to the MCU side, GND to board GND.
If the mean reads about -1.6 uA while the board runs, VIN/VOUT are swapped.

PPK2 behaviors handled here:
- The data port is USB interface 1 (if01); ppk2-api's list_devices() can
  return the other interface first, which yields garbage.
- Sample bytes still in flight after a stop are misparsed as metadata, so the
  port is drained until quiet before reading calibration.
- The PPK2 turns DUT power off when its port is closed: the board is only
  powered while this script (measure or hold) is running.
- On this host about 84 of 100 kS/s arrive (blocks are dropped before Python);
  levels are unaffected, so use long captures for averages.
"""
import argparse
import signal
import statistics
import sys
import time

from ppk2_api.ppk2_api import PPK2_API

PORT = "/dev/serial/by-id/usb-Nordic_Semiconductor_PPK2_D606736FB724-if01"
RATE = 100_000


def drain(ppk2, quiet=0.3, cap=5.0):
    """Discard in-flight sample data until the port is quiet for `quiet` s."""
    ppk2.ser.timeout = 0.05
    t_end, last = time.time() + cap, time.time()
    while time.time() < t_end:
        if ppk2.ser.read(65536):
            last = time.time()
        elif time.time() - last > quiet:
            break
    ppk2.ser.timeout = 1


def open_powered(port):
    ppk2 = PPK2_API(port, timeout=1, write_timeout=1, exclusive=True)
    ppk2.stop_measuring()
    drain(ppk2)
    ppk2.get_modifiers()
    ppk2.use_ampere_meter()
    ppk2.set_source_voltage(3300)  # required by ppk2-api; not driven in ampere mode
    ppk2.toggle_DUT_power("ON")
    return ppk2


def record(ppk2, seconds):
    """Return (samples_uA, wall_seconds) for `seconds` of streaming."""
    ppk2.start_measuring()
    buf = bytearray()
    t_first = None
    deadline = time.time() + seconds + 1.0
    while time.time() < deadline:
        chunk = ppk2.get_data()
        if chunk:
            if t_first is None:
                t_first = time.time()
                deadline = t_first + seconds
            buf += chunk
        else:
            time.sleep(0.0005)
    span = time.time() - (t_first or time.time())
    ppk2.stop_measuring()
    samples, _ = ppk2.get_samples(bytes(buf))
    return samples, span


def summarize(samples, span):
    n = len(samples)
    s = sorted(samples)
    mean = statistics.fmean(samples)
    print(f"samples  {n} over {span:.1f} s ({n / span / 1e3:.1f} kS/s)")
    print(f"mean     {mean / 1000:8.3f} mA")
    print(f"median   {statistics.median(samples) / 1000:8.3f} mA")
    print(f"p1..p99  {s[n // 100] / 1000:8.3f} .. {s[-n // 100] / 1000:.3f} mA")
    if mean < 0:
        print("negative mean: PPK2 VIN and VOUT are probably swapped")
        return
    # Two-level summary from 10 ms bins (e.g. a blinking LED)
    k = RATE // 100
    bins = [statistics.fmean(samples[i:i + k]) for i in range(0, n - k + 1, k)]
    lo, hi = min(bins), max(bins)
    if hi - lo > 0.2 * hi:
        mid = (lo + hi) / 2
        low = [b for b in bins if b < mid]
        high = [b for b in bins if b >= mid]
        print(f"levels   low {statistics.fmean(low) / 1000:.3f} mA ({len(low)} bins), "
              f"high {statistics.fmean(high) / 1000:.3f} mA ({len(high)} bins)")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--port", default=PORT)
    sub = ap.add_subparsers(dest="cmd", required=True)
    m = sub.add_parser("measure", help="power the DUT and measure current")
    m.add_argument("seconds", type=float)
    m.add_argument("--settle", type=float, default=2.0, help="boot time before measuring")
    m.add_argument("--csv", help="write samples (uA, one per line)")
    sub.add_parser("hold", help="keep DUT power on until Ctrl+C / SIGTERM")
    args = ap.parse_args()

    ppk2 = open_powered(args.port)
    if args.cmd == "hold":
        print("DUT power ON; Ctrl+C to release", flush=True)
        signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            return

    time.sleep(args.settle)
    samples, span = record(ppk2, args.seconds)
    summarize(samples, span)
    if args.csv:
        with open(args.csv, "w") as f:
            f.write("\n".join(f"{v:.2f}" for v in samples) + "\n")


if __name__ == "__main__":
    main()
