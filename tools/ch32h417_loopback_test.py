"""
CH32H417 USBSS WinUSB bulk loopback smoke test.

Prerequisites:
  - Device must appear in Device Manager (WinUSB driver auto-installed via
    MS OS 2.0 descriptor, or installed manually with Zadig).
  - winusbpy must be importable:
      cd tools/winusbpy && pip install -e .

Usage:
  python tools/ch32h417_loopback_test.py [--count N] [--size B]

  If winusbpy discovery fails (device found by pnputil but not by the
  SetupDi GUID search), pass the interface path directly:
  python tools/ch32h417_loopback_test.py --path "\\\\?\\USB#VID_1209&PID_0001#123463#{dee824ef-729b-4a0e-9c14-b7117d33a817}"

Exit code 0 = all N round-trips passed.
"""

import sys
import os
import argparse

# Allow running from the repo root without installing the package.
_WINUSBPY_DIR = os.path.join(os.path.dirname(__file__), "winusbpy")
if _WINUSBPY_DIR not in sys.path:
    sys.path.insert(0, _WINUSBPY_DIR)

try:
    from winusbpy import WinUsbPy
except ImportError:
    print("ERROR: winusbpy not found. Run:  cd tools/winusbpy && pip install -e .")
    sys.exit(1)

VID = 0x1209
PID = 0x0001
EP_OUT = 0x01   # EP1 OUT host->device
EP_IN  = 0x81   # EP1 IN  device->host

READ_TIMEOUT_MS  = 2000
WRITE_TIMEOUT_MS = 1000


def open_device(device_path=None):
    """Return an open WinUsbPy handle or None on failure."""
    dev = WinUsbPy()
    dev.list_usb_devices(allclasses=True, present=True, deviceinterface=True)

    if device_path:
        # Inject the known path so init_winusb_device can find it by name.
        dev.device_paths[device_path] = device_path
        if dev.init_winusb_device(device_path, None, None):
            return dev
        print(f"ERROR: could not open device at path:\n  {device_path}")
        return None

    if dev.init_winusb_device("", VID, PID):
        return dev

    # Try again matching against the path string directly (some versions of
    # winusbpy key device_paths on the friendly name, hiding VID/PID).
    for key in dev.device_paths:
        path = dev.device_paths[key]
        if "1209" in path.lower() and "0001" in path.lower():
            dev.device_paths[path] = path
            if dev.init_winusb_device(path, None, None):
                return dev

    print(f"ERROR: device {VID:04X}:{PID:04X} not found.")
    print("  Check Device Manager -- WinUSB driver must be installed.")
    print("  If the device node is healthy but discovery fails, run:")
    print(r"    pnputil /enum-interfaces  (find the interface path)")
    print(r"    python tools\ch32h417_loopback_test.py --path <path>")
    return None


def loopback_test(count: int, size: int, device_path=None) -> bool:
    dev = open_device(device_path)
    if dev is None:
        return False

    print(f"Opened {VID:04X}:{PID:04X}  EP_OUT=0x{EP_OUT:02X}  EP_IN=0x{EP_IN:02X}")
    dev.set_timeout(EP_OUT, WRITE_TIMEOUT_MS)
    dev.set_timeout(EP_IN,  READ_TIMEOUT_MS)

    passed = 0
    failed = 0

    for i in range(count):
        payload = bytes((i * size + j) & 0xFF for j in range(size))

        written = dev.write(EP_OUT, payload)
        if written != len(payload):
            print(f"  [{i+1}/{count}] WRITE error: sent {written} of {len(payload)}")
            failed += 1
            continue

        read_data = dev.read(EP_IN, size)
        if read_data is None:
            print(f"  [{i+1}/{count}] READ timeout/error")
            failed += 1
            continue

        echo = bytes(read_data)
        if echo == payload:
            print(f"  [{i+1}/{count}] OK  {size} bytes echoed")
            passed += 1
        else:
            print(f"  [{i+1}/{count}] MISMATCH")
            print(f"    sent : {payload[:16].hex()} ...")
            print(f"    echo : {echo[:16].hex()} ...")
            failed += 1

    dev.close_winusb_device()
    print(f"\nResult: {passed}/{count} passed, {failed}/{count} failed.")
    return failed == 0


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="CH32H417 USBSS loopback smoke test")
    ap.add_argument("--count", type=int, default=5,  help="Round-trips (default 5)")
    ap.add_argument("--size",  type=int, default=64, help="Payload bytes (default 64)")
    ap.add_argument("--path",  type=str, default=None,
                    help="Device interface path (bypass SetupDi discovery)")
    args = ap.parse_args()

    ok = loopback_test(args.count, args.size, device_path=args.path)
    sys.exit(0 if ok else 1)
