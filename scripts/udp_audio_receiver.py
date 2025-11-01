#!/usr/bin/env -S uv run
# /// script
# requires-python = ">=3.10"
# dependencies = [
#     "numpy>=1.26",
#     "sounddevice>=0.4",
# ]
# ///
"""Listen for PCM audio frames from ESPHome's udp_audio_streamer component.

Examples
--------
$ scripts/udp_audio_receiver.py --port 7000 --sample-rate 16000 --channels 1 --bits 16
"""
from __future__ import annotations

import argparse
import socket
from contextlib import closing
from typing import Dict

import numpy as np
import sounddevice as sd


DTYPE_MAP: Dict[int, tuple[str, str]] = {
    16: ("<i2", "int16"),
    24: ("<i4", "int32"),  # 24-bit samples are packed into 32-bit integers
    32: ("<i4", "int32"),
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="0.0.0.0", help="IP to bind the UDP listener")
    parser.add_argument("--port", type=int, default=7000, help="UDP port to bind")
    parser.add_argument("--sample-rate", type=int, default=16000, help="Sample rate in Hz")
    parser.add_argument("--channels", type=int, default=1, choices=(1, 2), help="Channel count")
    parser.add_argument("--bits", type=int, default=16, choices=(16, 24, 32), help="Bits per sample")
    parser.add_argument(
        "--frame-bytes",
        type=int,
        default=0,
        help="Expected payload size; defaults to auto-detect on first packet",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    dtype = DTYPE_MAP.get(args.bits)
    if dtype is None:
        raise SystemExit(f"Unsupported bit depth: {args.bits}")
    buffer_dtype, stream_dtype = dtype

    with closing(socket.socket(socket.AF_INET, socket.SOCK_DGRAM)) as sock:
        sock.bind((args.host, args.port))
        sock.settimeout(None)
        print(f"Listening on {args.host}:{args.port} ({args.sample_rate} Hz, {args.channels} ch, {args.bits}-bit)")

        with sd.RawOutputStream(
            samplerate=args.sample_rate,
            channels=args.channels,
            dtype=stream_dtype,
        ) as stream:
            expected = args.frame_bytes
            try:
                while True:
                    payload, addr = sock.recvfrom(8192)
                    if not payload:
                        continue
                    if expected == 0:
                        expected = len(payload)
                        print(f"Detected frame size: {expected} bytes from {addr[0]}:{addr[1]}")
                    elif len(payload) != expected:
                        print(
                            f"Warning: expected {expected} bytes, received {len(payload)} — playing anyway",
                        )

                    if args.bits == 24:
                        samples = np.frombuffer(payload, dtype=buffer_dtype)
                        samples = (samples >> 8).astype(np.int32)
                        stream.write(samples.tobytes())
                    else:
                        stream.write(payload)
            except KeyboardInterrupt:
                print("\nStopping receiver")


if __name__ == "__main__":
    main()
