#!/usr/bin/env python3
"""Run Stage 2 checks in an already loaded SIDSynth scratch alias song.

Uses ReMCP for Renoise operations and records only the selected Renoise PID's
PipeWire/JACK output ports. Requires numpy, scipy, pw-record and pw-link.
"""
import argparse
import hashlib
import json
import subprocess
import time
import warnings
from pathlib import Path

import numpy as np
from scipy.io import wavfile

from renoise_mcp import RenoiseMCP

ROOT = Path(__file__).resolve().parent.parent / "work/renoise-host"
RATE = 48000


class Checks:
    def __init__(self, pid):
        self.pid = pid
        self.client = RenoiseMCP()
        self.client.initialize()
        self.results = {}
        self.transcript = ROOT / "mcp-transcript.jsonl"
        state = self.host("inspect")
        assert str(state["file"]).startswith(str(ROOT) + "/"), "Open the dedicated scratch song first"
        assert [i["alias"] for i in state["instruments"][:3]] == [0, 1, 1], "Need one instance plus two aliases"
        self.results["version"] = state["version"]
        self.results["module_sha256"] = hashlib.sha256((Path.home() /
            ".vst3/SIDSynth.vst3/Contents/x86_64-linux/SIDSynth.so").read_bytes()).hexdigest()
        self.results["aliases"] = [{k: i[k] for k in ("index", "alias", "channel", "path")}
                                   for i in state["instruments"][:3]]

    def call(self, name, args=None):
        result = self.client.call(name, args)
        with self.transcript.open("a") as stream:
            stream.write(json.dumps({"time": time.time(), "tool": name, "arguments": args, "result": result}) + "\n")
        text = result["content"][0]["text"]
        try:
            return json.loads(text)
        except json.JSONDecodeError:
            return text

    def host(self, operation, **args):
        return self.call("sidsynth_host_check", {"operation": operation, **args})

    def parameter(self, name, value):
        return self.host("set_parameter", parameter=name, value=value)

    def reset_mode(self, mode=0):
        self.call("transport_stop")
        self.parameter("Play mode", 1 - mode)
        time.sleep(.05)
        self.parameter("Play mode", mode)
        time.sleep(.05)

    def pattern(self, events):
        self.call("pattern_clear", {"index": 0})
        for track, line, note, instrument, column in events:
            self.call("pattern_set_note", {"pattern": 0, "track": track, "line": line,
                "note": note, "instrument": instrument, "column": column})

    def capture(self, name, seconds, action=None, settle_seconds=0):
        objects = json.loads(subprocess.check_output(["pw-dump"]))
        clients = {o["id"] for o in objects if o.get("type") == "PipeWire:Interface:Client"
                   and str(o.get("info", {}).get("props", {}).get("application.process.id")) == str(self.pid)}
        nodes = [o for o in objects if o.get("type") == "PipeWire:Interface:Node"
                 and o.get("info", {}).get("props", {}).get("client.id") in clients
                 and o.get("info", {}).get("n-output-ports") == 2]
        assert len(nodes) == 1, "Cannot identify this Renoise instance's two-channel output"
        source_ports = sorted([o for o in objects if o.get("type") == "PipeWire:Interface:Port"
            and o.get("info", {}).get("props", {}).get("node.id") == nodes[0]["id"]
            and o["info"]["direction"] == "output"], key=lambda o: o["info"]["props"]["port.id"])
        path = ROOT / (name + ".wav")
        node_name = "sidsynth-host-capture-" + str(self.pid)
        recorder = subprocess.Popen(["pw-record", "--target", "0", "--rate", str(RATE),
            "--channels", "2", "--format", "f32", "--properties", json.dumps({"node.name": node_name}),
            "-n", str(round(seconds * RATE)), str(path)], stderr=subprocess.PIPE)
        try:
            deadline = time.monotonic() + 3
            while True:
                objects = json.loads(subprocess.check_output(["pw-dump"]))
                target = [o for o in objects if o.get("info", {}).get("props", {}).get("node.name") == node_name]
                if target:
                    ports = sorted([o for o in objects if o.get("type") == "PipeWire:Interface:Port"
                        and o.get("info", {}).get("props", {}).get("node.id") == target[0]["id"]
                        and o["info"]["direction"] == "input"], key=lambda o: o["info"]["props"]["port.name"])
                    if len(ports) == 2:
                        break
                assert time.monotonic() < deadline, "Recorder ports did not appear"
                time.sleep(.05)
            for source, destination in zip(source_ports, ports):
                subprocess.run(["pw-link", str(source["id"]), str(destination["id"])], check=True)
            if action is not None:
                action()
            _, stderr = recorder.communicate(timeout=seconds + 5)
            assert not stderr, stderr.decode()
            # pw-record returns 1 when its finite sample count is reached.
            with warnings.catch_warnings():
                warnings.simplefilter("ignore", wavfile.WavFileWarning)
                rate, audio = wavfile.read(path)
            assert rate == RATE and len(audio) >= round(seconds * RATE) - 64
            assert np.isfinite(audio).all() and np.max(np.abs(audio)) < 1
            # For already sounding manual/keyboard UI checks, the two recorder
            # ports are linked sequentially. Exclude their setup interval from
            # stereo comparison/analysis, while retaining the complete raw WAV.
            skipped = round(settle_seconds * RATE)
            frames = len(audio)
            audio = audio[skipped:]
            assert np.max(np.abs(audio[:, 0] - audio[:, 1])) < 1e-5, "Stereo channels differ"
            y = audio[:, 0].astype(float)
            active = np.flatnonzero(np.abs(y) > .001)
            offset = int(active[0]) if len(active) else 0
            self.results.setdefault("captures", {})[name] = {"file": str(path), "frames": frames, "analysis_skip_frames": skipped,
                "peak": float(np.max(np.abs(y))), "onset_sample": offset,
                "source_pid": self.pid, "source_ports": [p["id"] for p in source_ports]}
            return y[offset:]
        finally:
            if recorder.poll() is None:
                recorder.terminate()
                recorder.wait(timeout=3)
            self.call("transport_stop")

    @staticmethod
    def rms(y, a, b):
        return float(np.sqrt(np.mean(y[round(a * RATE):round(b * RATE)] ** 2)))

    @staticmethod
    def tone(y, a, b, hz):
        part = y[round(a * RATE):round(b * RATE)]
        weights = np.hanning(len(part))
        return float(2 * abs(np.dot(part * weights, np.exp(-2j * np.pi * hz * np.arange(len(part)) / RATE))) / weights.sum())

    def play(self):
        self.call("transport_play")

    def defaults(self):
        self.parameter("Output", .8)
        self.parameter("Layered MIDI channel", 0)
        for i in range(3):
            self.parameter("Voice " + str(i + 1) + " MIDI channel", i / 15)
            self.host("channel", instrument=i, channel=i + 1)
        self.reset_mode()

    def run(self):
        self.defaults()
        self.pattern([(1, 1, "A-5", 0, 1), (1, 17, "OFF", 0, 1)])
        solo = self.capture("host-solo", 4, self.play)
        solo_rms = self.rms(solo, .5, 1.5)
        assert solo_rms > .003 and self.tone(solo, .5, 1.5, 440) > .003
        assert self.rms(solo, 3, 3.5) < .0003
        self.results["solo"] = {"held_rms": solo_rms, "tail_rms": self.rms(solo, 3, 3.5)}
        print("PASS: host note, concert A4 and release", flush=True)

        self.reset_mode()
        self.pattern([(1, 1, "A-5", 0, 1), (2, 5, "C-5", 1, 1), (3, 9, "E-5", 2, 1),
                      (1, 17, "OFF", 0, 1), (2, 25, "OFF", 1, 1), (3, 33, "OFF", 2, 1)])
        parts = self.capture("host-three-parts", 6, self.play)
        before = [self.tone(parts, 1.3, 1.8, hz) for hz in (440, 261.626, 329.628)]
        after1 = [self.tone(parts, 2.4, 2.8, hz) for hz in (440, 261.626, 329.628)]
        after2 = [self.tone(parts, 3.4, 3.8, hz) for hz in (440, 261.626, 329.628)]
        assert min(before) > .002
        assert after1[0] < before[0] * .15 and min(after1[1:]) > .002
        assert after2[1] < before[1] * .15 and after2[2] > .002
        assert self.rms(parts, 5, 5.5) < .0003
        self.results["independent_release"] = {"all_three": before, "after_part1": after1, "after_part2": after2}
        print("PASS: two aliases, three simultaneous parts, independent release", flush=True)

        for repeat in (False, True):
            self.reset_mode()
            self.pattern([(1, 1, "A-5", 0, 1), (1, 5, "A-5" if repeat else "C-6", 0, 2),
                          (1, 9, "OFF", 0, 2), (1, 17, "OFF", 0, 1)])
            audio = self.capture("host-repeated" if repeat else "host-fallback", 4, self.play)
            assert self.tone(audio, 1.3, 1.7, 440) > .003
            assert self.rms(audio, 3, 3.5) < .0003
            if not repeat:
                assert self.tone(audio, .7, .9, 523.251) > .003
            self.results["repeated" if repeat else "fallback"] = True
        print("PASS: held-note fallback and counted repeated pitches", flush=True)

        self.run_remaining(solo_rms)
        self.run_midi()

    def run_remaining(self, solo_rms):
        self.reset_mode(1)
        self.pattern([(1, 1, "A-5", 0, 1), (1, 17, "OFF", 0, 1)])
        layered = self.capture("host-layered", 4, self.play)
        ratio = self.rms(layered, .5, 1.5) / solo_rms
        # SID reset preserves oscillator accumulators. After playing different
        # pitches, unison oscillators need not be in phase or sum to 3x RMS.
        # Compare Layered to the same chip playing all voices on one channel.
        self.parameter("Voice 2 MIDI channel", 0)
        self.parameter("Voice 3 MIDI channel", 0)
        self.reset_mode(0)
        reference = self.capture("host-shared-channel-reference", 4, self.play)
        reference_rms = self.rms(reference, .5, 1.5)
        assert abs(reference_rms / self.rms(layered, .5, 1.5) - 1) < .05
        for hz in (440, 880, 1320, 1760):
            actual = self.tone(layered, .5, 1.5, hz)
            expected = self.tone(reference, .5, 1.5, hz)
            assert abs(actual - expected) < max(.0001, expected * .05)
        self.results["layered_shared_reference_rms_ratio"] = reference_rms / self.rms(layered, .5, 1.5)
        self.results["layered_rms_ratio"] = ratio
        self.parameter("Voice 2 MIDI channel", 1 / 15)
        self.parameter("Voice 3 MIDI channel", 2 / 15)
        self.reset_mode(1)

        self.parameter("Layered MIDI channel", 1)
        ignored = self.capture("host-layered-wrong-channel", 2, self.play)
        assert self.rms(ignored, .2, 1.5) < .0003
        self.host("channel", instrument=0, channel=16)
        matched = self.capture("host-layered-channel16", 4, self.play)
        assert self.tone(matched, .5, 1.5, 440) > .005
        self.results["layered_channel16"] = True
        print("PASS: Layered sum and selected input channel 16", flush=True)

        self.host("channel", instrument=0, channel=1)
        self.parameter("Layered MIDI channel", 0)
        for change in ("mode", "channel", "stop"):
            self.reset_mode()
            self.parameter("Voice 1 MIDI channel", 0)
            self.pattern([(1, 1, "A-5", 0, 1)])
            def action():
                self.play()
                time.sleep(1)
                if change == "mode": self.parameter("Play mode", 1)
                elif change == "channel": self.parameter("Voice 1 MIDI channel", 1)
                else: self.call("transport_stop")
            audio = self.capture("host-reset-" + change, 3, action)
            assert self.rms(audio, .3, .7) > .003
            assert self.rms(audio, 1.5, 2.5) < .0003
            self.results["reset_" + change] = True
        print("PASS: held notes clear on mode/channel changes and transport stop", flush=True)

        # Save while a note is playing. Save any later modifications separately
        # before reload, avoiding a modal prompt and preserving both snapshots.
        self.reset_mode(1)
        self.parameter("Layered MIDI channel", 1)
        self.parameter("Voice 1 MIDI channel", 1)
        self.parameter("Voice 2 MIDI channel", .6)
        self.parameter("Voice 3 MIDI channel", 14 / 15)
        self.parameter("Output", .6)  # -24 dB
        self.host("channel", instrument=0, channel=16)
        self.pattern([(1, 1, "A-5", 0, 1), (1, 17, "OFF", 0, 1)])
        self.play()
        time.sleep(.5)
        self.host("save", name="sidsynth-recall")
        self.call("transport_stop")
        self.parameter("Output", .4)
        self.parameter("Play mode", 0)
        self.host("save", name="sidsynth-before-recall")
        self.host("load_song", name="sidsynth-recall")
        deadline = time.monotonic() + 10
        while True:
            time.sleep(.25)
            restored = self.host("inspect")
            if restored["file"].endswith("/sidsynth-recall.xrns"):
                break
            assert time.monotonic() < deadline, "Song reload did not complete"
        parameters = {p["name"]: p for p in restored["instruments"][0]["parameters"]}
        expected = {"Output": .6, "Play mode": 1, "Layered MIDI channel": 1,
                    "Voice 1 MIDI channel": 1, "Voice 2 MIDI channel": .6, "Voice 3 MIDI channel": 14 / 15}
        assert all(abs(parameters[name]["value"] - value) < 1e-5 for name, value in expected.items())
        assert restored["instruments"][0]["channel"] == 16 and not restored["playing"]
        quiet = self.capture("host-recall-silence", 2)
        assert self.rms(quiet, .3, 1.5) < .0003
        replay = self.capture("host-recall-replay", 4, self.play)
        assert self.tone(replay, .5, 1.5, 440) > .001
        self.results["recall"] = {"parameters": parameters, "silent_rms": self.rms(quiet, .3, 1.5),
                                  "replay_rms": self.rms(replay, .5, 1.5)}
        print("PASS: song recall restores gain/routing, excludes held notes, and plays again", flush=True)

        for _ in range(5):
            opened = self.host("editor", visible=True)
            assert opened["instruments"][0]["editor"]
            time.sleep(.15)
            closed = self.host("editor", visible=False)
            assert not closed["instruments"][0]["editor"]
        self.results["editor_open_close_cycles"] = 5
        self.host("save", name="sidsynth-recall")
        print("PASS: five editor open/close cycles inside Renoise", flush=True)

    def run_midi(self):
        self.defaults()
        configured = self.host("midi_control_setup", instrument=0)
        assert configured[0]["name"] == "Pitchbend" and "64" in configured[2]["name"]
        def midi(parameter, value):
            return self.call("track_device_set_param", {"index": 1, "device_index": 2,
                "param_index": parameter, "value": value})
        midi(1, 63.5)
        midi(3, 0)
        self.pattern([(1, 1, "A-5", 0, 1), (2, 1, "C-5", 1, 1),
                      (1, 25, "OFF", 0, 1), (2, 25, "OFF", 1, 1)])
        def bend_action():
            self.play()
            time.sleep(1)
            midi(1, 127)
            time.sleep(1.2)
            midi(1, 63.5)
        bent = self.capture("host-pitchbend", 4.5, bend_action)
        before = self.tone(bent, .3, .7, 440)
        shifted = self.tone(bent, 1.4, 1.8, 493.883)
        other = self.tone(bent, 1.4, 1.8, 261.626)
        assert min(before, shifted, other) > .002
        assert self.tone(bent, 1.4, 1.8, 440) < before * .15
        assert self.tone(bent, 2.5, 2.8, 440) > .002
        self.results["pitchbend_host"] = {"A4_before": before, "B4_bent": shifted, "alias_C4_unbent": other}
        print("PASS: host pitch bend reaches +2 semitones and leaves the other alias unchanged", flush=True)

        self.reset_mode()
        self.pattern([(1, 1, "A-5", 0, 1), (2, 1, "C-5", 1, 1),
                      (1, 9, "OFF", 0, 1), (2, 9, "OFF", 1, 1)])
        def sustain_action():
            self.play()
            time.sleep(.5)
            midi(3, 127)
            time.sleep(1.6)
            midi(3, 0)
        sustained = self.capture("host-sustain", 4, sustain_action)
        held = self.tone(sustained, 1.4, 1.8, 440)
        released_other = self.tone(sustained, 1.4, 1.8, 261.626)
        tail = self.rms(sustained, 3, 3.5)
        assert held > .003 and released_other < .0003 and tail < .0003
        self.results["sustain_host"] = {"held_A4": held, "released_alias_C4": released_other, "pedal_up_tail_rms": tail}
        print("PASS: CC64 holds the assigned channel only and pedal-up releases it", flush=True)
        self.host("save", name="sidsynth-midi-tests")
        self.host("load_song", name="sidsynth-recall")
        deadline = time.monotonic() + 10
        while True:
            time.sleep(.25)
            state = self.host("inspect")
            if state["file"].endswith("/sidsynth-recall.xrns"):
                break
            assert time.monotonic() < deadline

    def save_results(self):
        (ROOT / "host-results.json").write_text(json.dumps(self.results, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int, required=True, help="PID of the dedicated test Renoise instance")
    parser.add_argument("--resume-from-layered", action="store_true", help="Reuse earlier captured/passed checks")
    parser.add_argument("--midi-only", action="store_true", help="Append the native MIDI Control checks")
    args = parser.parse_args()
    if args.resume_from_layered and args.midi_only:
        parser.error("Choose only one resume option")
    checks = Checks(args.pid)
    try:
        if args.resume_from_layered or args.midi_only:
            previous = json.loads((ROOT / "host-results.json").read_text())
            checks.results.update(previous)
            checks.results.pop("error", None)
            if args.midi_only:
                checks.run_midi()
            else:
                checks.run_remaining(previous["solo"]["held_rms"])
                checks.run_midi()
        else:
            checks.run()
        checks.results["passed"] = True
    except Exception as error:
        checks.results["passed"] = False
        checks.results["error"] = repr(error)
        raise
    finally:
        checks.save_results()
