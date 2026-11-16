#!/usr/bin/env python3
"""Create a scratch one-instance/two-alias song from a Renoise-saved seed.

Renoise's Lua API exposes alias_instrument_index as read-only. Derive the two
alias links in the scratch XRNS, then load and verify them through the host API.
"""
import argparse
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent / "work/renoise-host"
SID_ID = "ABCDEF019182FAEB5369647353796469"


def create(seed, output):
    assert seed.resolve().is_relative_to(ROOT) and output.resolve().is_relative_to(ROOT)
    assert seed.resolve() != output.resolve()
    with zipfile.ZipFile(seed) as source:
        root = ET.fromstring(source.read("Song.xml"))
        instruments = root.findall("./Instruments/Instrument")
        plugins = root.findall(".//PluginGenerator/PluginDevice")
        assert len(plugins) == 1 and plugins[0].findtext("PluginIdentifier") == SID_ID
        assert len(instruments) >= 3
        for index, instrument in enumerate(instruments[:3]):
            generator = instrument.find("PluginGenerator")
            assert generator is not None
            generator.find("Channel").text = str(index)
            generator.find("AutoSuspend").text = "false"
            if index:
                assert generator.find("PluginDevice") is None
                generator.find("AliasInstrumentIndex").text = "0"
        with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as target:
            for name in source.namelist():
                target.writestr(name, ET.tostring(root, encoding="utf-8", xml_declaration=True)
                               if name == "Song.xml" else source.read(name))
    print(output)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seed", type=Path, default=ROOT / "sidsynth-seed.xrns")
    parser.add_argument("--output", type=Path, default=ROOT / "sidsynth-aliases.xrns")
    args = parser.parse_args()
    create(args.seed, args.output)
