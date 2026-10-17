# The IBM VGA font

`IBM_VGA_8x16.raw` (256 characters, 16 rows) and `IBM_VGA_8x8.raw` (256 characters, 8 rows: the VGA's 8x8
ROM font, the one of its 80x50 mode) are the glyphs of the IBM VGA's character ROM, taken from
**The Ultimate Oldschool PC Font Pack** by VileR (https://int10h.org/oldschool-pc-fonts/), licensed under
the Creative Commons Attribution-ShareAlike 4.0 International License (`LICENSE-CC-BY-SA-4.0.txt`).

They are stored one byte a row, bit 7 the left pixel, character after character. `tools/mkfont.py` makes
them into the Amiga bitmap font `ibmvga.font` (sizes 8 and 16) without changing a pixel; that font is
shared under the same licence.
