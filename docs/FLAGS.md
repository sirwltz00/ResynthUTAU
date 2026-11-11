# Flags and expression controls — Phase 5 roadmap

The Phase 4 parser accepts the flag string and never crashes merely because a flag is unknown. The conventional renderer currently warns when a non-empty flag string is supplied; the following controls are planned for Phase 5 and are not active yet.

| Flag / control | Intended meaning from project specification | Phase 4 status |
|---|---|---|
| `g` | Gender/formant adjustment | Parsed as part of the string; not applied. |
| `t` | Fine pitch adjustment | Parsed as part of the string; not applied. |
| `B` | Breathiness | Parsed as part of the string; not applied. |
| `H` | Low-pass filtering | Parsed as part of the string; not applied. |
| `Y` | Noise suppression | Parsed as part of the string; not applied. |
| `br` | Custom breathiness | Not applied. |
| `sf` | Super Formant Shifter | Not applied. |
| `vv` | Voice Voltage-inspired control | Not applied. |
| `cr` | Consonant Rate | Not applied. |
| `as` | Attack Speed | Not applied. |
| `ng` | Note Gain | Not applied; use positional volume for current note gain. |
| `nh` | Noise level | Not applied. |
| `hb` | Harmonic balance | Not applied. |
| modulation | Preserve source pitch variation | Parsed and validated; not applied yet. |
| pitch-bend tokens | Continuous pitch movement | Parsed as opaque trailing tokens; not applied yet. |

Do not configure these flags as active production controls until a later implementation documents their ranges and tests their behavior. The numeric ranges in a future flag implementation will be project-defined and will not be represented as Crypton's proprietary scales.
