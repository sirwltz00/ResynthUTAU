# Changelog

## 0.3.1 — Phase 3 resynthesis quality update

- Changed the default harmonic phase-observation strength from 0.85 to 1.0 so the oscillator path follows the measured inter-frame phase correction fully by default.
- Rotated each extracted harmonic phase from the FFT frame origin to the frame's center-sample reference. This keeps phase measurements aligned when adaptive spectral-window sizes change.
- Added a clean 220 Hz pure-sine regression that measures residual energy against a best-fit sine, rather than relying only on F0 and adjacent-sample checks.
- Added phase-reference regression coverage across 1024-, 2048-, and 4096-sample analysis windows.
- Updated DSP and testing documentation.

This update does not implement Phase 4. UTAU timing, phoneme transitions, consonant protection, and native renderer integration remain outstanding. Full phase following has not yet been perceptually evaluated across real voicebanks; confidence weighting for weak/noisy partials remains a future quality task.
