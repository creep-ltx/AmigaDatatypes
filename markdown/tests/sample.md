# markdown.datatype test

A paragraph with **bold**, *italic*, ***both***, `inline code`, ~~struck~~ and a
[link to Aminet](https://aminet.net). Bare address: https://github.com.
Some UTF-8: “quotes”, en – dash, em — dash, ellipsis…, café, Łódź, € and ✓.

## Lists

- First item
- Second item with a longer text that should wrap nicely when the window is
  narrow, keeping the indent under the bullet
  - Nested item
    - Deeper still
1. One
2. Two
   1. Two point one

- [x] Done task
- [ ] Open task

### A quote

> Quoted text, with **bold** inside.
>
> > A nested quote.

#### Code

```c
int main(void)
{
    /* a comment */
    return 0;
}
```

    indented code block

##### Table

| Kickstart | Year | Datatypes |
|---|:---:|---:|
| 1.3 | 1988 | none |
| 3.0 | 1992 | 39 |
| 3.2 | 2021 | 47 |

###### Smallest heading

---

> [!NOTE]
> An alert, as GitHub writes it.

Line one  
Line two after a hard break.

<b>HTML bold</b> and <!-- a hidden comment --> the end.
