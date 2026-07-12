"#" @punctuation.special
"#[" @punctuation.bracket
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

(raw_open) @punctuation.bracket
(raw_close) @punctuation.bracket
(raw_content) @string.special
(backtick_raw) @string.special

(wiki_target) @link_uri

(content_call
  function: (qualified_name) @function.call)

(raw_call
  function: (qualified_name) @function.call)

(named_argument
  name: (identifier) @variable.parameter)

(none) @constant.builtin
(boolean) @boolean
(integer) @number
(float) @number
(string) @string
(escape_sequence) @string.escape

(id_attribute
  name: (identifier) @label)

(tag_attribute
  name: (identifier) @tag)

(class_attribute
  name: (identifier) @type)

(property_attribute
  key: (identifier) @property)

(attribute_string) @string
