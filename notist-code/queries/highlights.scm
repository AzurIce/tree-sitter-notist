(comment) @comment
(string) @string
(path_literal) @string.special.path
(number) @number
(boolean) @constant.builtin
(fn_keyword) @keyword
["import" "as" "let" "if" "else" "match" "comptime"] @keyword
["for" "in" "#for" "where"] @keyword
(identifier) @variable
(function_declaration name: (identifier) @function)
(parameter name: (identifier) @variable.parameter)
(type name: (path) @type)
(dict_entry key: (_) @property)
(named_argument name: (_) @variable.parameter)
(invoke_expression function: (path) @function.call)
(call name: (path) @function.call)
(reference name: (path) @variable)
(for_expression binding: (identifier) @variable.parameter)
(markup_for_expression binding: (identifier) @variable.parameter)
(member_expression member: (identifier) @property)
(invoke_expression function: (member_expression member: (identifier) @function.call))
(import_member name: (identifier) @variable)
(import_member alias: (identifier) @variable)
(binary_expression operator: _ @operator)
(unary_expression operator: _ @operator)
["(" ")" "[" "]" "<" ">" "{" "}"] @punctuation.bracket
[";" "," ":" "."] @punctuation.delimiter
["->" "=" "?" "-"] @operator
"children" @variable.builtin
(escape) @string.escape
(strong) @emphasis.strong
(emphasis) @emphasis
(strike) @strikethrough
(raw_inline) @string
(raw_content) @string
(math) @string.special
(heading title: (title) @title)
["#" "@" "@!"] @punctuation.delimiter
