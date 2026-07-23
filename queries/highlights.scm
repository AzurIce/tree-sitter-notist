"#" @punctuation.special

"[[" @punctuation.bracket
"]]" @punctuation.bracket
"[" @punctuation.bracket
"]" @punctuation.bracket
"(" @punctuation.bracket
")" @punctuation.bracket
"," @punctuation.delimiter
"=" @operator
"::" @punctuation.delimiter
"@" @attribute

(heading
  marker: (heading_marker) @punctuation.special
  body: (line_text) @title.markup)

[(list_item marker: (list_marker) @punctuation.list_marker.markup)
 (enum_item marker: (enum_marker) @punctuation.list_marker.markup)
 (task_item marker: (task_marker) @punctuation.list_marker.markup)]

(wiki_target) @link_uri
[(line_comment)
 (block_comment)] @comment

(call_expression
  function: (qualified_name) @function.call)

(named_argument
  name: (identifier) @variable.parameter)

(none) @constant.builtin
(boolean) @boolean
(integer) @number
(float) @number
(string) @string
(escape_sequence) @string.escape

[(escaped_inline_open)
 (escaped_multiline_open)
 (raw_inline_open)
 (raw_multiline_open)
 (string_close)] @punctuation.bracket

(inline_raw) @string.special
(fence_open) @punctuation.bracket
(fence_info) @label
(fence_content) @string.special
(fence_close) @punctuation.bracket

(id_attribute
  name: (identifier) @label)

(tag_attribute
  name: (identifier) @tag)

(class_attribute
  name: (identifier) @type)

(property_attribute
  key: (identifier) @property)

(attribute_string) @string
