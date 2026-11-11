# Flags and expression controls — Phase 1

The Phase 1 parser accepts the flag string and never crashes merely because a flag is unknown. It does not yet apply UTAU effect flags or the proposed custom expression flags. A warning is printed when a non-empty flag string is supplied.

| Flag / control | Intended meaning from project specification | Phase 1 status |
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
| `ng` | Note Gain | Not applied; use positional volume for Phase 1 gain. |
| `nh` | Noise level | Not applied. |
| `hb` | Harmonic balance | Not applied. |
| modulation | Preserve source pitch variation | Parsed and validated; not applied. |
| pitch-bend tokens | Continuous pitch movement | Parsed as opaque trailing tokens; not applied. |

Do not configure these flags as active production controls until a later implementation documents their ranges and tests their behavior. The numeric ranges in a future flag implementation will be project-defined and will not be represented as Crypton's proprietary scales.
