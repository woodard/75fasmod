#!/usr/bin/env python3
import socket
import time
import sys

SOCKET_PATH = "/tmp/75fasmod_data.sock"

def test_tx_burst():
    client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    
    try:
        print(f"[TEST] Connecting to daemon at {SOCKET_PATH}...")
        client.connect(SOCKET_PATH)

        # Generate ~10 KB payload (repeating line 100 times)
        base_str = "75fasmod 16-QAM TCM Test Pattern -- 0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ\n"
        payload = (base_str * 130).encode('utf-8')
        
        print(f"[TEST] Sending {len(payload)} bytes (~3 seconds of audio)...")
        client.sendall(payload)
        
        # Keep socket connected while the modem flushes and transmits
        time.sleep(5.0)

    except FileNotFoundError:
        print(f"[ERROR] Socket path '{SOCKET_PATH}' not found. Is 75fasmod running?")
        sys.exit(1)
    except Exception as e:
        print(f"[ERROR] Connection failed: {e}")
        sys.exit(1)
    finally:
        client.close()
        print("[TEST] Socket closed.")

if __name__ == "__main__":
    test_tx_burst()