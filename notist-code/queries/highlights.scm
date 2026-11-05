(comment) @comment
(string) @string
(number) @number
(boolean) @constant.builtin
(fn_keyword) @keyword
["import" "as" "let" "if" "else" "match" "comptime"] @keyword
(identifier) @variable
(function_declaration name: (identifier) @function)
(parameter name: (identifier) @variable.parameter)
(type name: (path) @type)
(dict_entry key: (_) @property)
(named_argument name: (_) @variable.parameter)
(invoke_expression function: (path) @function.call)
(call name: (path) @function.call)
(reference name: (path) @variable)
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
