# reLCS reference audit

Reference repository: https://github.com/Gri-rasp/reLCS-Separate-Version-

Files reviewed:

- src/audio/AudioLogic.cpp — weapon-specific firing/reload events; surface-specific
  footsteps; SFX_BANK_0 selection; distinction between events and samples.
- src/audio/AudioSamples.h — evaluated sample enum values and TOTAL_AUDIO_SAMPLES.
- src/audio/sampman.h — 20-byte tSample descriptor.
- src/audio/sampman_oal.cpp — SDT/RAW access and bank-relative offsets.
- src/audio/soundlist.h — engine sound-event names.
- src/weapons/WeaponInfo.h — CWeaponInfo layout and weapon flags.

This fork retains Vice City audio sample names and an explicit LCS TODO.
Its numeric sample IDs are not verified Stories PS2/PSP IDs. Storyland therefore
only offers its inherited numeric profile for an SFX-named 20-byte/9941-entry
legacy PC table. It does not apply these numbers to Stories 12-byte banks.

Enum comments are stale in places. Evaluating the actual enum gives BODY_LAND=333,
SPLASH_1=324, WEAPON_LEFT=361, and FOOTSTEP_GRASS_1=496. Copying their nearby numeric
comments would produce incorrect routing even for the inherited PC profile.

No executable game code or audio assets from reLCS are bundled. Storyland's
implementation is separate; the root project license continues to apply.
