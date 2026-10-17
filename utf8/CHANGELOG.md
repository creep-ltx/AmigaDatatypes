# utf8.datatype changes

## 0.1b3 (17.10.2026)

- **UTF-8 late in a long file:** a file of plain ASCII for its first 16 KB is now taken and looked through to
  the end (four bytes at a time), so a changelog with "Zoë" on its last line shows her right. Plain ASCII and
  Latin-1 files taken this way are shown as they are. Cost on a 7 MHz 68000: about 0.6 s for 100 KB.
- **Less memory while opening:** the text is converted where it lies, line by line, instead of into a
  second buffer the size of the file. If memory runs out part way, the rest is shown as it was.
- The new recognition code takes effect after a restart (AddDataTypes REFRESH keeps the old one).

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
