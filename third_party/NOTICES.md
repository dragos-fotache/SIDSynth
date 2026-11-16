# Third-party software

SIDSynth's original code is licensed under **AGPL-3.0-only**; see `LICENSE`.
The open-source JUCE license is selected. No commercial JUCE license is used.

| Dependency | Pinned source | License used |
| --- | --- | --- |
| JUCE 9.0.5 | [`d60b7a78030661574a615b586d9325ba7a7fa7dd`](https://github.com/juce-framework/JUCE/tree/d60b7a78030661574a615b586d9325ba7a7fa7dd) | AGPL-3.0-only |
| libresidfp 1.3.0 | [`5393471a02a505ff43aa710148b2d35289b72c31`](https://github.com/libsidplayfp/libresidfp/tree/5393471a02a505ff43aa710148b2d35289b72c31) | GPL-2.0-or-later, exercising the GPLv3 option for combination with AGPLv3 |
| VST3 SDK 3.8.0 | Vendored in the pinned JUCE source | MIT |

JUCE copyright: Raw Material Software Limited. Its exact license statement is
retained in `licenses/JUCE-LICENSE.md`. The full upstream dependency inventory,
including vendored versions, source locations and licenses, is retained in
`JUCE.spdx.json`. This inventory also lists JUCE components that SIDSynth does
not build or use. JUCE's modules retain their embedded third-party notices.

libresidfp copyright: Dag Lem (2000–2011), Antti Lankila (2007–2010), and Leandro
Nini (2010–2026). See `licenses/libresidfp-AUTHORS` and
`licenses/libresidfp-GPL-2.0.txt`. The GPLv3 text is retained as
`licenses/GPL-3.0.txt`; GPLv3 section 13 and AGPLv3 section 13 govern the
combination, with each component retaining its own license.

The Linux plugin's JUCE graphics/core dependencies include zlib 1.3.2 (Zlib),
HarfBuzz 15.0.0 (MIT), SheenBidi 2.9.0 (Apache-2.0), LunaSVG 3.5.0 and PlutoVG
1.3.2 (MIT), libpng 1.6.58 (libpng-2.0), Independent JPEG Group software 10.0
(IJG), and libwebp 1.6.0 (BSD-3-Clause). Their upstream license/notice files are
copied without modification beneath `licenses/juce-vendored/`, preserving source
paths. This software is based in part on the work of the Independent JPEG Group.
Additional embedded source notices remain in the pinned source distribution.

Stage 4 uses the MIDI keyboard in JUCE's `juce_audio_utils` module, which
also builds JUCE audio-format support. Its pinned-source inventory includes
FLAC 1.5.0, libogg 1.3.6, libvorbis 1.3.7, Opus 1.6.1,
opusfile 0.12-59-g6dfd29e and libopusenc 0.3 (BSD-3-Clause).
Their original notices are retained beneath
`licenses/juce-vendored/modules/juce_audio_formats/codecs/`. JUCE's built-in MP3,
WAV and AIFF implementations use JUCE's selected AGPL route. The plugin does
not offer a file player or open a separate audio device.

The SDK root, base, pluginterfaces and public.sdk MIT notices are retained.
VST is a trademark of Steinberg Media Technologies GmbH.

The build uses the system FreeType, Fontconfig, X11 and standard Linux runtime
libraries. These are system dependencies, not bundled copies. Web browser and
curl integration are disabled. No external SID player, C64 ROM, or musical data
is included.

`cmake/Dependencies.cmake` pins archive SHA-256 digests as well as source commits.
Its CMake adapter uses the libresidfp source list from the pinned `Makefile.am`
and configures its two generated headers with release settings. Stage 3 builds
from a separate source copy beneath the build tree and adds only a forward
`SidBackend` declaration and friend declaration to `SID.h`. The downloaded
pinned source stays immutable. `src/SidBackend.cpp` caches both models' existing
DAC/waveform tables and PAL/NTSC resamplers during construction/preparation,
then copies/swaps these configurations during playback without the allocation
and mutex in upstream's public setters. Exactly one SID core remains. No DSP
algorithm or upstream license notice is removed or changed. The adapter is
tightly coupled to this pinned version and the configure step checks its patch
anchor; changes to upstream private fields require adapter review.
It does not use a system-installed, potentially different libresidfp.

Before distributing binaries, provide the corresponding SIDSynth source,
build files and these exact dependency sources under their respective licenses.
The local installation target includes the project license and retained notices.
