# Flags and expression controls — Phase 5

Phase 5 adds an independently addressable expression layer to the conventional renderer. The scales below are **ResynthUTAU's documented clean-room scales**, not Crypton's or Yamaha's proprietary parameter values. UTAU flags are not perfectly uniform across resamplers; use the profile documented here when using ResynthUTAU.

## Standard-compatible flags

| Flag | Range | Default | ResynthUTAU behavior |
|---|---:|---:|---|
| `g` | -100 to +100 | 0 | Gender/formant control. Positive values lower the spectral envelope; negative values raise it. The mapping is -0.06 formant semitones per g unit. |
| `t` | -100 to +100 | 0 | Fine pitch adjustment in 10-cent increments per unit. |
| `B` | 0 to 100 | 0 | Breathiness amount. Adds aperiodic energy and retains more original breath texture. |
| `H` | 0 to 99 | 0 | Low-pass filtering. Increasing values lower the cutoff. |
| `Y` | 0 to 100 | 0 | Noise suppression. Increasing values reduce synthesized aperiodic energy and the retained dry noise floor. |

The `g`, `t`, `B`, `H`, and `Y` characters are case-sensitive. In particular, uppercase `B` is breathiness.

## Custom expression flags

The parser accepts adjacent expressions, and comma, semicolon, space, underscore, or pipe separators. A leading `+` or `-` is allowed for signed values. Examples: `g+20t-2B35` and `br40,sf+3,vv120`.

| Flag | Range | Default | Meaning |
|---|---:|---:|---|
| `br` | 0–100 | 0 | Breathiness; custom alias/control combined with `B` using the higher value. |
| `sf` | -24–+24 semitones | 0 | Super Formant Shifter-inspired spectral-envelope movement, independent of F0. Adds to the `g`-derived formant movement. |
| `vv` | 0–200 | 100 | Voice Voltage-inspired control. Changes overall harmonic excitation and gently tilts the upper partials. |
| `cr` | 50–200 | 100 | Consonant Rate. Scales the fixed consonant region duration; the requested total note length still wins when explicitly supplied. |
| `as` | 25–200 | 100 | Attack Speed. Lower values add a slower voiced-onset ramp; higher values add a restrained onset emphasis. |
| `ng` | 0–400 | 100 | Note Gain, multiplied with the positional UTAU volume argument. |
| `nh` | 0–200 | 100 | Noise Level, scaling the synthesized aperiodic component. |
| `hb` | 0–200 | 100 | Harmonic Balance, scaling the harmonic component relative to the aperiodic component. |
| `dy` | 0–200 | 100 | Dynamics. Alters the smoothed amplitude contour with bounded expansion/compression. |
| `vib` | 0–100 | 0 | Vibrato depth. At 100, the maximum depth is ±50 cents. Vibrato fades in and out smoothly. |
| `vr` | 2–10 Hz | 5.5 Hz | Vibrato rate. |
| `sb` | 0–200 | 100 | Spectral Brightness. 100 is neutral; lower/higher values attenuate/emphasize the high-frequency portion. |
| `dc` or `dec` | 0–100 | 0 | Decay. Applies a smooth ending fade up to 250 ms. |

## Modulation and pitch automation

The positional modulation argument is 0–100. It controls how much of the source F0 contour is retained around the median voiced F0: 0 is flat (the default), and 100 retains the measured source contour. It is not the vibrato control.

The trailing pitch-bend arguments are interpreted as cents offsets. Values are linearly interpolated and integrated into the continuous harmonic oscillator phase; oscillator phase is not restarted at control points.

- Without a tempo marker, points are spaced by 256 source samples, mapped through the note's time-stretch ratio.
- A leading `Q<tempo>` marker selects a grid of 96 points per quarter note. For example, `Q120` uses 120 BPM.
- A standalone `Q` followed by a numeric tempo is also accepted.
- Each pitch point is limited to -2400…+2400 cents. Invalid points are ignored with a warning.

The legacy Q-grid/256-sample distinction is based on publicly documented descriptions of the classic UTAU resampler interface. ResynthUTAU's point values are explicitly interpreted as cents offsets; verify the result in your host project before relying on an automation sequence from another engine.

## Unknown flags

Unknown flag tokens are ignored and reported as warnings. They do not crash the renderer. No behavior is claimed for flags belonging to other resamplers, including engine-specific flags from moresampler or other third-party tools.
