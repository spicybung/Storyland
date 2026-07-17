# GAME.DTZ streaming table fix

`GAME(38).DTZ` exposed an overlap in Storyland's sentinel-name scanning. Storyland correctly found the MDL and XTX name-table starts, but the MDL pass continued through the XTX table and overwrote those names. This made the paired IMG appear to go out of alignment directly after `plr.xtx` and `plr.mdl`; the sector offsets themselves were not the cause.

The DTZ header pointer at `0x7C` leads to the streaming-info header. Its cumulative limits are now used as hard table boundaries:

- `+0x08`: last model slot
- `+0x0C`: last texture slot
- `+0x14`: total streaming slots

The limits are accepted only when they are monotonic and within a conservative streaming-slot range. If the metadata is absent or invalid, the parser retains its conservative legacy scan.

Regression sequence from the supplied archive:

```text
plr.xtx, plr.mdl,
cop.xtx, cop.mdl,
swat.xtx, swat.mdl,
fbi.xtx, fbi.mdl,
army.xtx, army.mdl,
medic.xtx, medic.mdl
```

The parser regression probe reports 1,580 directory entries without the earlier cross-table overwrite.
