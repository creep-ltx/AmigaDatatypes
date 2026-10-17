# utf8.datatype changes

## 0.1b2 (17.10.2026)

- **Other scripts spelt in Latin letters** with utf8.library 0.1b2: Cyrillic, Greek, Japanese kana and
  Korean ("Привет" Privet, "서울" seoul) instead of ?.
- Text whose Latin-1 is longer than its UTF-8 (Korean, Russian щ) gets a new buffer; the old one is
  freed (text.datatype does not free a buffer it is given another for).
- Faster with utf8.library 0.1b2: a 100 KB worst-case file opens in 24 s instead of 40 on a 68000.

## 0.1b1 (17.10.2026)

- The first version: UTF-8 text files recognised by their contents (recognition code in the
  descriptors) and shown in Latin-1 through ascii.datatype, converted by utf8.library: the nearest stand-in
  for what Latin-1 lacks, CR LF made LF, the byte order mark dropped. One build for every Amiga (68000,
  Kickstart 3.0+).
- Markdown, HTML and AmigaGuide files are left to their own datatypes.
- Tested on Workbench 3.1 and 3.2.1 (68020) and on a 68000 A600 with 3.1, in FS-UAE.
