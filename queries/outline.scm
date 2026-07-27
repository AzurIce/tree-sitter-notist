((heading
  body: (inline_body) @name) @item)

((embedded_expression
  expression: (call_expression
    function: (qualified_name
      (identifier) @_function)
    trailing: (content_block
      body: (content_body) @name))) @item
  (#eq? @_function "heading"))
