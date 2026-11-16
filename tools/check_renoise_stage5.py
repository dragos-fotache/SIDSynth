#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Stage 5 actual-host checks and native demo export in the dedicated scratch PID.

Reuses the readiness, scoped recorder and speaker isolation from Stage 4.
Never invokes Renoise Demo's unavailable offline renderer.
"""
import argparse
import json
import shutil
import time
from pathlib import Path
from check_renoise_stage4 import Stage4, ROOT, WINDOWS, tree, root

PROJECT = Path(__file__).resolve().parent.parent


class Stage5(Stage4):
    def __init__(self, pid):
        super().__init__(pid)
        self.transcript = ROOT / 'stage5-mcp-transcript.jsonl'
        self.results.pop('legacy_schema2_loaded', None)

    def silence(self):
        self.call('transport_stop')
        for v in range(1, 4):
            self.parameter('Voice ' + str(v) + ' manual hold', 0)
        mode = self.host('inspect')['instruments'][0]['parameters'][1]['value']
        self.parameter('Play mode', 1 - mode)
        time.sleep(.08)
        self.parameter('Play mode', mode)
        time.sleep(.08)

    def reload(self, name):
        self.host('load_song', name=name)
        deadline = time.monotonic() + 20
        while True:
            WINDOWS.clear()
            tree(root)
            if any(w['pid'] == self.pid and w['title'].startswith(name + '.xrns - ') for w in WINDOWS):
                break
            assert time.monotonic() < deadline, 'Song main window did not settle'
            time.sleep(.2)
        time.sleep(1)
        return self.host('inspect')

    def run(self):
        self.default_patch()
        self.pattern([(1, 1, 'A-5', 0, 1), (2, 1, 'C-5', 1, 1), (3, 1, 'E-5', 2, 1),
                      (1, 9, 'OFF', 0, 1), (2, 17, 'OFF', 1, 1), (3, 25, 'OFF', 2, 1)])
        audio = self.capture('stage5-aliases', 4.5, self.play)
        before = [self.tone(audio, .3, .7, hz) for hz in (440, 261.626, 329.628)]
        after = [self.tone(audio, 1.4, 1.8, hz) for hz in (440, 261.626, 329.628)]
        assert min(before) > .002 and after[0] < before[0] * .15 and min(after[1:]) > .002
        self.results['aliases_independent_release'] = {'before': before, 'after': after}

        self.parameter('Filter voice 1', 1)
        self.parameter('Filter low pass', 1)
        self.pattern([(1, 1, 'A-5', 0, 1), (1, 33, 'OFF', 0, 1)])
        self.host('automation_setup', parameter='Filter cutoff')
        time.sleep(.15)
        self.host('automation_envelope')
        audio = self.capture('stage5-automation', 4.5, self.play)
        levels = [self.rms(audio, a, a + .4) for a in (.4, 1.4, 2.4)]
        assert levels[0] > .002 and levels[1] < levels[0] * .4 and levels[2] > levels[1] * 2
        self.results['automation_open_closed_open_rms'] = levels
        self.default_patch()

        state = self.host('inspect')
        assert not state['instruments'][3]['loaded'], 'Slot 03 must be empty'
        self.host('load_plugin', instrument=3)
        for p in self.host('inspect')['instruments'][3]['parameters']:
            self.host('set_parameter', instrument=3, parameter=p['name'], value=p['default'])
        self.host('set_parameter', instrument=3, parameter='Output', value=.7)
        assert abs(self.host('inspect')['instruments'][0]['parameters'][0]['value'] - .8) < .001
        self.pattern([(1, 1, 'A-5', 0, 1), (4, 1, 'C-6', 3, 1),
                      (1, 13, 'OFF', 0, 1), (4, 25, 'OFF', 3, 1)])
        audio = self.capture('stage5-two-instances', 4.5, self.play)
        assert min(self.tone(audio, .4, .8, h) for h in (440, 523.251)) > .001
        assert self.tone(audio, 2, 2.5, 440) < .0003 and self.tone(audio, 2, 2.5, 523.251) > .001
        self.results['two_instances_independent'] = True

        for inst in (0, 3):
            for _ in range(5):
                self.host('editor', instrument=inst, visible=True)
                time.sleep(.08)
                assert self.host('inspect')['instruments'][inst]['editor']
                self.host('editor', instrument=inst, visible=False)
        self.results['editor_cycles_per_instance'] = 5
        self.host('save', name='sidsynth-stage5-two-instances')
        expected = [{p['name']: p['text'] for p in i['parameters']} for i in self.host('inspect')['instruments'] if i['index'] in (0, 3)]
        self.host('set_parameter', instrument=3, parameter='Output', value=.5)
        self.host('save', name='sidsynth-stage5-altered')
        state = self.reload('sidsynth-stage5-two-instances')
        actual = [{p['name']: p['text'] for p in i['parameters']} for i in state['instruments'] if i['index'] in (0, 3)]
        assert actual == expected, 'Multiple-instance song recall differs'
        quiet = self.capture('stage5-recalled-silence', 1.5)
        assert self.rms(quiet, .3, 1) < .0003
        self.results['two_instance_recall'] = True
        for n in range(3):
            audio = self.capture('stage5-stop-start-' + str(n), 1, self.play)
            assert self.rms(audio, .3, .7) > .002
            quiet = self.capture('stage5-stop-silence-' + str(n), .8)
            assert self.rms(quiet, .2, .6) < .0003
        self.results['stop_start_cycles'] = 3
        self.host('unload_plugin', instrument=3)
        self.export_examples()
        self.results['offline_render'] = self.host('render', name='sidsynth-stage5-offline')
        assert self.results['offline_render']['state'] == 'unavailable'
        self.results['passed'] = True
        print('PASS: aliases, automation, two instances, 10 editor cycles, recall, stop/start and demo patches', flush=True)

    def export_examples(self):
        patches = json.loads((PROJECT / 'examples/patches.json').read_text())['patches']
        self.results['demo_patches'] = []
        for patch in patches:
            self.default_patch()
            for name, value in patch['parameters'].items():
                self.parameter(name, value)
            self.host('save_instrument', name=patch['file'])
            expected = {p['name']: p['text'] for p in self.host('inspect')['instruments'][0]['parameters']}
            self.parameter('Output', .4)
            # Replacing Renoise's primary instrument drops its alias links.
            # Verify native recall in the now-empty independent test slot.
            assert not self.host('inspect')['instruments'][3]['loaded']
            self.host('load_instrument', instrument=3, name=patch['file'])
            actual = {p['name']: p['text'] for p in self.host('inspect')['instruments'][3]['parameters']}
            assert actual == expected, 'Native demo instrument recall differs'
            shutil.copy2(ROOT / (patch['file'] + '.xrni'), PROJECT / 'examples')
            self.pattern([(4, 1, 'A-5', 3, 1), (4, 13, 'OFF', 3, 1)])
            audio = self.capture('stage5-' + patch['file'], 3, self.play)
            assert self.rms(audio, .3, .8) > .0005, 'Demo patch is silent'
            self.host('unload_plugin', instrument=3)
            self.results['demo_patches'].append(patch['file'])
        self.default_patch()
        for name, value in patches[0]['parameters'].items():
            self.parameter(name, value)
        events = []
        for track, notes in ((1, ['C-4', 'G-4', 'A#4', 'G-4']), (2, ['G-5', 'A#5', 'D-6', 'C-6']),
                             (3, ['D#5', 'D-5', 'F-5', 'D#5'])):
            for n, note in enumerate(notes):
                events.extend([(track, n * 16 + 1, note, track - 1, 1), (track, n * 16 + 13, 'OFF', track - 1, 1)])
        self.pattern(events)
        assert [i['alias'] for i in self.host('inspect')['instruments'][:3]] == [0, 1, 1], 'Demo lost its primary-instance aliases'
        self.host('save', name='sidsynth-three-parts-example')
        self.reload('sidsynth-three-parts-example')
        assert [i['alias'] for i in self.host('inspect')['instruments'][:3]] == [0, 1, 1], 'Saved demo alias recall differs'
        self.results['demo_aliases_after_native_reload'] = True
        shutil.copy2(ROOT / 'sidsynth-three-parts-example.xrns', PROJECT / 'examples')
        self.capture('stage5-three-parts-example', 8, self.play)
        self.host('editor', visible=True)
        self.host('save', name='sidsynth-stage5-audition')
        self.results['audition_song'] = str(ROOT / 'sidsynth-stage5-audition.xrns')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid', type=int, required=True)
    parser.add_argument('--resume-examples', action='store_true')
    args = parser.parse_args()
    checks = Stage5(args.pid)
    if args.resume_examples:
        previous = json.loads((ROOT / 'stage5-host-results.json').read_text())
        assert previous['module_sha256'] == checks.results['module_sha256'] and previous['pid'] == args.pid
        assert previous['stop_start_cycles'] == 3 and previous['two_instance_recall']
        checks.results = previous
    try:
        checks.detach_monitor()
        if args.resume_examples:
            checks.export_examples()
            checks.results['offline_render'] = checks.host('render', name='sidsynth-stage5-offline')
            assert checks.results['offline_render']['state'] == 'unavailable'
            checks.results['passed'] = True
            print('PASS: resumed native demo exports; earlier identical-module host checks retained')
        else:
            checks.run()
    finally:
        checks.restore_monitor()
        (ROOT / 'stage5-host-results.json').write_text(json.dumps(checks.results, indent=2))
