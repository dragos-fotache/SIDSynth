#!/usr/bin/env python3
"""Check Stage 3 in the dedicated Renoise scratch alias song, recording its PID only."""
import argparse
import json
import time
import numpy as np
from check_renoise_host import Checks, ROOT, RATE


class Stage3(Checks):
    def __init__(self, pid):
        super().__init__(pid)
        self.transcript = ROOT / "stage3-mcp-transcript.jsonl"
        state = self.host("inspect")
        assert state["pid"] == pid, "MCP endpoint belongs to a different process"
        assert len(state["instruments"][0]["parameters"]) == 67, "Stage 3 module is not loaded"
        self.results["legacy_schema2_loaded"] = {p["name"]: p["text"] for p in state["instruments"][0]["parameters"]}

    def default_patch(self):
        self.call("transport_stop")
        for p in self.host("inspect")["instruments"][0]["parameters"]:
            self.parameter(p["name"], p["default"])
        self.defaults()

    def spectrum(self, data):
        a = data[int(.4 * RATE):int(.9 * RATE)]
        return np.abs(np.fft.rfft(a * np.hanning(len(a))))

    def run(self):
        self.default_patch()
        self.pattern([(1, 1, "A-5", 0, 1), (3, 1, "D-5", 2, 1),
                      (1, 17, "OFF", 0, 1), (3, 17, "OFF", 2, 1)])
        self.parameter("Voice 3 direct output", 0)
        baseline = self.capture("stage3-host-direct-off", 3.8, self.play)
        self.parameter("Voice 1 sync", 1)
        synced = self.capture("stage3-host-sync", 3.8, self.play)
        difference = np.linalg.norm(self.spectrum(synced) - self.spectrum(baseline)) / np.linalg.norm(self.spectrum(baseline))
        assert difference > .1, "Sync did not change the audible voice with the modulator's direct output off"
        self.parameter("Voice 1 sync", 0)
        self.parameter("Voice 1 saw", 0)
        self.parameter("Voice 1 triangle", 1)
        triangle = self.capture("stage3-host-triangle", 3.8, self.play)
        self.parameter("Voice 1 ring", 1)
        ring = self.capture("stage3-host-ring", 3.8, self.play)
        ring_difference = np.linalg.norm(self.spectrum(ring) - self.spectrum(triangle)) / np.linalg.norm(self.spectrum(triangle))
        assert ring_difference > .1
        self.results["shared_chip_interactions"] = {"sync_spectrum_difference": float(difference), "ring_spectrum_difference": float(ring_difference)}
        print("PASS: host sync/ring with voice 3 direct output off", flush=True)

        self.parameter("Voice 1 ring", 0)
        self.parameter("Voice 1 triangle", 0)
        self.parameter("Voice 1 saw", 1)
        self.parameter("Filter voice 1", 1)
        self.parameter("Filter low pass", 1)
        self.pattern([(1, 1, "A-5", 0, 1), (1, 33, "OFF", 0, 1)])
        self.host("automation_setup", parameter="Filter cutoff")
        time.sleep(.15)
        envelope = self.host("automation_envelope")
        audio = self.capture("stage3-host-cutoff-automation", 5, self.play)
        high = self.rms(audio, .4, .8)
        low = self.rms(audio, 1.4, 1.8)
        reopened = self.rms(audio, 2.4, 2.8)
        assert high > .002 and low < high * .4 and reopened > low * 2
        self.results["native_automation"] = {"envelope": envelope, "open_rms": high, "closed_rms": low, "reopened_rms": reopened}
        print("PASS: native Instrument Automation envelope changes shared-filter audio", flush=True)

        # Clear automation before manual/recall checks; pattern_clear also clears
        # automation according to the native Pattern API.
        self.pattern([])
        self.parameter("Filter voice 1", 0)
        self.parameter("Filter low pass", 0)
        for model, clock in ((0, 0), (0, 1), (1, 1), (1, 0)):
            self.parameter("SID model", model)
            self.parameter("SID clock", clock)
            self.pattern([(1, 1, "A-5", 0, 1), (1, 17, "OFF", 0, 1)])
            audio = self.capture(f"stage3-host-model{model}-clock{clock}", 3.5, self.play)
            assert self.tone(audio, .5, 1.5, 440) > .002
        self.results["models_clocks_A4"] = True
        print("PASS: both models and both clocks play A4 in Renoise", flush=True)

        self.run_manual_and_recall()

    def run_manual_and_recall(self):
        self.pattern([])
        self.parameter("Voice 1 source", 1)
        self.parameter("Voice 1 manual frequency", 7493 / 65535)
        self.parameter("Voice 1 manual hold", 0)
        time.sleep(.08)  # Ensure the off edge reaches a 1024-frame callback.
        self.parameter("Voice 1 manual hold", 1)
        # Manual source holds without any keyboard focus or MIDI event.
        manual = self.capture("stage3-host-manual-hold", 2)
        assert self.tone(manual, .4, 1.4, 440) > .002
        self.call("transport_play")
        time.sleep(.15)
        self.call("transport_stop")
        quiet = self.capture("stage3-host-manual-stop", 2)
        assert self.rms(quiet, .3, 1.5) < .0003
        self.parameter("Voice 1 pulse width", .7)
        quiet = self.capture("stage3-host-manual-stays-disarmed", 2)
        assert self.rms(quiet, .3, 1.5) < .0003
        self.parameter("Voice 1 manual hold", 0)
        time.sleep(.08)  # Ensure the off edge reaches a 1024-frame callback.
        self.parameter("Voice 1 manual hold", 1)
        rearmed = self.capture("stage3-host-manual-rearmed", 2)
        assert self.tone(rearmed, .4, 1.4, 440) > .002
        self.results["manual_ownership_stop_rearm"] = True
        print("PASS: explicit manual hold, stop disarming, no unrelated rearm, explicit recovery", flush=True)

        # A nontrivial complete patch, with a deliberately saved manual hold.
        for name, value in {"Output": .65, "Filter cutoff": .46, "Filter resonance": .6,
                            "Filter voice 1": 1, "Filter voice 2": 1, "Filter external input": 1,
                            "Filter low pass": 1, "Filter band pass": 1, "SID master volume": .8,
                            "Voice 2 triangle": 1, "Voice 2 saw": 0, "Voice 2 tuning": .5625,
                            "Voice 2 pulse width": .37, "Voice 2 attack": .2, "Voice 2 decay": .4,
                            "Voice 2 sustain": .6, "Voice 2 release": .8, "Voice 2 sync": 1,
                            "Voice 3 pulse": 1, "Voice 3 test / reset": 1, "Voice 3 ring": 1,
                            "SID model": 0, "SID clock": 1}.items():
            self.parameter(name, value)
        self.parameter("Voice 1 manual hold", 0)
        time.sleep(.08)  # Ensure the off edge reaches a 1024-frame callback.
        self.parameter("Voice 1 manual hold", 1)
        expected = {p["name"]: p for p in self.host("inspect")["instruments"][0]["parameters"]}
        (ROOT / "stage3-recall-expected.json").write_text(json.dumps(expected, indent=2) + "\n")
        self.host("save", name="sidsynth-stage3-recall")
        self.parameter("Output", .4)
        self.parameter("Filter resonance", 0)
        self.parameter("Voice 2 pulse width", 1)
        self.host("save", name="sidsynth-stage3-before-recall")
        self.host("load_song", name="sidsynth-stage3-recall")
        self.finish_recall(expected)

    def finish_recall(self, expected):
        deadline = time.monotonic() + 10
        while True:
            time.sleep(.2)
            state = self.host("inspect")
            if state["file"].endswith("/sidsynth-stage3-recall.xrns"):
                break
            assert time.monotonic() < deadline
        actual = {p["name"]: p["value"] for p in state["instruments"][0]["parameters"]}
        # Renoise can retain the unsnapped normalized value it last sent (e.g.
        # .46 cutoff), while the plugin stores the rounded register (942/2047).
        # Compare exact displayed hardware/musical values at their resolution.
        displayed = {p["name"]: p["text"] for p in state["instruments"][0]["parameters"]}
        assert len(actual) == 67 and all(displayed[k] == v["text"] for k, v in expected.items())
        quiet = self.capture("stage3-host-recall-silence", 2)
        assert self.rms(quiet, .3, 1.5) < .0003
        self.results["complete_patch_recall"] = {"parameters": actual, "quiet_rms": self.rms(quiet, .3, 1.5)}
        print("PASS: song recall restores all 67 parameters and disarms saved manual hold", flush=True)
        self.results["input_probe"] = self.host("input_inspect")
        self.results["input_limitation"] = "Native Sidechain exposes Dest. Track/Effect, with no instrument destination. Track output routes are Master/hardware. EXT IN is implemented and offline-tested; Renoise instrument audio routing is unavailable through this native/API workflow."
        for _ in range(5):
            assert self.host("editor", visible=True)["instruments"][0]["editor"]
            time.sleep(.15)
            assert not self.host("editor", visible=False)["instruments"][0]["editor"]
        self.results["editor_open_close_cycles"] = 5
        self.host("save", name="sidsynth-stage3-recall")
        print("PASS: five host editor cycles; native input-routing limitation recorded", flush=True)
        self.create_audition()

    def create_audition(self):
        self.default_patch()
        for name, value in {"Output": .7, "Play mode": 1, "Voice 1 sync": 1,
                            "Voice 2 saw": 0, "Voice 2 triangle": 1, "Voice 2 ring": 1,
                            "Voice 2 tuning": .4375, "Voice 3 saw": 0, "Voice 3 pulse": 1,
                            "Voice 3 tuning": .53125, "Voice 3 direct output": 0,
                            "Filter voice 1": 1, "Filter voice 2": 1, "Filter low pass": 1,
                            "Filter cutoff": 1300 / 2047, "Filter resonance": 6 / 15}.items():
            self.parameter(name, value)
        self.pattern([(1, 1, "A-5", 0, 1), (1, 13, "OFF", 0, 1),
                      (1, 17, "C-6", 0, 1), (1, 29, "OFF", 0, 1)])
        audition = self.capture("stage3-host-audition", 5, self.play)
        assert self.rms(audition, .4, .9) > .001
        self.host("save", name="sidsynth-stage3-audition")
        self.results["audition_song"] = str(ROOT / "sidsynth-stage3-audition.xrns")
        print("PASS: stopped playable Stage 3 audition song retained", flush=True)

    def save_results(self):
        (ROOT / "stage3-host-results.json").write_text(json.dumps(self.results, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int, required=True)
    resume = parser.add_mutually_exclusive_group()
    resume.add_argument("--resume-from-manual", action="store_true")
    resume.add_argument("--resume-recall", action="store_true")
    resume.add_argument("--audition-only", action="store_true")
    args = parser.parse_args()
    checks = Stage3(args.pid)
    try:
        if args.audition_only:
            checks.results.update(json.loads((ROOT / "stage3-host-results.json").read_text()))
            checks.create_audition()
        elif args.resume_recall:
            checks.results.update(json.loads((ROOT / "stage3-host-results.json").read_text()))
            checks.results.pop("error", None)
            checks.finish_recall(json.loads((ROOT / "stage3-recall-expected.json").read_text()))
        elif args.resume_from_manual:
            checks.results.update(json.loads((ROOT / "stage3-host-results.json").read_text()))
            checks.results.pop("error", None)
            checks.run_manual_and_recall()
        else:
            checks.run()
        checks.results["passed"] = True
    except Exception as error:
        checks.results["passed"] = False
        checks.results["error"] = repr(error)
        raise
    finally:
        checks.save_results()
