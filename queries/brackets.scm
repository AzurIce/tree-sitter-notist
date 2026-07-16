("[" @open
  "]" @close)

("(" @open
  ")" @close)

[(escaped_inline_open)
 (escaped_multiline_open)
 (raw_inline_open)
 (raw_multiline_open)
 (fence_open)] @open

[(string_close)
 (fence_close)] @close
