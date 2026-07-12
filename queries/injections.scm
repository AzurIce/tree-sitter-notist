((raw_call
  arguments: (arguments
    (argument
      (named_argument
        name: (identifier) @_name
        value: (expression
          (string
            (string_content) @injection.language)))))
  body: (raw_content) @injection.content)
  (#eq? @_name "lang"))
