; Lexical

(comment) @comment
(string) @string
(integer) @number
(escape) @string.escape
(raw) @string
(math) @string.special
(autolink) @link_text
(list_marker) @punctuation.special
(annotation "@" @punctuation.special)
(annotation "!" @punctuation.special)

["true" "false" "none"] @constant.builtin
["let" "use" "wasm" "as" "if" "else"] @keyword

[
  "=="
  "!="
  "<"
  ">"
  "<="
  ">="
  "+"
  "-"
  "*"
  "/"
  "="
  "=>"
  "->"
] @operator

["(" ")" "[" "]" "{" "}"] @punctuation.bracket
[";" "," ":" "::" "." "#"] @punctuation.delimiter

; Markup

(heading_marker) @punctuation.special
(section title: (title) @title)

(strong "*" @punctuation.special) @emphasis.strong
(emphasis "_" @punctuation.special) @emphasis

(wikilink
  "[[" @punctuation.special
  "]]" @punctuation.special)
(wikilink_module) @module
(wikilink label: (string) @label)

; References and targets

(item_target module: (qualified_name) @module)
(item_target label: (string) @label)

(use_leaf alias: (identifier) @variable)
(use_glob "*" @punctuation.special)
(declaration_wasm path: (string) @string.special)

; Definitions and uses

(declaration_let name: (identifier) @variable)

(parameter name: (identifier) @variable.parameter)
(named_argument name: (identifier) @variable.parameter)

(dict_entry key: (identifier) @property)
(field_access field: (identifier) @property)
(interpolation_field field: (identifier) @property)

(type name: (identifier) @type)

(call_expression function: (_) @function.call)
(interpolation_call function: (_) @function.call)
