# The Amiga art fonts

The eight `*.raw` files are TrueSchool's **Multi Platform Amiga Fonts v1.0** by dMG/t!s^dS!
(https://github.com/rewtnull/amigafonts), raw conversions: 256 characters (Latin-1) of 16 rows each, one byte
a row, bit 7 the left pixel, every row of the 8-row original doubled. `../pcansi/tools/mkfont.py` makes each
into an Amiga bitmap font with an 8-row size (every other row: the original) and a 16-row size (as stored),
without changing a pixel:

| File | Font | SAUCE name |
|---|---|---|
| `Topaz_a500_v1.0.raw` | `ansi_topaz1.font` | Amiga Topaz 1 |
| `TopazPlus_a500_v1.0.raw` | `ansi_topaz1p.font` (the default) | Amiga Topaz 1+ |
| `Topaz_a1200_v1.0.raw` | `ansi_topaz2.font` | Amiga Topaz 2 |
| `TopazPlus_a1200_v1.0.raw` | `ansi_topaz2p.font` | Amiga Topaz 2+ |
| `MicroKnight_v1.0.raw` | `ansi_mk.font` | Amiga MicroKnight |
| `MicroKnightPlus_v1.0.raw` | `ansi_mkp.font` | Amiga MicroKnight+ |
| `P0T-NOoDLE_v1.0.raw` | `ansi_noodle.font` | Amiga P0T-NOoDLE |
| `mOsOul_v1.0.raw` | `ansi_mosoul.font` | Amiga mOsOul |

From the pack's README (section 14, LEGAL STUFF): the fonts are released under the GPL-FE licence (the GPL
with the font exception, https://www.gnu.org/licenses/gpl-faq.html#FontException: using the fonts in a
document or program does not by itself make that document or program GPL). Topaz is © Amiga Inc.; the raw
versions of Topaz and TopazPlus are © dMG/t!s^dS!. P0T-NOoDLE is © Leo "Nudel" Davidson, MicroKnight is ©
Niels Krogh "Nölb/Grafictive" Mortensen, mO'sOul is © Desoto/Mo'Soul; their raw versions are © dMG/t!s^dS!.
The GPL: https://www.gnu.org/licenses/gpl-3.0.txt
