#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Create deterministic Linux binary and complete offline-build source archives.

Runs CMake's package installation into a temporary project directory. Includes
the original hash-verified dependency source archives and all license notices.
Does not install to the user's plugin directory or publish anything.
"""
import argparse
import gzip
import hashlib
import json
import re
import shutil
import subprocess
import tarfile
import tempfile
from pathlib import Path

PROJECT = Path(__file__).resolve().parent.parent


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def archive(directory, target):
    # Stable ordering, ownership, timestamps and gzip header; permissions remain.
    with target.open('wb') as raw, gzip.GzipFile(filename='', mode='wb', fileobj=raw, mtime=0) as compressed:
        with tarfile.open(fileobj=compressed, mode='w') as tar:
            for path in [directory, *sorted(directory.rglob('*'))]:
                info = tar.gettarinfo(str(path), str(path.relative_to(directory.parent)))
                info.uid = info.gid = info.mtime = 0
                info.uname = info.gname = ''
                if path.is_file():
                    with path.open('rb') as content:
                        tar.addfile(info, content)
                else:
                    tar.addfile(info)


def package(build, dependencies, output):
    version = re.search(r'project\(SIDSynth VERSION ([\d.]+)', (PROJECT / 'CMakeLists.txt').read_text())[1]
    assert not (build / 'CMakeCache.txt').read_text().count('SIDSYNTH_SANITIZERS:BOOL=ON'), 'Do not distribute sanitizer builds'
    module = build / 'SIDSynth_artefacts/Release/VST3/SIDSynth.vst3'
    # The SDK writes JSON5-style trailing commas in moduleinfo.json.
    metadata = (module / 'Contents/Resources/moduleinfo.json').read_text()
    declared = re.search(r'"Version"\s*:\s*"([^"]+)"', metadata)
    assert declared and declared[1] == version, 'Build version does not match source'
    output.mkdir(parents=True, exist_ok=True)
    for name, digest in (
        ('juce-9.0.5.tar.gz', '8237b2976871982eb9107fd67b576d95bf02355d99540a0127c4e7c001ecd9f5'),
        ('libresidfp-1.3.0.tar.gz', '0c38824bb51a8c5b62bb5400c290c60cc4f8e7c2d145c559e6449f7d42cf5358')):
        assert sha(dependencies / name) == digest, 'Pinned dependency archive mismatch: ' + name
    with tempfile.TemporaryDirectory(prefix='sidsynth-release-', dir=build) as temp:
        temp = Path(temp)
        binary = temp / ('SIDSynth-' + version + '-linux-x86_64')
        source = temp / ('SIDSynth-' + version + '-source')
        binary.mkdir()
        source.mkdir()
        subprocess.run(['cmake', '--install', str(build), '--prefix', str(binary), '--component', 'Plugin'], check=True)
        for name in ('README.md', 'LICENSE', 'AGENTS.md', 'CMakeLists.txt', '.gitignore'):
            shutil.copy2(PROJECT / name, source)
        for name in ('src', 'tests', 'cmake', 'docs', 'tools', 'design', 'third_party', 'examples', '.github'):
            shutil.copytree(PROJECT / name, source / name, ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
        shutil.copytree(dependencies, source / 'dependency-downloads',
                        ignore=lambda path, names: [n for n in names if n not in ('juce-9.0.5.tar.gz', 'libresidfp-1.3.0.tar.gz')])
        for name in ('docs', 'examples', 'third_party'):
            shutil.copytree(PROJECT / name, binary / name, ignore=shutil.ignore_patterns('__pycache__'))
        shutil.copy2(PROJECT / 'LICENSE', binary)
        (binary / 'INSTALL.txt').write_text(
            'SIDSynth ' + version + ' — Linux x86_64 VST3 preview\n\n'
            'Close loaded SIDSynth instances. Copy SIDSynth.vst3 to ~/.vst3/\n'
            'and rescan VST3 plugins in Renoise. No root access is needed.\n'
            'Remove only ~/.vst3/SIDSynth.vst3 to uninstall.\n\n'
            'Open examples/sidsynth-three-parts-example.xrns for one SID plus two aliases.\n'
            'Load the .xrni patches into an empty Renoise instrument slot.\n'
            'See docs/RELEASE.md for validation and known limits.\n'
            'The accompanying source archive includes the pinned dependency sources.\n')
        (source / 'BUILD-OFFLINE.txt').write_text(
            'Install the system build dependencies listed in docs/BUILD.md, then:\n\n'
            'cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release '
            '-DSIDSYNTH_DEPENDENCY_ARCHIVE_DIR="$PWD/dependency-downloads"\n'
            'cmake --build build --parallel 4\n'
            'ctest --test-dir build --output-on-failure\n\n'
            'Dependency archive hashes are verified during configuration.\n')
        manifest = {'version': version, 'platform': 'Linux x86_64',
                    'module_sha256': sha(module / 'Contents/x86_64-linux/SIDSynth.so'),
                    'state_schema': 3, 'parameter_count': 67,
                    'dependencies': {p.name: sha(p) for p in sorted((source / 'dependency-downloads').iterdir())}}
        (binary / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        for directory in (binary, source):
            archive(directory, output / (directory.name + '.tar.gz'))
    archives = sorted(output.glob('SIDSynth-' + version + '-*.tar.gz'))
    (output / 'SHA256SUMS').write_text(''.join(sha(p) + '  ' + p.name + '\n' for p in archives))
    print(json.dumps(manifest, indent=2))
    print(output / 'SHA256SUMS')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=PROJECT / 'build-clean')
    parser.add_argument('--dependencies', type=Path, default=PROJECT / 'build/dependency-downloads')
    parser.add_argument('--output', type=Path, default=PROJECT / 'build-release')
    args = parser.parse_args()
    package(args.build.resolve(), args.dependencies.resolve(), args.output.resolve())
