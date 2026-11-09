;; From nvim-treesitter/nvim-treesitter
[
  (code_span)
  (link_title)
] @text.literal

[
  (emphasis_delimiter)
  (code_span_delimiter)
] @punctuation.delimiter

(emphasis) @text.emphasis

(strong_emphasis) @text.strong

[
  (link_destination)
  (uri_autolink)
] @text.uri

[
  (link_label)
  (link_text)
  (image_description)
] @text.reference

[
  (backslash_escape)
  (hard_line_break)
] @string.escape

(image ["!" "[" "]" "(" ")"] @punctuation.delimiter)
(inline_link ["[" "]" "(" ")"] @punctuation.delimiter)
(shortcut_link ["[" "]"] @punctuation.delimiter)

; NOTE: extension not enabled by default
; (wiki_link ["[" "|" "]"] @punctuation.delimiter)

(strikethrough) @text.strike
(comment) @comment
(string) @string
(path_literal) @string.special.path
(number) @number
(boolean) @constant.builtin
(math) @string.special
(call name: (path) @function.call)
(reference name: (path) @variable)
["#import" "as" "let" "if" "else" "match" "comptime"] @keyword
["for" "in" "#for"] @keyword
(for_expression binding: (identifier) @variable.parameter)
(markup_for_expression binding: (identifier) @variable.parameter)
(member_expression member: (identifier) @property)
(invoke_expression function: (member_expression member: (identifier) @function.call))
(import_member name: (identifier) @variable)
(import_member alias: (identifier) @variable)
(invoke_expression function: (path) @function.call)
(binary_expression operator: _ @operator)
(unary_expression operator: _ @operator)
(named_argument name: (_) @variable.parameter)
(dict_entry key: (_) @property)
["(" ")" "[" "]"] @punctuation.bracket
["#" "@" "@!" "," ":" "." ";"] @punctuation.delimiter
(negative_number "-" @operator)
