(comment) @comment
(string) @string
(path_literal) @string.special.path
(number) @number
(boolean) @constant.builtin
(escape) @string.escape
(explicit_break) @punctuation.special
(raw_inline) @string
(raw_content) @string
(raw_fence_start) @punctuation.special
(raw_fence_end) @punctuation.special
(math) @string.special
(heading_marker) @punctuation.special
(heading title: (title) @title)
(strong) @emphasis.strong
(emphasis) @emphasis
(strike) @strikethrough
(list_marker) @punctuation.special
(divider) @punctuation.special
(table_header) @title
(table_delimiter) @punctuation.special
(link_target) @link_text
(link_destination) @link_uri
(call name: (path) @function.call)
(reference name: (path) @variable)
(import_declaration source: (string) @string.special.path)
["#import" "as" "let" "if" "else" "match" "comptime"] @keyword
["for" "in" "#for"] @keyword
(for_expression binding: (identifier) @variable.parameter)
(markup_for_expression binding: (identifier) @variable.parameter)
(member_expression member: (identifier) @property)
(invoke_expression function: (member_expression member: (identifier) @function.call))
(import_member name: (identifier) @variable)
(import_member alias: (identifier) @variable)
(invoke_expression function: (path) @function.call)
(let_binding name: (identifier) @variable)
(binary_expression operator: _ @operator)
(unary_expression operator: _ @operator)
(named_argument name: (_) @variable.parameter)
(dict_entry key: (_) @property)
["(" ")" "[" "]" "[[" "]]" "{" "}" "<" ">"] @punctuation.bracket
["#" "!" "@" "@!" "|" "," ":" "." ";"] @punctuation.delimiter
["*" "_" "~"] @punctuation.special
(negative_number "-" @operator)
