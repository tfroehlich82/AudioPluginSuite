#!/usr/bin/env python3
"""
Pure-Python PNG generator (no PIL).
Generates 4K textures for Micro-Timing Chaos.
"""

import os
import struct
import zlib

def make_sequencer_bg(width=4096, height=2458, accent="#7DFF5A", noise=0.02):
    """Dark gradient with subtle noise."""
    data = bytearray()
    # Gradient background
    for y in range(0, height, 32):
        for x in range(0, width, 32):
            t = (x * height + y * width) / (width * height)
            r = int(0x0B + t * (0x1F - 0x0B))
            g = int(0x0E + t * (0x26 - 0x0E))
            b = int(0x13 + t * (0x31 - 0x13))
            data.extend([r, g, b, 255])
    # Noise
    for py in range(0, height, 32):
        for px in range(0, width, 32):
            v = int(noise * 255 * (1 if (px + py) % 7 == 0 else 0))
            data.extend([v, v, v, 255])
    # Border
    data.extend([0x2A, 0x33, 0x41, 255])
    data.extend([0x15, 0x1A, 0x22, 255])
    data.extend([0x2A, 0x33, 0x41, 255])
    data.extend([0x2A, 0x33, 0x41, 255])
    data.extend([0x15, 0x1A, 0x22, 255])
    data.extend([0x2A, 0x33, 0x41, 255])
    data.extend([0x2A, 0x33, 0x41, 255])
    data.extend([0x15, 0x1A, 0x22, 255])
    data.extend([0x2A, 0x33, 0x41, 255])
    data.extend([0x2A, 0x33, 0x41, 255])
    data.extend([0x15, 0x1A, 0x22, 255])
    return bytes(data)

def make_sequencer_grid(width=4096, height=2458):
    """Grid pattern."""
    data = bytearray()
    # Grid lines
    for y in range(0, height, 64):
        for x in range(width):
            data.extend([0x1E, 0x24, 0x2F, 64])
    for x in range(0, width, 64):
        for y in range(height):
            data.extend([0x1E, 0x24, 0x2F, 64])
    # Accent dots
    accent_radius = 32
    for i in range(0, width, 64):
        for j in range(0, height, 64):
            cx, cy = i + 32, j + 32
            for dy in range(-16, 17):
                for dx in range(-16, 17):
                    if dx*dx + dy*dy <= 256:
                        data.extend([0x3A, 0x4A, 0x5A, 128])
    return bytes(data)

def make_bpm_sync(width=4096, height=2458):
    """BPM sync icon background."""
    data = bytearray()
    cx, cy = width // 2, height // 2
    radius = min(width, height) // 3
    inner_radius = radius * 0.6
    # Background circle
    for dy in range(-int(radius), int(radius) + 1):
        for dx in range(-int(radius), int(radius) + 1):
            if dx*dx + dy*dy <= radius*radius:
                data.extend([0x1A, 0x2A, 0x3A, 255])
    # Inner circle
    for dy in range(-int(inner_radius), int(inner_radius) + 1):
        for dx in range(-int(inner_radius), int(inner_radius) + 1):
            if dx*dx + dy*dy <= inner_radius*inner_radius:
                data.extend([0x00, 0x00, 0x00, 128])
    return bytes(data)

def encode_png(data, width=4096, height=2458):
    """Encode raw RGBA data as PNG."""
    if not data:
        return b""
    
    signature = b'\x89PNG\r\n\x1a\n'
    
    # IHDR chunk
    ihdr_data = struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)
    ihdr_crc = 0x907753D5  # Precomputed for 4096x2458, 8-bit RGBA
    ihdr = struct.pack('>I', 13) + b'IHDR' + ihdr_data + struct.pack('>I', ihdr_crc)
    
    # IDAT chunk (compressed data)
    compressed = zlib.compress(data)
    idat_crc = zlib.crc32(b'IDAT\n\x00\x00\x00' + compressed) & 0xffffffff
    idat = struct.pack('>I', len(compressed)) + b'IDAT' + compressed + struct.pack('>I', idat_crc)
    
    # IEND chunk
    iend = struct.pack('>I', 0) + b'IEND\xae\x42\x60\x82'
    
    return signature + ihdr + idat + iend

def main():
    os.makedirs("Resources/textures", exist_ok=True)
    
    print("  sequencer_bg_4k.png")
    with open("Resources/textures/sequencer_bg_4k.png", "wb") as f:
        f.write(encode_png(make_sequencer_bg()))
    
    print("  sequencer_grid_4k.png")
    with open("Resources/textures/sequencer_grid_4k.png", "wb") as f:
        f.write(encode_png(make_sequencer_grid()))
    
    print("  bpm_sync_4k.png")
    with open("Resources/textures/bpm_sync_4k.png", "wb") as f:
        f.write(encode_png(make_bpm_sync()))
    
    print("  Done.")

if __name__ == "__main__":
    main()
