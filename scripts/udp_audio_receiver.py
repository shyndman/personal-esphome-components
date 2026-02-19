#!/usr/bin/env -S uv run
# /// script
# requires-python = ">=3.10"
# dependencies = [
#     "numpy>=1.26",
#     "sounddevice>=0.4",
# ]
# ///
"""Listen for PCM audio frames from ESPHome's udp_audio_streamer component with jitter buffering."""
from __future__ import annotations

import argparse
import queue
import socket
import threading
import time
from contextlib import closing
from typing import Dict, Tuple

import numpy as np
import sounddevice as sd


DTYPE_MAP: Dict[int, Tuple[str, np.dtype]] = {
    16: ("<i2", np.int16),
    24: ("<i4", np.int32),  # 24-bit samples are packed into 32-bit integers
    32: ("<i4", np.int32),
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="0.0.0.0", help="IP to bind the UDP listener")
    parser.add_argument("--port", type=int, default=7000, help="UDP port to bind")
    parser.add_argument("--sample-rate", type=int, default=16000, help="Sample rate in Hz")
    parser.add_argument("--channels", type=int, default=1, choices=(1, 2), help="Channel count")
    parser.add_argument("--bits", type=int, default=16, choices=(16, 24, 32), help="Bits per sample")
    parser.add_argument(
        "--queue-len", type=int, default=64, help="Number of packets to buffer before playback"
    )
    parser.add_argument(
        "--prefill", type=float, default=0.5,
        help="Fraction of the queue to fill before starting playback (0.0-1.0)"
    )
    parser.add_argument(
        "--record", type=str, default=None,
        help="Optional path to save audio as a WAV file"
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    dtype_info = DTYPE_MAP.get(args.bits)
    if dtype_info is None:
        raise SystemExit(f"Unsupported bit depth: {args.bits}")
    buffer_dtype, stream_dtype = dtype_info
    bytes_per_sample = 4 if args.bits == 24 else args.bits // 8

    queue_capacity = max(4, args.queue_len)
    pkt_queue: queue.Queue[np.ndarray] = queue.Queue(maxsize=queue_capacity)
    packet_stats = {"bytes": 0, "packets": 0, "last_log": time.monotonic(), "underruns": 0}
    block_frames = None
    stop_event = threading.Event()

    def udp_reader(sock: socket.socket) -> None:
        nonlocal block_frames
        try:
            while not stop_event.is_set():
                payload, addr = sock.recvfrom(65535)
                if not payload:
                    continue

                if block_frames is None:
                    samples = len(payload) // (bytes_per_sample * args.channels)
                    if samples == 0:
                        print("Received undersized packet; skipping")
                        continue
                    block_frames = samples
                    print(
                        f"Detected frame: {len(payload)} bytes ({samples} samples) from {addr[0]}:{addr[1]}"
                    )

                if args.bits == 24:
                    data = np.frombuffer(payload, dtype=buffer_dtype, count=block_frames * args.channels)
                    data = (data >> 8).astype(np.int32)
                else:
                    data = np.frombuffer(payload, dtype=buffer_dtype, count=block_frames * args.channels)

                try:
                    pkt_queue.put_nowait(data)
                except queue.Full:
                    try:
                        pkt_queue.get_nowait()
                    except queue.Empty:
                        pass
                    pkt_queue.put_nowait(data)

                packet_stats["bytes"] += len(payload)
                packet_stats["packets"] += 1
        except OSError:
            pass

    with closing(socket.socket(socket.AF_INET, socket.SOCK_DGRAM)) as sock:
        sock.bind((args.host, args.port))
        sock.settimeout(None)
        print(f"Listening on {args.host}:{args.port} ({args.sample_rate} Hz, {args.channels} ch, {args.bits}-bit)")

        reader = threading.Thread(target=udp_reader, args=(sock,), name="udp-reader", daemon=True)
        reader.start()

        try:
            target_prefill = max(1, int(queue_capacity * max(0.0, min(args.prefill, 1.0))))
            while block_frames is None or pkt_queue.qsize() < target_prefill:
                time.sleep(0.01)

            def callback(outdata: np.ndarray, frames: int, time_info, status) -> None:
                nonlocal packet_stats
                try:
                    data = pkt_queue.get_nowait()
                except queue.Empty:
                    data = np.zeros(frames * args.channels, dtype=stream_dtype)
                    packet_stats["underruns"] += 1

                if data.shape[0] != frames * args.channels:
                    padded = np.zeros(frames * args.channels, dtype=stream_dtype)
                    count = min(data.shape[0], padded.shape[0])
                    padded[:count] = data[:count]
                    data = padded

                outdata[:] = data.reshape(-1, args.channels)

                now = time.monotonic()
                elapsed = now - packet_stats["last_log"]
                if elapsed >= 1.0:
                    rate = packet_stats["bytes"] / elapsed if packet_stats["bytes"] else 0.0
                    print(
                        f"Rate: {rate:.0f} B/s, packets: {packet_stats['packets']}, underruns: {packet_stats['underruns']}"
                    )
                    packet_stats["bytes"] = 0
                    packet_stats["packets"] = 0
                    packet_stats["underruns"] = 0
                    packet_stats["last_log"] = now

            with sd.OutputStream(
                samplerate=args.sample_rate,
                channels=args.channels,
                dtype=stream_dtype,
                blocksize=block_frames,
                callback=callback,
            ):
                print("Streaming audio... Press Ctrl+C to stop")
                while True:
                    time.sleep(1)
        except KeyboardInterrupt:
            print("\nStopping receiver")
        finally:
            stop_event.set()


if __name__ == "__main__":
    main()
