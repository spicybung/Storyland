# Bundled opening movies

These are converted copies of the two supplied MP4 videos. The original MP4s remain outside the runtime package.

| File | Use | Video | Audio | File size |
| --- | --- | --- | --- | --- |
| `intro_pc.pss` | VCS game intro with the PC shader profile | 1920 × 1680, 8:7, 30 fps, 2,527 frames | Stereo PCM16LE, 48 kHz, 84.24 seconds | 94,964,396 bytes |
| `reigns_studio.pss` | Studio sting after native studio stings and before the LCS/VCS game intro, every shader profile | 1920 × 1080, 16:9, 30 fps, 360 frames | Stereo PCM16LE, 16 kHz, 12 seconds | 25,365,862 bytes |

VCS plays its native sting, Reigns Studio, then the selected native or PC HD
intro. LCS plays RSNY, RSLEEDS, RSNORTH, Reigns Studio, then `DM.PSS`.
Storyland's own application startup splash and intro tune remain independent.

## Container and preservation

Both files contain MPEG-2 video in a program stream with native Sony PSS audio: private stream `0xBD`, `FF A0 00 00` substream headers, and `SShd`/`SSbd` PCM data with 512-byte channel interleave. They target Storyland's software decoder. Their HD dimensions are not intended for the original PS2 hardware decoder.

The conversion preserves every source frame, the original frame dimensions, square pixels, and 1.0× timing. It does not crop, stretch, scale, or resample the supplied media. All opening movies use the current game viewport or window ratio: the complete movie frame is fitted in the foreground, with a dimmed cover copy behind it filling any remaining area. This preserves the HD intro's credits and final logo on wide displays.

Audio samples match the decoded 16-bit source audio through each source track's declared endpoint. The final AAC padding beyond that endpoint is excluded: 256 sample frames for the HD intro and 512 for Reigns. No audible section is intentionally shortened. The Reigns track remains at its original 16 kHz; resampling cannot recover detail absent from that source.

MPEG-2 video is a lossy conversion. The HD intro uses quantizer 3, and Reigns uses quantizer 2. Full-frame source comparisons measured 52.225565 dB PSNR / 0.997677 SSIM for the HD intro and 48.324548 dB / 0.992604 for Reigns. Both files are below 100 MiB.

## Validation and playback

Every video frame was decoded and compared. Native Storyland audio decoding was checked sample by sample through filesystem, fully indexed callback, and incremental streaming readers, including seeks, with AddressSanitizer and UndefinedBehaviorSanitizer enabled. Independent Sony ADS decoding also reproduced the same PCM data. A separate container scan verified packet boundaries, nondecreasing system clocks, native audio headers, matching first audio/video timestamps, the program-end marker, and unchanged MPEG video payloads during PSS multiplexing.

The portable player discovers audio packets as playback needs them, allowing compressed APK movie assets to stream without first inflating the complete file. The bundled audio headers require reading only through source offset 2,128. Playback keeps packet offsets and bounded decode buffers; it does not extract movies to disk or retain a complete decoded movie. Split PCM blocks are consumed before discovering the next packet to avoid repeated decompression from the beginning at packet boundaries.

The HD video's MPEG program-stream duration estimate can be 84.166667 seconds because its final reordered frames share PES packets. The complete video contains 2,527 frames (84.233333 seconds), and its audio ends at 84.24 seconds. Playback completion should follow decoded EOF and the audio endpoint rather than that estimate alone.

`manifest.json` records input/output SHA-256 hashes, exact codec parameters, dimensions, durations, and validation metrics. Source format references are the [PSS format description](https://azuco.sakura.ne.jp/fao/doc/pss.html) and the [FFmpeg Sony ADS decoder](https://www.ffmpeg.org/doxygen/trunk/ads_8c_source.html).
