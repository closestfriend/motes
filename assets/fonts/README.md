# Embedded fonts

Unmodified static TTFs from the Google Fonts repository (https://github.com/google/fonts, `ofl/`),
fetched 2026-10-07. All SIL Open Font License 1.1; the license texts sit beside the fonts.

| File | Source path | SHA-256 |
|---|---|---|
| BarlowCondensed-Medium.ttf | ofl/barlowcondensed/ | 262bd143292ce479ee0cd09a42b47ab173fca8e9c6eb5ed0b5c8a845bc371d17 |
| BarlowCondensed-SemiBold.ttf | ofl/barlowcondensed/ | 7b619d14bc2327509a9ef32b0890f709626f7ecc9ff61191c2a4314c5499d2d9 |
| IBMPlexMono-Regular.ttf | ofl/ibmplexmono/ | 6a3412f058c7d8dfd9170c41e85ade48e5156ecb89356110ca57a0a27734af46 |
| OFL-BarlowCondensed.txt | ofl/barlowcondensed/OFL.txt | 186d750eb496a4c17a76385f82be6aea2ac1cf2de074a811d63786cf374ea73f |
| OFL-IBMPlexMono.txt | ofl/ibmplexmono/OFL.txt | 7e6b2818edbd8f6a01ae80641cc8f16a51080d08fb4e532be3a0b6f74adb07da |

Embedded with `juce_add_binary_data` (namespace `MotesFonts`). The editor falls back to the system
sans/monospace font if a face fails to load.
