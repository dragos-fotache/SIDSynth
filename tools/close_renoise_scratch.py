#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Save and normally close only the ready, dedicated SIDSynth scratch host.

Sends WM_DELETE_WINDOW, waits at most 20 seconds, and never forces termination.
Do not run during startup/recovery. Requires the Stage 4 X11 helpers and MCP.
"""
import argparse
import ctypes as C
import json
import time
from pathlib import Path
from check_renoise_stage4 import X, d, root, tree, WINDOWS, U, I
from renoise_mcp import RenoiseMCP


def close(pid, name):
    WINDOWS.clear()
    tree(root)
    windows = [w for w in WINDOWS if w['pid'] == pid and 'sidsynth-' in w['title'].lower()
               and w['title'].endswith('.xrns - Renoise (x86_64)')]
    assert len(windows) == 1, 'Need a ready scratch main window'
    time.sleep(1)
    client = RenoiseMCP()
    client.initialize()
    state = json.loads(client.call('sidsynth_host_check', {'operation': 'inspect'})['content'][0]['text'])
    assert state['pid'] == pid and Path(state['file']).resolve().is_relative_to(Path(__file__).resolve().parent.parent / 'work/renoise-host')
    client.call('transport_stop')
    client.call('sidsynth_host_check', {'operation': 'save', 'name': name})
    class Data(C.Union):
        _fields_ = [('b', C.c_char * 20), ('s', C.c_short * 10), ('l', C.c_long * 5)]
    class ClientMessage(C.Structure):
        _fields_ = [('type', I), ('serial', U), ('send_event', I), ('display', C.c_void_p),
                    ('window', U), ('message_type', U), ('format', I), ('data', Data)]
    class Event(C.Union):
        _fields_ = [('client', ClientMessage), ('pad', C.c_long * 24)]
    event = Event()
    event.client = ClientMessage(33, 0, 1, d, windows[0]['window'], X.XInternAtom(d, b'WM_PROTOCOLS', 0), 32, Data())
    event.client.data.l[0] = X.XInternAtom(d, b'WM_DELETE_WINDOW', 0)
    X.XSendEvent.argtypes = [C.c_void_p, U, I, C.c_long, C.POINTER(Event)]
    assert X.XSendEvent(d, windows[0]['window'], 0, 0, C.byref(event))
    X.XFlush(d)
    started = time.monotonic()
    proc = Path(f'/proc/{pid}/stat')
    while proc.exists() and time.monotonic() - started < 20:
        if proc.read_text().split(')')[1].split()[0] == 'Z': break
        time.sleep(.2)
    exited = not proc.exists() or proc.read_text().split(')')[1].split()[0] == 'Z'
    result = {'pid': pid, 'normal_close': exited, 'seconds': time.monotonic() - started, 'saved': name}
    print(json.dumps(result))
    assert exited, 'Scratch host did not close in 20 seconds; no force was used'


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid', type=int, required=True)
    parser.add_argument('--name', required=True)
    args = parser.parse_args()
    close(args.pid, args.name)
